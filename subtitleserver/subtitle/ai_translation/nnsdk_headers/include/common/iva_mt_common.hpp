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

#include <string>
#include <vector>
#include <functional>

/**
 * @brief machine translation context struct
 */

 
typedef struct mt_struct *mt_context_detect_handle_t;
using StringCallback = std::function<int(const std::string&,int)>;

/**
 * @brief machine translation parameter struct
 */
struct aai_iva_mt_param_t
{
    aai_iva_mt_param_t()
        :input_path("eng.txt"),model_path("/etc/aai_nndata/"),input_translator("en2fr"),stream_enable(1){}
    
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
struct aai_iva_mt_input_t
{
    aai_iva_mt_input_t()
        :input_language("english"),target_language("french")
    {}
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
struct aai_iva_mt_output_t
{
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
typedef struct aai_iva_mt_struct
{
    /**
     * @brief machine translation context
     */
    mt_context_detect_handle_t ctx_detect;

} aai_iva_mt_struct_t, *aai_iva_mt_handle_t;



#endif