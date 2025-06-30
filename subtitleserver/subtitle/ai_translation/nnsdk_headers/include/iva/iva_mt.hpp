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

/*
 *   The confidential and proprietary information contained in this file may
 *   only be used by a person authorized under and to the extent permitted
 *   by a subsisting licensing agreement from the owner.
 *
 *   This entire notice must be reproduced on all copies of this file
 *   and copies of this file may only be made by a person if such person is
 *   permitted to do so under the terms of a subsisting license agreement
 *   from the owner.
 */

#ifndef AAI_IVA_MTALL_H
#define AAI_IVA_MTALL_H

#include <vector>

#include "../common/iva_mt_common.hpp"
#include "iva.hpp"

// using StringCallback = std::function<void(const std::string&)>;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NN machine translation module initialize function
 *
 * @param t_mt_param structure of machine translation detection parameters
 * @return aai_iva_mt_handle_t NN machine translation module global handle
 */
__attribute((visibility("default"))) aai_iva_mt_handle_t
aai_iva_mt_init(aai_iva_mt_param_t &t_mt_param);

/**
 * @brief NN machine translation module release function
 *
 * @param iva_mt_handle global machine translation handle
 * @return IVA_STATUS_E execution status: success, failure, or other
 */
__attribute((visibility("default"))) IVA_STATUS_E
aai_iva_mt_deinit(aai_iva_mt_handle_t iva_mt_handle);

/**
 * @brief detect functions for machine translation module
 *
 * @param iva_mt_handle NN machine translation module global handle
 * @param t_mt_in structure of machine translation detection input
 * @param t_mt_out structure of machine translation detection output
 * @return IVA_STATUS_E execution status: success, failure, or other
 * @details
 * long sentence would be cut to short sentences.
 */
__attribute((visibility("default"))) IVA_STATUS_E
aai_iva_mt_detect(aai_iva_mt_handle_t iva_mt_handle, aai_iva_mt_input_t &t_mt_in,
                  aai_iva_mt_output_t &t_mt_out, StringCallback callback);

#ifdef __cplusplus
}
#endif
#endif