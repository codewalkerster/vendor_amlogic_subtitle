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
#include "iva.hpp"
#include "../common/iva_mt_common.hpp"

// using StringCallback = std::function<void(const std::string&)>;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief NN machine translation module initialize function
     *
     * @param t_mt_param structure of machine translation detection parameters
     * @return aai_iva_mt_handle_t NN machine translation module global handle
     */
    __attribute ((visibility("default"))) aai_iva_mt_handle_t aai_iva_mt_init(aai_iva_mt_param_t &t_mt_param);

    /**
     * @brief NN machine translation module release function
     *
     * @param iva_mt_handle global machine translation handle
     * @return IVA_STATUS_E execution status: success, failure, or other
     */
    __attribute ((visibility("default"))) IVA_STATUS_E aai_iva_mt_deinit(aai_iva_mt_handle_t iva_mt_handle);

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
    __attribute ((visibility("default"))) IVA_STATUS_E aai_iva_mt_detect(aai_iva_mt_handle_t iva_mt_handle,
                                    aai_iva_mt_input_t &t_mt_in, 
                                    aai_iva_mt_output_t &t_mt_out,
                                    StringCallback callback);





#ifdef __cplusplus
}
#endif
#endif