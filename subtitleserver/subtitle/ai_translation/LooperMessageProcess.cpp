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

#define LOG_TAG "LooperMessageProcess"

#include "LooperMessageProcess.h"

#include <utils/Log.h>

LooperMessageProcess::LooperMessageProcess(IHandler* messageHandler)
    : mStopped(false), mMessageHandler(messageHandler) {
    mLooperThread = std::thread(&LooperMessageProcess::looperLoop, this);
}

LooperMessageProcess::~LooperMessageProcess() {
    ALOGI("%s getStrongCount = %d\n", __func__, getStrongCount());
}

void LooperMessageProcess::Stop() {
    ALOGI("%s\n", __func__);
    mStopped = true;
    if (mLooper != nullptr) {
        mLooper->removeMessages(this);
        mLooper->wake();
    }
    if (mLooperThread.joinable()) {
        mLooperThread.join();
    }
    mLooper = nullptr;
    ALOGI("%s: DONE\n", __func__);
}

void LooperMessageProcess::SendMessage(int what, int delayInMs) {
    if (delayInMs == 0) {
        mLooper->sendMessage(this, Message(what));
        return;
    }
    mLooper->sendMessageDelayed(ms2ns(delayInMs), this, Message(what));
}

void LooperMessageProcess::handleMessage(const Message& message) {
    android::AutoMutex _l(mLock);
    // TODO: why must remove the messages? doesn't the pullInner remove them?
    mLooper->removeMessages(this, message.what);
    // TODO: why must send a delayed message? why the handler is de-constructed if all messages are handled.
    SendMessage(message.what, 10 * 1000);

    mMessageHandler->HandleMessage(message);
}

void LooperMessageProcess::looperLoop() {
    ALOGI("%s: START\n", __func__);
    mLooper = new Looper(true);
    while (!mStopped) {
        int32_t ret = mLooper->pollAll(-1);
        switch (ret) {
            case -1:
                ALOGI("A_LOOPER_POLL_WAKE\n");
                break;
            case -3:  // timeout
                ALOGI("A_LOOPER_POLL_TIMEDOUT\n");
                break;
            default:
                ALOGI("default ret=%d", ret);
                break;
        }
    }
    ALOGI("%s: STOPPED\n", __func__);
}
