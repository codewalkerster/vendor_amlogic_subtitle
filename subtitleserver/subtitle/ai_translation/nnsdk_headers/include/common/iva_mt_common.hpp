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

/**
 * @file iva_mt_common.hpp
 * @brief NN Machine Translation Structures
 * @version 1.0.0
 * @date 2024-08-08
 *
 * @copyright
 * @copydetails
 *   The confidential and proprietary information contained in this file may
 *   only be used by a person authorized under and to the extent permitted
 *   by a subsisting licensing agreement from the owner.
 *
 *   This entire notice must be reproduced on all copies of this file
 *   and copies of this file may only be made by a person if such person is
 *   permitted to do so under the terms of a subsisting license agreement
 *   from the owner.
 */

#ifndef AAI_IVA_MT_ALL_HCS_COMMON_H
#define AAI_IVA_MT_ALL_HCS_COMMON_H

#include <functional>
#include <string>
#include <vector>

/**
 * @brief machine translation context struct
 */

typedef struct mt_struct *mt_context_detect_handle_t;
using StringCallback = std::function<int(const std::string &, int)>;

/**
 * @brief machine translation parameter struct
 */
struct aai_iva_mt_param_t {
    aai_iva_mt_param_t()
        : input_path("eng.txt"),
          model_path("/etc/aai_nndata/"),
          input_translator("en2fr"),
          stream_enable(1) {}

    /**
     * @brief input file
     */
    std::string input_path;

    /**
     * @brief The path of NN model files
     */
    std::string model_path;

    std::string input_translator;

    /**
     * @brief To control whether using streaming output
     * Using streaming output:         1
     * Without using streaming output: 0
     */
    int stream_enable;
};

/**
 * @brief machine translation input struct
 */
struct aai_iva_mt_input_t {
    aai_iva_mt_input_t() : input_language("english"), target_language("french") {}
    /**
     * @brief input text
     */

    std::string text_input;

    std::string input_language;

    std::string target_language;
};

/**
 * @brief machine translation output struct
 */
struct aai_iva_mt_output_t {
    /**
     * @brief output text
     */
    std::string text_output;

    /**
     * @brief Whether the output is complete
     * end:1
     */
    int end;
};

/**
 * @brief machine translation handle struct
 */
typedef struct aai_iva_mt_struct {
    /**
     * @brief machine translation context
     */
    mt_context_detect_handle_t ctx_detect;

} aai_iva_mt_struct_t, *aai_iva_mt_handle_t;

#endif