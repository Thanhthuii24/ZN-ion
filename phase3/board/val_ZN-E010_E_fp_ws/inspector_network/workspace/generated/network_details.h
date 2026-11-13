/**
  ******************************************************************************
  * @file    network.h
  * @date    2026-11-13T15:36:09+0700
  * @brief   ST.AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef STAI_NETWORK_DETAILS_H
#define STAI_NETWORK_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_network_details = {
  .tensors = (const stai_tensor[53]) {
   { .size_bytes = 3600, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 3, 300}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "curve_output" },
   { .size_bytes = 3600, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 3}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "curve_Transpose_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 150, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_0_Conv_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 150, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_1_Div_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 150, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_1_Erf_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 150, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_1_Add_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 150, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_1_Mul_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 75, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_2_Conv_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 75, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_3_Div_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 75, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_3_Erf_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 75, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_3_Add_output_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 75, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_3_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_4_Conv_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_5_Div_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_5_Erf_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_5_Add_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_5_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_stem_stem_5_Mul_1_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_conv1_Conv_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 64, 38}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_Reshape_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_Reshape_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_InstanceNormalization_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 8, 304}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_Reshape_1_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_Reshape_1_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm1_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_Relu_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_conv2_Conv_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 64, 38}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_Reshape_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_Reshape_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_InstanceNormalization_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 8, 304}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_Reshape_1_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_Reshape_1_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_norm2_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_Add_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_0_Relu_1_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_conv1_Conv_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 64, 38}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_Reshape_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_Reshape_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_InstanceNormalization_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 8, 304}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_Reshape_1_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_Reshape_1_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm1_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_Relu_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_conv2_Conv_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 64, 38}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_Reshape_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_Reshape_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 8}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_InstanceNormalization_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 8, 304}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_Reshape_1_output_0_to_chlast_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_Reshape_1_output_0_to_chfirst_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_norm2_Mul_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_Add_output_0_output" },
   { .size_bytes = 9728, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 38, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_res_res_1_Relu_1_output_0_output" },
   { .size_bytes = 256, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 1, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "_pool_GlobalAveragePool_output_0_output0" }
  },
  .nodes = (const stai_node_details[52]){
    {.id = 2, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){1}} }, /* curve_Transpose */
    {.id = 8, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* _stem_stem_0_Conv_output_0 */
    {.id = 9, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* _stem_stem_1_Div_output_0 */
    {.id = 10, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* _stem_stem_1_Erf_output_0 */
    {.id = 11, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* _stem_stem_1_Add_output_0 */
    {.id = 12, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){2, 5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* _stem_stem_1_Mul_output_0 */
    {.id = 13, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* _stem_stem_2_Conv_output_0 */
    {.id = 14, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* _stem_stem_3_Div_output_0 */
    {.id = 15, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* _stem_stem_3_Erf_output_0 */
    {.id = 16, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){9}}, .output_tensors = {1, (const int32_t[1]){10}} }, /* _stem_stem_3_Add_output_0 */
    {.id = 17, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){7, 10}}, .output_tensors = {1, (const int32_t[1]){11}} }, /* _stem_stem_3_Mul_output_0 */
    {.id = 18, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){11}}, .output_tensors = {1, (const int32_t[1]){12}} }, /* _stem_stem_4_Conv_output_0 */
    {.id = 19, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){12}}, .output_tensors = {1, (const int32_t[1]){13}} }, /* _stem_stem_5_Div_output_0 */
    {.id = 20, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){13}}, .output_tensors = {1, (const int32_t[1]){14}} }, /* _stem_stem_5_Erf_output_0 */
    {.id = 21, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){14}}, .output_tensors = {1, (const int32_t[1]){15}} }, /* _stem_stem_5_Add_output_0 */
    {.id = 22, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){12, 15}}, .output_tensors = {1, (const int32_t[1]){16}} }, /* _stem_stem_5_Mul_output_0 */
    {.id = 23, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){16}}, .output_tensors = {1, (const int32_t[1]){17}} }, /* _stem_stem_5_Mul_1_output_0 */
    {.id = 24, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){17}}, .output_tensors = {1, (const int32_t[1]){18}} }, /* _res_res_0_conv1_Conv_output_0 */
    {.id = 25, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){18}}, .output_tensors = {1, (const int32_t[1]){19}} }, /* _res_res_0_norm1_Reshape_output_0_to_chlast */
    {.id = 25, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){19}}, .output_tensors = {1, (const int32_t[1]){20}} }, /* _res_res_0_norm1_Reshape_output_0_to_chfirst */
    {.id = 26, .type = AI_LAYER_INSTANCENORMALIZATION_TYPE, .input_tensors = {1, (const int32_t[1]){20}}, .output_tensors = {1, (const int32_t[1]){21}} }, /* _res_res_0_norm1_InstanceNormalization_output_0 */
    {.id = 27, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){21}}, .output_tensors = {1, (const int32_t[1]){22}} }, /* _res_res_0_norm1_Reshape_1_output_0_to_chlast */
    {.id = 27, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){22}}, .output_tensors = {1, (const int32_t[1]){23}} }, /* _res_res_0_norm1_Reshape_1_output_0_to_chfirst */
    {.id = 29, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){23}}, .output_tensors = {1, (const int32_t[1]){24}} }, /* _res_res_0_norm1_Mul_output_0 */
    {.id = 30, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){24}}, .output_tensors = {1, (const int32_t[1]){25}} }, /* _res_res_0_Relu_output_0 */
    {.id = 31, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){25}}, .output_tensors = {1, (const int32_t[1]){26}} }, /* _res_res_0_conv2_Conv_output_0 */
    {.id = 32, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){26}}, .output_tensors = {1, (const int32_t[1]){27}} }, /* _res_res_0_norm2_Reshape_output_0_to_chlast */
    {.id = 32, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){27}}, .output_tensors = {1, (const int32_t[1]){28}} }, /* _res_res_0_norm2_Reshape_output_0_to_chfirst */
    {.id = 33, .type = AI_LAYER_INSTANCENORMALIZATION_TYPE, .input_tensors = {1, (const int32_t[1]){28}}, .output_tensors = {1, (const int32_t[1]){29}} }, /* _res_res_0_norm2_InstanceNormalization_output_0 */
    {.id = 34, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){29}}, .output_tensors = {1, (const int32_t[1]){30}} }, /* _res_res_0_norm2_Reshape_1_output_0_to_chlast */
    {.id = 34, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){30}}, .output_tensors = {1, (const int32_t[1]){31}} }, /* _res_res_0_norm2_Reshape_1_output_0_to_chfirst */
    {.id = 36, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){31}}, .output_tensors = {1, (const int32_t[1]){32}} }, /* _res_res_0_norm2_Mul_output_0 */
    {.id = 37, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){32, 17}}, .output_tensors = {1, (const int32_t[1]){33}} }, /* _res_res_0_Add_output_0 */
    {.id = 38, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){33}}, .output_tensors = {1, (const int32_t[1]){34}} }, /* _res_res_0_Relu_1_output_0 */
    {.id = 39, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){34}}, .output_tensors = {1, (const int32_t[1]){35}} }, /* _res_res_1_conv1_Conv_output_0 */
    {.id = 40, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){35}}, .output_tensors = {1, (const int32_t[1]){36}} }, /* _res_res_1_norm1_Reshape_output_0_to_chlast */
    {.id = 40, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){36}}, .output_tensors = {1, (const int32_t[1]){37}} }, /* _res_res_1_norm1_Reshape_output_0_to_chfirst */
    {.id = 41, .type = AI_LAYER_INSTANCENORMALIZATION_TYPE, .input_tensors = {1, (const int32_t[1]){37}}, .output_tensors = {1, (const int32_t[1]){38}} }, /* _res_res_1_norm1_InstanceNormalization_output_0 */
    {.id = 42, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){38}}, .output_tensors = {1, (const int32_t[1]){39}} }, /* _res_res_1_norm1_Reshape_1_output_0_to_chlast */
    {.id = 42, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){39}}, .output_tensors = {1, (const int32_t[1]){40}} }, /* _res_res_1_norm1_Reshape_1_output_0_to_chfirst */
    {.id = 44, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){40}}, .output_tensors = {1, (const int32_t[1]){41}} }, /* _res_res_1_norm1_Mul_output_0 */
    {.id = 45, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){41}}, .output_tensors = {1, (const int32_t[1]){42}} }, /* _res_res_1_Relu_output_0 */
    {.id = 46, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){42}}, .output_tensors = {1, (const int32_t[1]){43}} }, /* _res_res_1_conv2_Conv_output_0 */
    {.id = 47, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){43}}, .output_tensors = {1, (const int32_t[1]){44}} }, /* _res_res_1_norm2_Reshape_output_0_to_chlast */
    {.id = 47, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){44}}, .output_tensors = {1, (const int32_t[1]){45}} }, /* _res_res_1_norm2_Reshape_output_0_to_chfirst */
    {.id = 48, .type = AI_LAYER_INSTANCENORMALIZATION_TYPE, .input_tensors = {1, (const int32_t[1]){45}}, .output_tensors = {1, (const int32_t[1]){46}} }, /* _res_res_1_norm2_InstanceNormalization_output_0 */
    {.id = 49, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){46}}, .output_tensors = {1, (const int32_t[1]){47}} }, /* _res_res_1_norm2_Reshape_1_output_0_to_chlast */
    {.id = 49, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){47}}, .output_tensors = {1, (const int32_t[1]){48}} }, /* _res_res_1_norm2_Reshape_1_output_0_to_chfirst */
    {.id = 51, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){48}}, .output_tensors = {1, (const int32_t[1]){49}} }, /* _res_res_1_norm2_Mul_output_0 */
    {.id = 52, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){49, 34}}, .output_tensors = {1, (const int32_t[1]){50}} }, /* _res_res_1_Add_output_0 */
    {.id = 53, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){50}}, .output_tensors = {1, (const int32_t[1]){51}} }, /* _res_res_1_Relu_1_output_0 */
    {.id = 54, .type = AI_LAYER_POOL_TYPE, .input_tensors = {1, (const int32_t[1]){51}}, .output_tensors = {1, (const int32_t[1]){52}} } /* _pool_GlobalAveragePool_output_0 */
  },
  .n_nodes = 52
};
#endif

