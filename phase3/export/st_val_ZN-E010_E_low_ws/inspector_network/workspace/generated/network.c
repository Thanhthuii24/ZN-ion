/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T14:27:46+0700
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
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

#include "ai_lite_inspect.h"
#include "ai_platform_interface.h"
#include "layers.h"
#include "core_convert.h"
#include "network.h"
#include "network_details.h"
#include "stai_events.h"

#include "ai_lite_inspect.h"

#include "lite_operators.h"
/*****************************************************************************/
#define STAI_INTERNAL_API_MAJOR               (1)
#define STAI_INTERNAL_API_MINOR               (0)
#define STAI_INTERNAL_API_MICRO               (0)

#define STAI_MAGIC                            (0xB1C00100)

/*****************************************************************************/
#define _STAI_CONCAT_ARG(a, b)     a ## b
#define STAI_CONCAT(a, b)         _STAI_CONCAT_ARG(a, b)

/*!  STAI_CAST SECTION                       *********************************/
#define STAI_CAST(type, expr) \
  ((type)(expr))


/*****************************************************************************/
#define STAI_SIZE(_size) \
  ((stai_size)(_size))

/*****************************************************************************/
#define STAI_INIT_BUFFER(_flags, _size, _address) \
  { \
    .size = (_size), \
    .address = (uintptr_t)(_address), \
    .flags = (_flags), \
  }

#define STAI_INIT_TENSOR(_name, _flags, _fmt, _size_bytes, _shape, _scale, _zeropoint) \
  { \
    .size_bytes = (_size_bytes), \
    .flags = (_flags), \
    .format = (stai_format)(_fmt), \
    .shape = STAI_PACK(_shape), \
    .scale = STAI_PACK(_scale), \
    .zeropoint = STAI_PACK(_zeropoint), \
    .name = (_name) \
  }

#define STAI_INIT_ARRAY(_size, _ptr) \
  { .size = STAI_SIZE(_size), .data = STAI_PACK(_ptr) }


#define STAI_CAST_ARRAY(_type, _size, _ptr) \
  { .size = STAI_SIZE(_size), .data = (_type)STAI_PACK(_ptr) }


#define STAI_DECLARE_ARRAY(_type, _size, ...) \
  { .size = STAI_SIZE(_size), .data = (_type[_size]) { STAI_PACK(__VA_ARGS__) } }


#define STAI_EMPTY_ARRAY() \
  { .size = 0, .data = NULL }


#define STAI_INIT_VERSION(_major, _minor, _micro) \
  { .major = (_major), .minor = (_minor), .micro = (_micro), .reserved = 0x0 }

/*****************************************************************************/
/**  Getters and setters  **/

#define STAI_GET_ARRAY_SIZE(nd_array) \
  (nd_array.size)


#define STAI_GET_ARRAY_ELEM(nd_array, pos) \
  (nd_array.data[(pos)])

#define _STAI_SET_ERROR(net_ctx, cond, value, exit) { \
  if (!(net_ctx)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE; } \
  if (((uintptr_t)net_ctx) & (_STAI_CONTEXT_ALIGNMENT-1)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_ALIGNMENT; } \
  if (((value) >= STAI_ERROR_GENERIC) && (cond)) { \
    if ((net_ctx)->_return_code == STAI_SUCCESS) { \
      (net_ctx)->_return_code = (value); \
    } \
    return (exit); \
  } \
}

/*****************************************************************************/
/* TODO REMOVE THESE TWO MACROS */
#define STAI_EVENT_NODE_START_CB
#define STAI_EVENT_NODE_STOP_CB

