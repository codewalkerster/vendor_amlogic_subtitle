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

#define LOG_TAG "SubtitleAiTranslation"

#include "SubtitleAiTranslation.h"

#include <utils/Log.h>

#include <cinttypes>
#include <cstring>

#include "StreamUtils.h"

namespace {

static const int MSG_AI_TRANSLATION_HAS_MORE = 1000;
static const int MSG_AI_TRANSLATION_COMPLETE = 1001;

constexpr char MT_MODULE_PATH[] = "/vendor/etc/";

/*
NOTE:
These flags are used for Amlogic's APK, which is used to recognize
which words are the translated text. The start flag can be used to change
the display color of the translated text, the end flag can be used to give
a clear end for UI. The format likes:

This is original English text
>>>>>>这是翻译出来的中文或者法文<<<<<<

*/
static const std::string START_FLAG_FOR_TRANSLATED_TEXT = ">>>>>>";
static const std::string END_FLAG_FOR_TRANSLATED_TEXT = "<<<<<<";

};  // namespace

SubtitleAiTranslation::SubtitleAiTranslation(ISubtitleAiTranslationObserver& observer,
                                             const std::string& targetLanguage)
    : Mt_Handler(nullptr), mTargetLanguage(targetLanguage), mObserver(&observer) {
    SUBTITLE_LOGI("enter %s", __func__);
    mLooperMessageProcess = new LooperMessageProcess(this);

    if (targetLanguage.empty()) {
        return;
    }

    if (targetLanguage.size() != 2) {
        SUBTITLE_LOGE("%s: input language '%s' is not <ISO 3166 Country Codes>", __func__,
                      targetLanguage.c_str());
        return;
    }

    if (!isLanguageAvailable(targetLanguage)) {
        SUBTITLE_LOGE("%s: input language '%s' is supported", __func__, targetLanguage.c_str());
        return;
    }

    loadAaiLanguage(targetLanguage);
}

SubtitleAiTranslation::~SubtitleAiTranslation() {
    SUBTITLE_LOGI("%s: stop worker thread", __func__);
    resetForSeek();
    if (mWorkerThread.IsThreadRunning()) {
        mWorkerThread.Stop();
    }

    if (mLooperMessageProcess != nullptr) {
        SUBTITLE_LOGI("%s: stop looper", __func__);
        mLooperMessageProcess->Stop();
    }
    deInitAai();
    if (!mTargetLanguage.empty()) {
        SUBTITLE_LOGI("%s: thanks to Ai translation for language '%s'", __func__,
                      mTargetLanguage.c_str());
    }
}

// Public
// ===========================

bool SubtitleAiTranslation::loadAaiLanguage(const std::string& lang) {
    SUBTITLE_LOGI("%s: lang = %s, oldLang = %s", __func__, lang.empty() ? " " : lang.c_str(),
                  mTargetLanguage.empty() ? " " : mTargetLanguage.c_str());

    if (lang.empty() || (!mTargetLanguage.empty() && lang != mTargetLanguage)) {
        if (!mTargetLanguage.empty()) {
            SUBTITLE_LOGI("%s: stop translatation for '%s'", __func__, mTargetLanguage.c_str());
        }
        if (hasUncompletedJob()) {
            SUBTITLE_LOGI("%s: has uncompleted job", __func__);
            mWorkerThread.QueueJob([this] { deInitAai(); });
        } else {
            if (mWorkerThread.IsThreadRunning()) {
                mWorkerThread.Stop();
            }
            deInitAai();
        }

        // Just stop nn
        if (lang.empty()) {
            return true;
        }
    }

    if (!mWorkerThread.IsThreadRunning()) {
        SUBTITLE_LOGI("%s: start worker thread", __func__);
        mWorkerThread.Start();
    }
    mWorkerThread.QueueJob([=] { doLoadAaiLanguage(lang); });
    return true;
}

bool SubtitleAiTranslation::isLanguageAvailable(const std::string& language) {
    if (language.size() > 0 && language.size() != 2) {
        SUBTITLE_LOGE("%s: input language '%s' is not <ISO 3166 Country Codes>", __func__,
                      language.c_str());
        return false;
    }

    // TODO: the capability depends on NNSDK.
    return language == "fr" || language == "cn";
}

bool SubtitleAiTranslation::hasUncompletedJob() {
    return mWorkerThread.GetJobAmount() > 0;
}

