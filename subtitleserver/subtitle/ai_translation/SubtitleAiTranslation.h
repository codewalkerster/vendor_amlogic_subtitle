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

#pragma once

#include <utils/Mutex.h>

#include "LooperMessageProcess.h"
#include "NnsdkCaller.h"
#include "SubtitleTypes.h"
#include "SubtitleWorkerThread.h"

using android::Message;
class SubtitleAiTranslation : public LooperMessageProcess::IHandler {
public:
    class ISubtitleAiTranslationObserver {
    public:
        ISubtitleAiTranslationObserver() = default;
        virtual ~ISubtitleAiTranslationObserver() = default;

        virtual void onReceiveTranslatedText(const std::shared_ptr<AML_SPUVAR>& item) = 0;
        virtual void onError(const std::string& reason) = 0;
    };

public:
    SubtitleAiTranslation(ISubtitleAiTranslationObserver& observer,
                          const std::string& targetLanguage = "");
    ~SubtitleAiTranslation();

    // language: the two bytes country code in <ISO 3166 Country Codes>
    bool loadAaiLanguage(const std::string& lang);
    bool isLanguageAvailable(const std::string& language);
    bool hasUncompletedJob();
    void push2Translate(std::shared_ptr<AML_SPUVAR> item);
    void resetForSeek();

    // LooperMessageProcess::IHandler
    void HandleMessage(const Message& message);

private:
    NnsdkCaller mNnsdkCaller;
    aai_iva_mt_handle_t Mt_Handler = nullptr;
    aai_iva_mt_param_t Mt_Params;
    aai_iva_mt_input_t Mt_InputString;
    aai_iva_mt_output_t Mt_OutString;

    std::string mTargetLanguage;
    bool mIsAiTranslationReady = false;
    uint64_t mBeginTime = 0;
    ISubtitleAiTranslationObserver* mObserver;

    std::list<std::shared_ptr<AML_SPUVAR>> mItemQueue;

    // The worker thread for translation
    SubtitleWorkerThread mWorkerThread;

    // Process the message from worker thread to looper
    std::mutex mMutex;
    // Must use sp to manager refBase
    sp<LooperMessageProcess> mLooperMessageProcess;

    bool mStopTranslation = false;

    bool isTextSubtitle(const std::shared_ptr<AML_SPUVAR>& item);
    void translateText(const std::string& originalText);
    int onAaiCallback(const std::string& translatedText, int complete);
    void reportResult(const std::string& origText, const std::string& translatedText = "",
                      bool complete = true);
    void postResult2Observer(std::shared_ptr<AML_SPUVAR> item);

    void doLoadAaiLanguage(const std::string& lang);
    void deInitAai();

    void dumpSpendTime(const std::string& text = "anon");
};