#ifdef STAI_EVENT_NODE_START_CB
#ifndef _STAI_NETWORK_EVENT_NODE_START_CB
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _start_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(const stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_START, (const void*)&_start_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_NETWORK_EVENT_NODE_STOP_CB
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _stop_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_STOP, (const void*)&_stop_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x10f4febe4f5130154dccbbbee7ee1575"
#define _STAI_NETWORK_DATETIME            "2026-11-13T14:27:46+0700"
#define _STAI_NETWORK_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_NETWORK_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_network_activations_1     (NULL)




#if defined(HAVE_NETWORK_INFO)
/*****************************************************************************/
static const stai_network_info g_network_info = {
  .model_signature = _STAI_NETWORK_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_NETWORK_COMPILE_DATETIME,
  .c_model_name = STAI_NETWORK_MODEL_NAME,
  .c_model_datetime = _STAI_NETWORK_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 1, 0),
  .tool_version = STAI_INIT_VERSION(4, 1, 0),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_NETWORK_MACC_NUM,
  .n_nodes = STAI_NETWORK_NODES_NUM,
  .flags = STAI_NETWORK_FLAGS,
  .n_inputs = STAI_NETWORK_IN_NUM,
  .n_outputs = STAI_NETWORK_OUT_NUM,
  .n_activations = STAI_NETWORK_ACTIVATIONS_NUM,
  .n_weights = STAI_NETWORK_WEIGHTS_NUM,
  .n_states = STAI_NETWORK_STATES_NUM,
  .inputs = (stai_tensor[STAI_NETWORK_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_1_NAME,
      STAI_NETWORK_IN_1_FLAGS,
      STAI_NETWORK_IN_1_FORMAT,
      STAI_NETWORK_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 3, 300),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 64, 1),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 39040),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 294468),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_network_context* _net_ctx = (_stai_network_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_network_check(_stai_network_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_NETWORK_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_NETWORK_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_network_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_network_context* net_ctx = (_stai_network_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_network_context))

  _STAI_SET_ERROR(net_ctx, STAI_NETWORK_CONTEXT_SIZE != sizeof(_stai_network_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_network_context _network_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_NETWORK_MODEL_SIGNATURE,
      ._flags = STAI_NETWORK_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_network_activations_1
      },
      ._weights = {
      NULL
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _network_context;

    _stai_network_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_NETWORK_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/





/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  curve_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 900, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  curve_Transpose_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 900, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_0_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_1_Constant_output_0_3D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_3_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 4800, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_4_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_5_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_conv1_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 8, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 8, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_conv2_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_5_Mul_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_conv1_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_conv2_Conv_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_Relu_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_Relu_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_output0_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 64, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  curve_Transpose_output, AI_STATIC,
  97, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 300), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &curve_Transpose_output_array, NULL)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  curve_output, AI_STATIC,
  98, 0x0,
  AI_SHAPE_INIT(4, 1, 300, 1, 3), AI_STRIDE_INIT(4, 4, 4, 1200, 1200),
  1, &curve_output_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_0_Conv_output_0_output, AI_STATIC,
  66, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_0_Conv_output_0_output_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Constant_output_0_3D, AI_STATIC,
  72, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_stem_stem_1_Constant_output_0_3D_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_output, AI_STATIC,
  73, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Div_output_0_output_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Add_output_0_output, AI_STATIC,
  70, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Add_output_0_output_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_output, AI_STATIC,
  75, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Mul_output_0_output_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_2_Conv_output_0_output, AI_STATIC,
  77, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_2_Conv_output_0_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_output, AI_STATIC,
  83, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Div_output_0_output_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Add_output_0_output, AI_STATIC,
  81, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Add_output_0_output_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_output, AI_STATIC,
  85, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Mul_output_0_output_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_4_Conv_output_0_output, AI_STATIC,
  87, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_4_Conv_output_0_output_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_output, AI_STATIC,
  91, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Div_output_0_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Add_output_0_output, AI_STATIC,
  90, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Add_output_0_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_output, AI_STATIC,
  96, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Mul_output_0_output_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_conv1_Conv_output_0_output, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_conv1_Conv_output_0_output_array, NULL)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output, AI_STATIC,
  22, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output, AI_STATIC,
  21, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array, NULL)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output0, AI_STATIC,
  23, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_bias, AI_STATIC,
  12, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_bias_array, NULL)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_output, AI_STATIC,
  13, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_scale, AI_STATIC,
  14, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_scale_array, NULL)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  19, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  18, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  20, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_conv2_Conv_output_0_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_conv2_Conv_output_0_output_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output, AI_STATIC,
  32, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output, AI_STATIC,
  31, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array, NULL)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output0, AI_STATIC,
  33, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm2_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  29, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  28, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  30, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_Add_output_0_output, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_Add_output_0_output_array, NULL)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Mul_output_0_output, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm2_Mul_output_0_output_array, NULL)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Mul_1_output_0_output, AI_STATIC,
  94, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Mul_1_output_0_output_array, NULL)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_conv1_Conv_output_0_output, AI_STATIC,
  38, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_conv1_Conv_output_0_output_array, NULL)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output, AI_STATIC,
  53, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output, AI_STATIC,
  52, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array, NULL)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output0, AI_STATIC,
  54, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_output, AI_STATIC,
  45, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm1_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  50, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  49, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  51, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_conv2_Conv_output_0_output, AI_STATIC,
  42, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_conv2_Conv_output_0_output_array, NULL)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output, AI_STATIC,
  63, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output, AI_STATIC,
  62, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array, NULL)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output0, AI_STATIC,
  64, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array, NULL)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_output, AI_STATIC,
  55, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm2_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  60, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  59, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  61, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_Relu_1_output_0_output, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_Relu_1_output_0_output_array, NULL)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_Add_output_0_output, AI_STATIC,
  34, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_Add_output_0_output_array, NULL)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Mul_output_0_output, AI_STATIC,
  57, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm2_Mul_output_0_output_array, NULL)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_output0, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_pool_GlobalAveragePool_output_0_output0_array, NULL)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_Relu_1_output_0_output, AI_STATIC,
  35, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_Relu_1_output_0_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  curve_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&curve_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&curve_Transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  curve_Transpose_layer, 2,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &curve_Transpose_chain,
  NULL, &curve_Transpose_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_0_Conv_output_0_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_1_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_layer, 9,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_1_Div_output_0_chain,
  NULL, &_stem_stem_1_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_0_Conv_output_0_output, (ai_tensor *)&_stem_stem_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_1_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_layer, 12,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_1_Mul_output_0_chain,
  NULL, &_stem_stem_1_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_2_Conv_output_0_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_3_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_layer, 14,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_3_Div_output_0_chain,
  NULL, &_stem_stem_3_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_2_Conv_output_0_output, (ai_tensor *)&_stem_stem_3_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_3_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_layer, 17,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_3_Mul_output_0_chain,
  NULL, &_stem_stem_3_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_4_Conv_output_0_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_5_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_layer, 19,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_5_Div_output_0_chain,
  NULL, &_stem_stem_5_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_4_Conv_output_0_output, (ai_tensor *)&_stem_stem_5_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_5_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_layer, 22,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_5_Mul_output_0_chain,
  NULL, &_stem_stem_5_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_conv1_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_layer, 25,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_output_0_to_chlast_chain,
  NULL, &_res_res_0_norm1_Reshape_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_layer, 25,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm1_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_layer, 26,
  INSTANCENORMALIZATION_TYPE, 0x0, NULL,
  instanceNormalization, forward_instanceNormalization,
  &_res_res_0_norm1_InstanceNormalization_output_0_chain,
  NULL, &_res_res_0_norm1_InstanceNormalization_output_0_layer, AI_STATIC, 
  .eps = 9.999999747378752e-06, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_1_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_layer, 27,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_1_output_0_to_chlast_chain,
  NULL, &_res_res_0_norm1_Reshape_1_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_1_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer, 27,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_conv2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_layer, 32,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm2_Reshape_output_0_to_chlast_chain,
  NULL, &_res_res_0_norm2_Reshape_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_layer, 32,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm2_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm2_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_layer, 33,
  INSTANCENORMALIZATION_TYPE, 0x0, NULL,
  instanceNormalization, forward_instanceNormalization,
  &_res_res_0_norm2_InstanceNormalization_output_0_chain,
  NULL, &_res_res_0_norm2_InstanceNormalization_output_0_layer, AI_STATIC, 
  .eps = 9.999999747378752e-06, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_1_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_layer, 34,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm2_Reshape_1_output_0_to_chlast_chain,
  NULL, &_res_res_0_norm2_Reshape_1_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_1_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_layer, 34,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm2_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm2_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm2_Mul_output_0_output, (ai_tensor *)&_stem_stem_5_Mul_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_Add_output_0_layer, 37,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_res_res_0_Add_output_0_chain,
  NULL, &_res_res_0_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_conv1_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_layer, 40,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_output_0_to_chlast_chain,
  NULL, &_res_res_1_norm1_Reshape_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_layer, 40,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm1_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_layer, 41,
  INSTANCENORMALIZATION_TYPE, 0x0, NULL,
  instanceNormalization, forward_instanceNormalization,
  &_res_res_1_norm1_InstanceNormalization_output_0_chain,
  NULL, &_res_res_1_norm1_InstanceNormalization_output_0_layer, AI_STATIC, 
  .eps = 9.999999747378752e-06, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_1_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_layer, 42,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_1_output_0_to_chlast_chain,
  NULL, &_res_res_1_norm1_Reshape_1_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_1_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer, 42,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_conv2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_layer, 47,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_output_0_to_chlast_chain,
  NULL, &_res_res_1_norm2_Reshape_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_layer, 47,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm2_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_layer, 48,
  INSTANCENORMALIZATION_TYPE, 0x0, NULL,
  instanceNormalization, forward_instanceNormalization,
  &_res_res_1_norm2_InstanceNormalization_output_0_chain,
  NULL, &_res_res_1_norm2_InstanceNormalization_output_0_layer, AI_STATIC, 
  .eps = 9.999999747378752e-06, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_1_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_layer, 49,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_1_output_0_to_chlast_chain,
  NULL, &_res_res_1_norm2_Reshape_1_output_0_to_chlast_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_1_output_0_to_chfirst_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer, 49,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_1_norm2_Mul_output_0_output, (ai_tensor *)&_res_res_0_Relu_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_Add_output_0_layer, 52,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_res_res_1_Add_output_0_chain,
  NULL, &_res_res_1_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_Relu_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_pool_GlobalAveragePool_output_0_output0),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_layer, 54,
  POOL_TYPE, 0x0, NULL,
  pool, forward_ap,
  &_pool_GlobalAveragePool_output_0_chain,
  NULL, &_pool_GlobalAveragePool_output_0_layer, AI_STATIC, 
  .pool_size = AI_SHAPE_2D_INIT(1, 38), 
  .pool_stride = AI_SHAPE_2D_INIT(1, 38), 
  .pool_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
)
/**  Hybrid layers declarations section  *************************************/