void SubtitleAiTranslation::push2Translate(std::shared_ptr<AML_SPUVAR> item) {
    assert(!mWorkerThread.InWorkerThreadContext());

    if (!item || !item->spu_data) {
        SUBTITLE_LOGE("%s: unexpected null item or null spu data", __func__);
        return;
    }

    // TODO: only support text translation by now, doesn't support OCR yet
    if (!isTextSubtitle(item)) {
        SUBTITLE_LOGI("%s: is not text, size=%d, w%d, h%d", __func__, item->buffer_size,
                      item->spu_width, item->spu_height);
        postResult2Observer(item);
        return;
    }

    std::string origText = reinterpret_cast<const char*>(item->spu_data);
    SUBTITLE_LOGI("%s: IsThreadRunning=%d, text = %s", __func__, mWorkerThread.IsThreadRunning(),
                  origText.c_str());

    if (origText.find("cea608") != std::string::npos ||
        origText.find("cea708") != std::string::npos) {
        SUBTITLE_LOGI("%s: is cea608/cea708", __func__);
        postResult2Observer(item);
        return;
    }

    if (mWorkerThread.IsThreadRunning()) {
        std::unique_lock<std::mutex> autolock(mMutex);
        mItemQueue.push_back(item);
        mWorkerThread.QueueJob([=] { translateText(origText); });
    } else {
        assert(mItemQueue.empty());
        postResult2Observer(item);
    }
}

void SubtitleAiTranslation::resetForSeek() {
    SUBTITLE_LOGI("%s", __func__);
    std::unique_lock<std::mutex> autolock(mMutex);
    mItemQueue.clear();
    if (Mt_Handler && mIsAiTranslationReady) {
        stopTranslation = true;
    }
}

void SubtitleAiTranslation::HandleMessage(const Message& message) {
    std::unique_lock<std::mutex> autolock(mMutex);
    if (mItemQueue.size() == 0 || stopTranslation) {
        return;
    }

    auto it = mItemQueue.front();
    if (!it->spu_data) {
        SUBTITLE_LOGE("%s: something is wrong as get null data", __func__);
        return;
    }
    auto result = it;
    bool isComplete = message.what == MSG_AI_TRANSLATION_COMPLETE;
    if (isComplete) {
        mItemQueue.pop_front();
    }

    postResult2Observer(result);
}

// Private
// ===========================
bool SubtitleAiTranslation::isTextSubtitle(const std::shared_ptr<AML_SPUVAR>& item) {
    assert(!mWorkerThread.InWorkerThreadContext());
    return (item->spu_data && item->buffer_size > 0 && item->spu_width == 0 &&
            item->spu_height == 0);
}

void SubtitleAiTranslation::translateText(const std::string& originalText) {
    assert(mWorkerThread.InWorkerThreadContext());

    if (!Mt_Handler || !mIsAiTranslationReady) {
        // Return original text as AI doesn't work
        SUBTITLE_LOGI("%s: aai is not ready", __func__);
        reportResult(originalText);
        return;
    }

    if (originalText.size() == 0) {
        SUBTITLE_LOGI("%s: empty string", __func__);
        reportResult(originalText);
        return;
    }

    mBeginTime = getCurrentTimeMs();

    size_t pos = originalText.find("\n" + START_FLAG_FOR_TRANSLATED_TEXT);
    if (pos != std::string::npos) {
        // As the memcpy in reportResult(), the original text is changed, which causes problems
        // in seek playback cases, so need delete the translated text from the original text
        // before starting the translation.
        Mt_InputString.text_input = originalText.substr(0, pos);
    } else {
        Mt_InputString.text_input = originalText;
    }

    SUBTITLE_LOGI("%s(language=%s): aai_iva_mt_detect start: =====> %s", __func__,
                  mTargetLanguage.c_str(), Mt_InputString.text_input.c_str());

    stopTranslation = false;
    auto ret = mNnsdkCaller.detect(Mt_Handler, Mt_InputString, Mt_OutString,
                                   [&](const std::string& translatedText, int complete) {
                                       return SubtitleAiTranslation::onAaiCallback(translatedText,
                                                                                   complete);
                                   });
    if (ret != IVA_STATUS_OK) {
        if (Mt_OutString.text_output.empty()) {
            SUBTITLE_LOGE("%s: ret=%d, failed translate '%s' with language '%s'", __func__,
                          static_cast<int>(ret), originalText.c_str(), mTargetLanguage.c_str());
        }
        reportResult(originalText);
        return;
    }

    if (!mTargetLanguage.empty() && !Mt_OutString.text_output.empty()) {
        SUBTITLE_LOGI("%s(language=%s): %s ----> %s", __func__, mTargetLanguage.c_str(),
                      originalText.c_str(), Mt_OutString.text_output.c_str());
    }
    dumpSpendTime("aai_iva_mt_detect ====> COMPLETED");
    if (!Mt_Params.stream_enable) {
        reportResult(originalText, Mt_OutString.text_output, true);
    }
}

