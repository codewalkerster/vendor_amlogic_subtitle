/*
 * Copyright (C) 2025 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SubtitleWorkerThread.h"

#include <cassert>
#include <functional>

class TInternalWorkerThreadJob : public SubtitleWorkerThread::IJob {
public:
    explicit TInternalWorkerThreadJob(const std::function<void()>& job) : Job(job) {}
    void Execute() { Job(); }

private:
    std::function<void()> Job;
};

SubtitleWorkerThread::SubtitleWorkerThread() {
    // Empty
}

SubtitleWorkerThread::~SubtitleWorkerThread() {
    Stop();
}

void SubtitleWorkerThread::Start() {
    if (mRunThread) {
        return;
    }
    mRunThread = true;
    mThread = std::thread(&SubtitleWorkerThread::run, this);
}

void SubtitleWorkerThread::Stop() {
    if (mRunThread) {
        QueueJob([this]() { mRunThread = false; });
        mThread.join();
    }
    ClearJobQueue();
}

void SubtitleWorkerThread::QueueJob(const std::function<void()>& job) {
    if (!mRunThread) {
        return;
    }
    queueJob(new TInternalWorkerThreadJob(job));
}

void SubtitleWorkerThread::WaitForCompletion() {
    std::unique_lock<std::mutex> autolock(mJobQueueMutex);
    while ((!mJobQueue.empty() || mProcessingJob) && mRunThread) {
        mEmptyQueueCondition.wait(autolock);
    }
}

void SubtitleWorkerThread::ClearJobQueue(bool waitForCurrentJob) {
    // Step 1: clear the job queue
    std::list<IJob*> pendingJobs;
    {
        std::unique_lock<std::mutex> autolock(mJobQueueMutex);
        pendingJobs.swap(mJobQueue);
    }

    // Step 2: optionally wait for the current job to finish
    if (waitForCurrentJob) {
        WaitForCompletion();
    }

    // Step 3: delete all pending jobs
    while (!pendingJobs.empty()) {
        delete pendingJobs.front();
        pendingJobs.pop_front();
    }

    mJobQueue.clear();
}

int SubtitleWorkerThread::GetJobAmount() {
    std::unique_lock<std::mutex> autolock(mJobQueueMutex);
    return mJobQueue.size() + (mProcessingJob ? 1 : 0);
}

void SubtitleWorkerThread::run() {
    std::unique_lock<std::mutex> autolock(mJobQueueMutex);

    while (mRunThread) {
        while (mJobQueue.empty()) {
            mJobQueuedCondition.wait(autolock);
        }

        auto job = mJobQueue.front();
        mJobQueue.pop_front();
        mProcessingJob = true;
        autolock.unlock();

        auto internalJob = static_cast<TInternalWorkerThreadJob*>(job);
        assert(internalJob);
        internalJob->Execute();
        delete job;
        autolock.lock();
        mProcessingJob = false;

        // Notify threads waiting for completion that all jobs are done.
        if (mJobQueue.empty()) {
            mEmptyQueueCondition.notify_all();
        }
    }

    // Notify threads waiting for completion that the thread has stopped.
    mEmptyQueueCondition.notify_all();
}

void SubtitleWorkerThread::queueJob(IJob* job) {
    if (!mRunThread) {
        return;
    }
    std::unique_lock<std::mutex> autolock(mJobQueueMutex);
    mJobQueue.push_back(job);
    mJobQueuedCondition.notify_one();
}