LITE_FORCE_INLINE void forward_lite_transpose_curve_Transpose_network(_stai_network_context* net_ctx)
{
  curve_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  curve_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  curve_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 19840);
  curve_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19840);
  _STAI_NETWORK_EVENT_NODE_START_CB(2, 1, { curve_output.data->data});
  forward_transpose((ai_layer *)&curve_Transpose_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(2, 1, { curve_Transpose_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_1_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_0_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 640);
  _stem_stem_0_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 640);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19840);
  _stem_stem_1_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19840);
  _STAI_NETWORK_EVENT_NODE_START_CB(9, 2, { _stem_stem_0_Conv_output_0_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_1_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(9, 1, { _stem_stem_1_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_1_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_0_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 640);
  _stem_stem_0_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 640);
  _stem_stem_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19840);
  _stem_stem_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19840);
  _stem_stem_1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19840);
  _stem_stem_1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19840);
  _STAI_NETWORK_EVENT_NODE_START_CB(12, 2, { _stem_stem_0_Conv_output_0_output.data->data,_stem_stem_1_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_1_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(12, 1, { _stem_stem_1_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_3_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_3_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_3_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 2, { _stem_stem_2_Conv_output_0_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_3_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, { _stem_stem_3_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_3_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_3_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_3_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_3_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_3_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 2, { _stem_stem_2_Conv_output_0_output.data->data,_stem_stem_3_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_3_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, { _stem_stem_3_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_5_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_4_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19968);
  _stem_stem_4_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19968);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_5_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 2, { _stem_stem_4_Conv_output_0_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_5_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, { _stem_stem_5_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_5_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_4_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19968);
  _stem_stem_4_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19968);
  _stem_stem_5_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _stem_stem_5_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(22, 2, { _stem_stem_4_Conv_output_0_output.data->data,_stem_stem_5_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_5_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(22, 1, { _stem_stem_5_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_conv1_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 10496);
  _res_res_0_conv1_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10496);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20224);
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, { _res_res_0_conv1_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, { _res_res_0_norm1_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(25, 1, { _res_res_0_norm1_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(25, 1, { _res_res_0_norm1_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_0_norm1_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, { _res_res_0_norm1_Reshape_output_0_to_chfirst_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_0_norm1_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, { _res_res_0_norm1_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(27, 1, { _res_res_0_norm1_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(27, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(27, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(27, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_conv2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29184);
  _res_res_0_conv2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29184);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 1, { _res_res_0_conv2_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, { _res_res_0_norm2_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 1, { _res_res_0_norm2_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, { _res_res_0_norm2_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_0_norm2_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(33, 1, { _res_res_0_norm2_Reshape_output_0_to_chfirst_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_0_norm2_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(33, 1, { _res_res_0_norm2_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(34, 1, { _res_res_0_norm2_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(34, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(34, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(34, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_0_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _stem_stem_5_Mul_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Mul_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(37, 2, { _res_res_0_norm2_Mul_output_0_output.data->data,_stem_stem_5_Mul_1_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_res_res_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(37, 1, { _res_res_0_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_conv1_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 10496);
  _res_res_1_conv1_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 10496);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20224);
  _STAI_NETWORK_EVENT_NODE_START_CB(40, 1, { _res_res_1_conv1_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(40, 1, { _res_res_1_norm1_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 20224);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(40, 1, { _res_res_1_norm1_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(40, 1, { _res_res_1_norm1_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_1_norm1_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(41, 1, { _res_res_1_norm1_Reshape_output_0_to_chfirst_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_1_norm1_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(41, 1, { _res_res_1_norm1_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(42, 1, { _res_res_1_norm1_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(42, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(42, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(42, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_conv2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 29184);
  _res_res_1_conv2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 29184);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(47, 1, { _res_res_1_conv2_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(47, 1, { _res_res_1_norm2_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(47, 1, { _res_res_1_norm2_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(47, 1, { _res_res_1_norm2_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_1_norm2_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 144132);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 144164);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(48, 1, { _res_res_1_norm2_Reshape_output_0_to_chfirst_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_1_norm2_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(48, 1, { _res_res_1_norm2_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(49, 1, { _res_res_1_norm2_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(49, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(49, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(49, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_1_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_norm2_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_Relu_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_0_Relu_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(52, 2, { _res_res_1_norm2_Mul_output_0_output.data->data,_res_res_0_Relu_1_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_res_res_1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(52, 1, { _res_res_1_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_ap__pool_GlobalAveragePool_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_Relu_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_Relu_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _pool_GlobalAveragePool_output_0_output0_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  _pool_GlobalAveragePool_output_0_output0_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(54, 1, { _res_res_1_Relu_1_output_0_output.data->data});
  forward_ap((ai_layer *)&_pool_GlobalAveragePool_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(54, 1, { _pool_GlobalAveragePool_output_0_output0.data->data});
}

/*****************************************************************************/



static const ai_u32 _stem_stem_0_Conv_output_0_t_in_0_shape_ch_const_u32 = 3;
static const ai_u32 _stem_stem_0_Conv_output_0_t_out_0_shape_ch_const_u32 = 32;
static const ai_u32 _stem_stem_0_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_0_Conv_output_0_t_in_0_shape_h_const_u32 = 300;
static const ai_u32 _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u32 = 150;
static const ai_u32 _stem_stem_0_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_0_Conv_output_0_t_weight_0_shape_h_const_u32 = 7;
static const ai_i32 _stem_stem_0_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _stem_stem_0_Conv_output_0_l_pad_H_0_const_s32 = 3;
static const ai_u16 _stem_stem_0_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_u16 _stem_stem_0_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_l_dilation_W_const_u16 = 1;


static const ai_i32 _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_2_Conv_output_0_t_in_0_shape_ch_const_u32 = 32;
static const ai_u32 _stem_stem_2_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _stem_stem_2_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_2_Conv_output_0_t_in_0_shape_h_const_u32 = 150;
static const ai_u32 _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u32 = 75;
static const ai_u32 _stem_stem_2_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_2_Conv_output_0_t_weight_0_shape_h_const_u32 = 5;
static const ai_i32 _stem_stem_2_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _stem_stem_2_Conv_output_0_l_pad_H_0_const_s32 = 2;
static const ai_u16 _stem_stem_2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_u16 _stem_stem_2_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_l_dilation_W_const_u16 = 1;


static const ai_i32 _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_4_Conv_output_0_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 _stem_stem_4_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _stem_stem_4_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_4_Conv_output_0_t_in_0_shape_h_const_u32 = 75;
static const ai_u32 _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u32 = 38;
static const ai_u32 _stem_stem_4_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _stem_stem_4_Conv_output_0_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 _stem_stem_4_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _stem_stem_4_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_u16 _stem_stem_4_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_l_dilation_W_const_u16 = 1;


static const ai_i32 _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;




static const ai_u32 _res_res_0_conv1_Conv_output_0_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_in_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv1_Conv_output_0_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 _res_res_0_conv1_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _res_res_0_conv1_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_dilation_W_const_u16 = 1;







static const ai_i32 _res_res_0_Relu_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;

static const ai_u32 _res_res_0_conv2_Conv_output_0_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_in_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_0_conv2_Conv_output_0_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 _res_res_0_conv2_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _res_res_0_conv2_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_dilation_W_const_u16 = 1;








static const ai_i32 _res_res_0_Relu_1_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;

static const ai_u32 _res_res_1_conv1_Conv_output_0_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_in_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv1_Conv_output_0_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 _res_res_1_conv1_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _res_res_1_conv1_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_dilation_W_const_u16 = 1;







static const ai_i32 _res_res_1_Relu_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;

static const ai_u32 _res_res_1_conv2_Conv_output_0_t_in_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_out_0_shape_ch_const_u32 = 64;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_in_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_in_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u32 = 38;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_weight_0_shape_w_const_u32 = 1;
static const ai_u32 _res_res_1_conv2_Conv_output_0_t_weight_0_shape_h_const_u32 = 3;
static const ai_i32 _res_res_1_conv2_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _res_res_1_conv2_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_dilation_H_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_dilation_W_const_u16 = 1;








static const ai_i32 _res_res_1_Relu_1_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;

STAI_API_ENTRY
stai_return_code stai_network_run(
  stai_network* network,
  const stai_run_mode mode)
{
   STAI_UNUSED(mode)
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_ACTIVATIONS) != STAI_FLAG_ACTIVATIONS,
        STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_INPUTS) != STAI_FLAG_INPUTS,
                  STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_OUTPUTS) != STAI_FLAG_OUTPUTS,
                  STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_WEIGHTS) != STAI_FLAG_WEIGHTS,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)


  /* LITE_KERNEL_SECTION BEGIN curve_Transpose */
  {
    
  forward_lite_transpose_curve_Transpose_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END curve_Transpose */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_0_Conv_output_0 */
  {
      const ai_float* _stem_stem_0_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19840);
    ai_float* _stem_stem_0_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 640);
    const ai_u8* _stem_stem_0_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 4);
    const ai_u8* _stem_stem_0_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 2692);
    ai_float* _stem_stem_0_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 23440);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(8, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_stem_stem_0_Conv_output_0_t_in_0_ptr_const_f32, _stem_stem_0_Conv_output_0_t_out_0_ptr_f32, _stem_stem_0_Conv_output_0_t_weight_0_ptr_const_u8, _stem_stem_0_Conv_output_0_t_weight_1_ptr_const_u8, _stem_stem_0_Conv_output_0_t_scratch_0_ptr_f32, _stem_stem_0_Conv_output_0_t_in_0_shape_ch_const_u32, _stem_stem_0_Conv_output_0_t_out_0_shape_ch_const_u32, _stem_stem_0_Conv_output_0_t_in_0_shape_w_const_u32, _stem_stem_0_Conv_output_0_t_in_0_shape_h_const_u32, _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u32, _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u32, _stem_stem_0_Conv_output_0_t_weight_0_shape_w_const_u32, _stem_stem_0_Conv_output_0_t_weight_0_shape_h_const_u32, _stem_stem_0_Conv_output_0_l_pad_W_0_const_s32, _stem_stem_0_Conv_output_0_l_pad_H_0_const_s32, _stem_stem_0_Conv_output_0_l_stride_1_const_u16, _stem_stem_0_Conv_output_0_l_stride_0_const_u16, 7, 1, _stem_stem_0_Conv_output_0_l_dilation_H_const_u16, _stem_stem_0_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(8, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_0_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_1_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Erf_output_0 */
  {
      ai_handle _stem_stem_1_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 19840);
    const ai_handle _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 19840);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(10, 1, {(stai_ptr) _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_1_Erf_output_0_t_out_0_ptr_handle, _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(10, 1, {(stai_ptr) _stem_stem_1_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Add_output_0 */
  {
      ai_float* _stem_stem_1_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19840);
    const ai_float* _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19840);
    const ai_float* _stem_stem_1_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 2820);
    const ai_float* _stem_stem_1_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 2948);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(11, 1, {(stai_ptr) _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_1_Add_output_0_t_out_0_ptr_f32, _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_1_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_1_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(4800), (ai_size)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(11, 1, {(stai_ptr) _stem_stem_1_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_1_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_2_Conv_output_0 */
  {
      const ai_float* _stem_stem_2_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19840);
    ai_float* _stem_stem_2_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_u8* _stem_stem_2_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 3076);
    const ai_u8* _stem_stem_2_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 44036);
    ai_float* _stem_stem_2_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_stem_stem_2_Conv_output_0_t_in_0_ptr_const_f32, _stem_stem_2_Conv_output_0_t_out_0_ptr_f32, _stem_stem_2_Conv_output_0_t_weight_0_ptr_const_u8, _stem_stem_2_Conv_output_0_t_weight_1_ptr_const_u8, _stem_stem_2_Conv_output_0_t_scratch_0_ptr_f32, _stem_stem_2_Conv_output_0_t_in_0_shape_ch_const_u32, _stem_stem_2_Conv_output_0_t_out_0_shape_ch_const_u32, _stem_stem_2_Conv_output_0_t_in_0_shape_w_const_u32, _stem_stem_2_Conv_output_0_t_in_0_shape_h_const_u32, _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u32, _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u32, _stem_stem_2_Conv_output_0_t_weight_0_shape_w_const_u32, _stem_stem_2_Conv_output_0_t_weight_0_shape_h_const_u32, _stem_stem_2_Conv_output_0_l_pad_W_0_const_s32, _stem_stem_2_Conv_output_0_l_pad_H_0_const_s32, _stem_stem_2_Conv_output_0_l_stride_1_const_u16, _stem_stem_2_Conv_output_0_l_stride_0_const_u16, 5, 1, _stem_stem_2_Conv_output_0_l_dilation_H_const_u16, _stem_stem_2_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_3_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Erf_output_0 */
  {
      ai_handle _stem_stem_3_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 19200);
    const ai_handle _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(15, 1, {(stai_ptr) _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_3_Erf_output_0_t_out_0_ptr_handle, _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(15, 1, {(stai_ptr) _stem_stem_3_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Add_output_0 */
  {
      ai_float* _stem_stem_3_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_float* _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    const ai_float* _stem_stem_3_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 44292);
    const ai_float* _stem_stem_3_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 44548);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 1, {(stai_ptr) _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_3_Add_output_0_t_out_0_ptr_f32, _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_3_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_3_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(4800), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, {(stai_ptr) _stem_stem_3_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_3_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_4_Conv_output_0 */
  {
      const ai_float* _stem_stem_4_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_float* _stem_stem_4_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19968);
    const ai_u8* _stem_stem_4_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 44804);
    const ai_u8* _stem_stem_4_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 93956);
    ai_float* _stem_stem_4_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(18, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_stem_stem_4_Conv_output_0_t_in_0_ptr_const_f32, _stem_stem_4_Conv_output_0_t_out_0_ptr_f32, _stem_stem_4_Conv_output_0_t_weight_0_ptr_const_u8, _stem_stem_4_Conv_output_0_t_weight_1_ptr_const_u8, _stem_stem_4_Conv_output_0_t_scratch_0_ptr_f32, _stem_stem_4_Conv_output_0_t_in_0_shape_ch_const_u32, _stem_stem_4_Conv_output_0_t_out_0_shape_ch_const_u32, _stem_stem_4_Conv_output_0_t_in_0_shape_w_const_u32, _stem_stem_4_Conv_output_0_t_in_0_shape_h_const_u32, _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u32, _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u32, _stem_stem_4_Conv_output_0_t_weight_0_shape_w_const_u32, _stem_stem_4_Conv_output_0_t_weight_0_shape_h_const_u32, _stem_stem_4_Conv_output_0_l_pad_W_0_const_s32, _stem_stem_4_Conv_output_0_l_pad_H_0_const_s32, _stem_stem_4_Conv_output_0_l_stride_1_const_u16, _stem_stem_4_Conv_output_0_l_stride_0_const_u16, 3, 1, _stem_stem_4_Conv_output_0_l_dilation_H_const_u16, _stem_stem_4_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(18, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_4_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_5_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Erf_output_0 */
  {
      ai_handle _stem_stem_5_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
    const ai_handle _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(20, 1, {(stai_ptr) _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_5_Erf_output_0_t_out_0_ptr_handle, _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(20, 1, {(stai_ptr) _stem_stem_5_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Add_output_0 */
  {
      ai_float* _stem_stem_5_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _stem_stem_5_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 44292);
    const ai_float* _stem_stem_5_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 44548);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(21, 1, {(stai_ptr) _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_5_Add_output_0_t_out_0_ptr_f32, _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_5_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_5_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(21, 1, {(stai_ptr) _stem_stem_5_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_5_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Mul_1_output_0 */
  {
      ai_float* _stem_stem_5_Mul_1_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_5_Mul_1_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _stem_stem_5_Mul_1_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 94212);
    const ai_float* _stem_stem_5_Mul_1_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 94468);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(23, 1, {(stai_ptr) _stem_stem_5_Mul_1_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_5_Mul_1_output_0_t_out_0_ptr_f32, _stem_stem_5_Mul_1_output_0_t_in_0_ptr_const_f32, _stem_stem_5_Mul_1_output_0_t_weight_0_ptr_const_f32, _stem_stem_5_Mul_1_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(23, 1, {(stai_ptr) _stem_stem_5_Mul_1_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Mul_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv1_Conv_output_0 */
  {
      const ai_float* _res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_float* _res_res_0_conv1_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 10496);
    const ai_u8* _res_res_0_conv1_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 94724);
    const ai_u8* _res_res_0_conv1_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 143876);
    ai_float* _res_res_0_conv1_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(24, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_f32, _res_res_0_conv1_Conv_output_0_t_out_0_ptr_f32, _res_res_0_conv1_Conv_output_0_t_weight_0_ptr_const_u8, _res_res_0_conv1_Conv_output_0_t_weight_1_ptr_const_u8, _res_res_0_conv1_Conv_output_0_t_scratch_0_ptr_f32, _res_res_0_conv1_Conv_output_0_t_in_0_shape_ch_const_u32, _res_res_0_conv1_Conv_output_0_t_out_0_shape_ch_const_u32, _res_res_0_conv1_Conv_output_0_t_in_0_shape_w_const_u32, _res_res_0_conv1_Conv_output_0_t_in_0_shape_h_const_u32, _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u32, _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u32, _res_res_0_conv1_Conv_output_0_t_weight_0_shape_w_const_u32, _res_res_0_conv1_Conv_output_0_t_weight_0_shape_h_const_u32, _res_res_0_conv1_Conv_output_0_l_pad_W_0_const_s32, _res_res_0_conv1_Conv_output_0_l_pad_H_0_const_s32, _res_res_0_conv1_Conv_output_0_l_stride_1_const_u16, _res_res_0_conv1_Conv_output_0_l_stride_0_const_u16, 3, 1, _res_res_0_conv1_Conv_output_0_l_dilation_H_const_u16, _res_res_0_conv1_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(24, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_conv1_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Reshape_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Reshape_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Reshape_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Reshape_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_InstanceNormalization_output_0 */
  {
    
  forward_lite_instanceNormalization__res_res_0_norm1_InstanceNormalization_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_InstanceNormalization_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Reshape_1_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Reshape_1_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Reshape_1_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Reshape_1_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Mul_output_0 */
  {
      ai_float* _res_res_0_norm1_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_0_norm1_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    const ai_float* _res_res_0_norm1_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 144196);
    const ai_float* _res_res_0_norm1_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 144452);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(29, 1, {(stai_ptr) _res_res_0_norm1_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_0_norm1_Mul_output_0_t_out_0_ptr_f32, _res_res_0_norm1_Mul_output_0_t_in_0_ptr_const_f32, _res_res_0_norm1_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_0_norm1_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(29, 1, {(stai_ptr) _res_res_0_norm1_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_Relu_output_0 */
  {
      ai_handle _res_res_0_Relu_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 19456);
    const ai_handle _res_res_0_Relu_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(30, 1, {(stai_ptr) _res_res_0_Relu_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_relu_if32of32(_res_res_0_Relu_output_0_t_out_0_ptr_handle, _res_res_0_Relu_output_0_t_in_0_ptr_const_handle, _res_res_0_Relu_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(30, 1, {(stai_ptr) _res_res_0_Relu_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv2_Conv_output_0 */
  {
      const ai_float* _res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    ai_float* _res_res_0_conv2_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29184);
    const ai_u8* _res_res_0_conv2_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 144708);
    const ai_u8* _res_res_0_conv2_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 193860);
    ai_float* _res_res_0_conv2_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(31, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_f32, _res_res_0_conv2_Conv_output_0_t_out_0_ptr_f32, _res_res_0_conv2_Conv_output_0_t_weight_0_ptr_const_u8, _res_res_0_conv2_Conv_output_0_t_weight_1_ptr_const_u8, _res_res_0_conv2_Conv_output_0_t_scratch_0_ptr_f32, _res_res_0_conv2_Conv_output_0_t_in_0_shape_ch_const_u32, _res_res_0_conv2_Conv_output_0_t_out_0_shape_ch_const_u32, _res_res_0_conv2_Conv_output_0_t_in_0_shape_w_const_u32, _res_res_0_conv2_Conv_output_0_t_in_0_shape_h_const_u32, _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u32, _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u32, _res_res_0_conv2_Conv_output_0_t_weight_0_shape_w_const_u32, _res_res_0_conv2_Conv_output_0_t_weight_0_shape_h_const_u32, _res_res_0_conv2_Conv_output_0_l_pad_W_0_const_s32, _res_res_0_conv2_Conv_output_0_l_pad_H_0_const_s32, _res_res_0_conv2_Conv_output_0_l_stride_1_const_u16, _res_res_0_conv2_Conv_output_0_l_stride_0_const_u16, 3, 1, _res_res_0_conv2_Conv_output_0_l_dilation_H_const_u16, _res_res_0_conv2_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(31, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_conv2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Reshape_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Reshape_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Reshape_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Reshape_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_InstanceNormalization_output_0 */
  {
    
  forward_lite_instanceNormalization__res_res_0_norm2_InstanceNormalization_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_InstanceNormalization_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Reshape_1_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Reshape_1_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Reshape_1_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Reshape_1_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Mul_output_0 */
  {
      ai_float* _res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 194116);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 194372);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(36, 1, {(stai_ptr) _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32, _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32, _res_res_0_norm2_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_0_norm2_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(36, 1, {(stai_ptr) _res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_0_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_Relu_1_output_0 */
  {
      ai_handle _res_res_0_Relu_1_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle _res_res_0_Relu_1_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(38, 1, {(stai_ptr) _res_res_0_Relu_1_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_relu_if32of32(_res_res_0_Relu_1_output_0_t_out_0_ptr_handle, _res_res_0_Relu_1_output_0_t_in_0_ptr_const_handle, _res_res_0_Relu_1_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(38, 1, {(stai_ptr) _res_res_0_Relu_1_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_Relu_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv1_Conv_output_0 */
  {
      const ai_float* _res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_float* _res_res_1_conv1_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 10496);
    const ai_u8* _res_res_1_conv1_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 194628);
    const ai_u8* _res_res_1_conv1_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 243780);
    ai_float* _res_res_1_conv1_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(39, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_f32, _res_res_1_conv1_Conv_output_0_t_out_0_ptr_f32, _res_res_1_conv1_Conv_output_0_t_weight_0_ptr_const_u8, _res_res_1_conv1_Conv_output_0_t_weight_1_ptr_const_u8, _res_res_1_conv1_Conv_output_0_t_scratch_0_ptr_f32, _res_res_1_conv1_Conv_output_0_t_in_0_shape_ch_const_u32, _res_res_1_conv1_Conv_output_0_t_out_0_shape_ch_const_u32, _res_res_1_conv1_Conv_output_0_t_in_0_shape_w_const_u32, _res_res_1_conv1_Conv_output_0_t_in_0_shape_h_const_u32, _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u32, _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u32, _res_res_1_conv1_Conv_output_0_t_weight_0_shape_w_const_u32, _res_res_1_conv1_Conv_output_0_t_weight_0_shape_h_const_u32, _res_res_1_conv1_Conv_output_0_l_pad_W_0_const_s32, _res_res_1_conv1_Conv_output_0_l_pad_H_0_const_s32, _res_res_1_conv1_Conv_output_0_l_stride_1_const_u16, _res_res_1_conv1_Conv_output_0_l_stride_0_const_u16, 3, 1, _res_res_1_conv1_Conv_output_0_l_dilation_H_const_u16, _res_res_1_conv1_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(39, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_conv1_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Reshape_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Reshape_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Reshape_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Reshape_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_InstanceNormalization_output_0 */
  {
    
  forward_lite_instanceNormalization__res_res_1_norm1_InstanceNormalization_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_InstanceNormalization_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Reshape_1_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Reshape_1_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Reshape_1_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Reshape_1_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Mul_output_0 */
  {
      ai_float* _res_res_1_norm1_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    const ai_float* _res_res_1_norm1_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 244036);
    const ai_float* _res_res_1_norm1_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 244292);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(44, 1, {(stai_ptr) _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_1_norm1_Mul_output_0_t_out_0_ptr_f32, _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32, _res_res_1_norm1_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_1_norm1_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(44, 1, {(stai_ptr) _res_res_1_norm1_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_Relu_output_0 */
  {
      ai_handle _res_res_1_Relu_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 19456);
    const ai_handle _res_res_1_Relu_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(45, 1, {(stai_ptr) _res_res_1_Relu_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_relu_if32of32(_res_res_1_Relu_output_0_t_out_0_ptr_handle, _res_res_1_Relu_output_0_t_in_0_ptr_const_handle, _res_res_1_Relu_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(45, 1, {(stai_ptr) _res_res_1_Relu_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_Relu_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv2_Conv_output_0 */
  {
      const ai_float* _res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    ai_float* _res_res_1_conv2_Conv_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 29184);
    const ai_u8* _res_res_1_conv2_Conv_output_0_t_weight_0_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 244548);
    const ai_u8* _res_res_1_conv2_Conv_output_0_t_weight_1_ptr_const_u8 = (ai_u8*)(net_ctx->_weights[0] + 293700);
    ai_float* _res_res_1_conv2_Conv_output_0_t_scratch_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(46, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_conv2d_if32of32wf32(_res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_f32, _res_res_1_conv2_Conv_output_0_t_out_0_ptr_f32, _res_res_1_conv2_Conv_output_0_t_weight_0_ptr_const_u8, _res_res_1_conv2_Conv_output_0_t_weight_1_ptr_const_u8, _res_res_1_conv2_Conv_output_0_t_scratch_0_ptr_f32, _res_res_1_conv2_Conv_output_0_t_in_0_shape_ch_const_u32, _res_res_1_conv2_Conv_output_0_t_out_0_shape_ch_const_u32, _res_res_1_conv2_Conv_output_0_t_in_0_shape_w_const_u32, _res_res_1_conv2_Conv_output_0_t_in_0_shape_h_const_u32, _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u32, _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u32, _res_res_1_conv2_Conv_output_0_t_weight_0_shape_w_const_u32, _res_res_1_conv2_Conv_output_0_t_weight_0_shape_h_const_u32, _res_res_1_conv2_Conv_output_0_l_pad_W_0_const_s32, _res_res_1_conv2_Conv_output_0_l_pad_H_0_const_s32, _res_res_1_conv2_Conv_output_0_l_stride_1_const_u16, _res_res_1_conv2_Conv_output_0_l_stride_0_const_u16, 3, 1, _res_res_1_conv2_Conv_output_0_l_dilation_H_const_u16, _res_res_1_conv2_Conv_output_0_l_dilation_W_const_u16, (ai_size)(1));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(46, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_conv2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Reshape_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Reshape_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Reshape_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Reshape_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_InstanceNormalization_output_0 */
  {
    
  forward_lite_instanceNormalization__res_res_1_norm2_InstanceNormalization_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_InstanceNormalization_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Reshape_1_output_0_to_chlast */
  {
    
  forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chlast_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Reshape_1_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Reshape_1_output_0_to_chfirst */
  {
    
  forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chfirst_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Reshape_1_output_0_to_chfirst */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Mul_output_0 */
  {
      ai_float* _res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 293956);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 294212);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(51, 1, {(stai_ptr) _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32, _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32, _res_res_1_norm2_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_1_norm2_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(51, 1, {(stai_ptr) _res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_1_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_Relu_1_output_0 */
  {
      ai_handle _res_res_1_Relu_1_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(53, 1, {(stai_ptr) _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_relu_if32of32(_res_res_1_Relu_1_output_0_t_out_0_ptr_handle, _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle, _res_res_1_Relu_1_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(53, 1, {(stai_ptr) _res_res_1_Relu_1_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_Relu_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _pool_GlobalAveragePool_output_0 */
  {
    
  forward_lite_ap__pool_GlobalAveragePool_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _pool_GlobalAveragePool_output_0 */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_network_get_context_size()
{
  return (stai_size)STAI_NETWORK_CONTEXT_SIZE;
}

#if defined(HAVE_NETWORK_INFO)
STAI_API_ENTRY
stai_return_code stai_network_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_network_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_network_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_NETWORK_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_NETWORK_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_NETWORK_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_NETWORK_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_NETWORK_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_NETWORK_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_NETWORK_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_NETWORK_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_NETWORK_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_network_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_NETWORK_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_network_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_NETWORK_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }
  net_ctx->_inputs[0] = activations[0] + 16240;

  net_ctx->_outputs[0] = activations[0] + 9728;
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_NETWORK_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_NETWORK_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_NETWORK_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_NETWORK_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_NETWORK_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_NETWORK_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_NETWORK_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_network_set_callback(
  stai_network* network, const stai_event_cb cb, void* cb_cookie)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  STAI_PRINT("  set_callback %p cb %p cookie %p\n", net_ctx, cb, cb_cookie)
  // _STAI_SET_ERROR(net_ctx, cb==NULL, STAI_ERROR_NETWORK_INVALID_CALLBACK, net_ctx->_return_code)
  net_ctx->_callback = cb;
  net_ctx->_callback_cookie = cb_cookie;
  return net_ctx->_return_code;
}

#undef _STAI_SET_ERROR
#undef _STAI_CONTEXT_ALIGNMENT
#undef _STAI_CONTEXT_ACQUIRE
#undef _STAI_NETWORK_EVENT_NODE_START_CB
#undef _STAI_NETWORK_EVENT_NODE_STOP_CB
#undef _STAI_NETWORK_MODEL_SIGNATURE
#undef _STAI_NETWORK_DATETIME
#undef _STAI_NETWORK_COMPILE_DATETIME