int SubtitleAiTranslation::onAaiCallback(const std::string& translatedText, int complete) {
    assert(mWorkerThread.InWorkerThreadContext());
    SUBTITLE_LOGI("%s: (%s) ----> %s", __func__, complete == 0 ? "hasMore" : "complete",
                  translatedText.c_str());
    dumpSpendTime("aai_iva_mt_detect");

    if (stopTranslation) {
        return -1;
    }

    reportResult(Mt_InputString.text_input, translatedText, complete);
    return 0;
}

void SubtitleAiTranslation::reportResult(const std::string& origText,
                                         const std::string& translatedText, bool complete) {
    std::unique_lock<std::mutex> autolock(mMutex);
    std::string origAndTranslatedText = origText;
    if (translatedText.size() > 0) {
        origAndTranslatedText = origText + "\n" + START_FLAG_FOR_TRANSLATED_TEXT + translatedText;

        if (complete) {
            origAndTranslatedText += END_FLAG_FOR_TRANSLATED_TEXT;
        }
    }

    if (mItemQueue.size() == 0) {
        SUBTITLE_LOGE("%s: unexpected 0 queue size, origAndTranslatedText = %s", __func__,
                      origAndTranslatedText.c_str());
        return;
    }

    auto it = mItemQueue.front();
    if (it->useMalloc) {
        free(it->spu_data);
    } else {
        delete[] it->spu_data;
    }
    it->spu_data = nullptr;

    it->buffer_size = origAndTranslatedText.size();
    it->useMalloc = true;
    it->spu_data = (uint8_t*)malloc(it->buffer_size);
    if (!it->spu_data) {
        perror("malloc error:");
    } else {
        std::memcpy(it->spu_data, origAndTranslatedText.data(), it->buffer_size);
    }

    mLooperMessageProcess->SendMessage(complete ? MSG_AI_TRANSLATION_COMPLETE
                                                : MSG_AI_TRANSLATION_HAS_MORE);
}

void SubtitleAiTranslation::postResult2Observer(std::shared_ptr<AML_SPUVAR> item) {
    assert(!mWorkerThread.InWorkerThreadContext());
    assert(mObserver);

    mObserver->onReceiveTranslatedText(item);
}

void SubtitleAiTranslation::doLoadAaiLanguage(const std::string& lang) {
    assert(mWorkerThread.InWorkerThreadContext());
    deInitAai();
    if (!isLanguageAvailable(lang)) {
        SUBTITLE_LOGI("%s: doesn't support %s", __func__, lang.c_str());
        mObserver->onError("unsupport language " + lang);
        return;
    }

    memset(&Mt_Params, 0, sizeof(Mt_Params));
    Mt_Params.model_path = MT_MODULE_PATH;
    Mt_Params.input_translator = "en2" + lang;
    Mt_Params.stream_enable = 1;

    // NOTE: nn requests these values should be assigned
    // NOTE: Only support french by now
    Mt_InputString.input_language = "english";
    Mt_InputString.target_language = lang == "fr" ? "french" : "chinese";

    SUBTITLE_LOGI("%s: start loading language '%s'", __func__, Mt_Params.input_translator.c_str());
    mBeginTime = getCurrentTimeMs();
    Mt_Handler = mNnsdkCaller.init(Mt_Params);
    dumpSpendTime("aai_iva_mt_init");

    if (!Mt_Handler) {
        SUBTITLE_LOGE("%s: failed to init AAI for language '%s'", __func__, lang.c_str());
        mObserver->onError("fail to aai_iva_mt_init: " + lang);
        return;
    }

    mTargetLanguage = lang;
    mIsAiTranslationReady = true;
    SUBTITLE_LOGI("%s: language '%s' is OK", __func__, lang.c_str());
}

void SubtitleAiTranslation::deInitAai() {
    SUBTITLE_LOGI("%s: current_lang = %s", __func__, mTargetLanguage.c_str());

    if (Mt_Handler && mIsAiTranslationReady) {
        SUBTITLE_LOGI("%s: aai_iva_mt_deinit", __func__);
        mNnsdkCaller.deinit(Mt_Handler);
    }

    mTargetLanguage.clear();
    Mt_Handler = nullptr;
    mIsAiTranslationReady = false;
}

void SubtitleAiTranslation::dumpSpendTime(const std::string& text) {
    const auto now = getCurrentTimeMs();
    const int writeTime = now - mBeginTime;
    SUBTITLE_LOGI("%s spent %d ms", text.c_str(), writeTime);
};
