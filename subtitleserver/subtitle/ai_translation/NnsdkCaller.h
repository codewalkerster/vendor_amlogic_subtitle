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

#include <dlfcn.h>
#include <utils/Log.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include "nnsdk_headers/include/iva/iva_mt.hpp"

class NnsdkCaller {
public:
    NnsdkCaller() {
        std::string libaaisdk = "/vendor/lib/libaaisdk.so";
#ifdef __LP64__
        libaaisdk = "/vendor/lib64/libaaisdk.so";
#endif
        ALOGI("%s: start loading %s", __func__, libaaisdk.c_str());
        mHandler = dlopen(libaaisdk.data(), RTLD_LAZY);
        if (!mHandler) {
            ALOGE("%s: fail to load %s, error:%s", __func__, libaaisdk.c_str(), dlerror());
            return;
        }

        m_init_func = reinterpret_cast<decltype(m_init_func)>(dlsym(mHandler, "aai_iva_mt_init"));
        m_deinit_func = reinterpret_cast<decltype(m_deinit_func)>(dlsym(mHandler, "aai_iva_mt_deinit"));
        m_detect_func = reinterpret_cast<decltype(m_detect_func)>(dlsym(mHandler, "aai_iva_mt_detect"));

        if (!m_init_func || !m_deinit_func || !m_detect_func) {
            dlclose(mHandler);
            mHandler = nullptr;
            ALOGE("%s: fail to parse functions: %s", __func__, dlerror());
            return;
        }

        ALOGI("%s: loaded %s successfully", __func__, libaaisdk.c_str());
    }

    ~NnsdkCaller() {
        if (mHandler) {
            dlclose(mHandler);
        }
        ALOGI("%s DONE ", __func__);
    }

    aai_iva_mt_handle_t init(aai_iva_mt_param_t& params) {
        if (!mHandler) {
            ALOGE("NnsdkCaller::%s: NnsdkCaller was not ready", __func__);
            return nullptr;
        }
        return m_init_func(params);
    }

    IVA_STATUS_E deinit(aai_iva_mt_handle_t handle) {
        if (!mHandler) {
            ALOGE("NnsdkCaller::%s: NnsdkCaller was not ready", __func__);
            return IVA_STATUS_ERROR;
        }
        return m_deinit_func(handle);
    }

    IVA_STATUS_E detect(aai_iva_mt_handle_t handle, aai_iva_mt_input_t& input, aai_iva_mt_output_t& output,
                        StringCallback callback) {
        if (!mHandler) {
            ALOGE("NnsdkCaller::%s: NnsdkCaller was not ready", __func__);
            return IVA_STATUS_ERROR;
        }
        return m_detect_func(handle, input, output, callback);
    }

    NnsdkCaller(const NnsdkCaller&) = delete;
    NnsdkCaller& operator=(const NnsdkCaller&) = delete;

private:
    void* mHandler = nullptr;

    using InitFunc = decltype(&aai_iva_mt_init);
    using DeinitFunc = decltype(&aai_iva_mt_deinit);
    using DetectFunc = decltype(&aai_iva_mt_detect);

    InitFunc m_init_func = nullptr;
    DeinitFunc m_deinit_func = nullptr;
    DetectFunc m_detect_func = nullptr;
};
