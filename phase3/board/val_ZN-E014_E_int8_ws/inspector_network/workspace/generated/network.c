/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:32:30+0700
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

#include "lite_operators.h"

#include "ai_lite_inspect.h"
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xac5040c47255d330324a0207b92041ea"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:32:30+0700"
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
      STAI_DECLARE_ARRAY(int32_t, 1, 38400),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 77160),
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



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(curve_QuantizeLinear_Output_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08599395304918289f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(curve_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08599395304918289f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00800393894314766f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00800393894314766f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00800393894314766f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020032256841659546f),
    AI_PACK_INTQ_ZP(10)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020032256841659546f),
    AI_PACK_INTQ_ZP(10)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020032256841659546f),
    AI_PACK_INTQ_ZP(10)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01838567480444908f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01838567480444908f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01838567480444908f),
    AI_PACK_INTQ_ZP(-9)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01842520385980606f),
    AI_PACK_INTQ_ZP(-18)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01842520385980606f),
    AI_PACK_INTQ_ZP(-18)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01842520385980606f),
    AI_PACK_INTQ_ZP(-18)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  curve_QuantizeLinear_Output_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 900, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  curve_Transpose_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 900, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_0_Conv_output_0_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
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
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
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
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
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
  _stem_stem_5_Mul_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  _stem_stem_1_Constant_2_output_0_3D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_conv1_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 8, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 8, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  onnxAdd_115_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_conv2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_norm2_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_0_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_conv1_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  onnxAdd_119_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_conv2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2432, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_norm2_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  _res_res_1_Relu_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 2432, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_output0_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 64, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  curve_QuantizeLinear_Output_output, AI_STATIC,
  113, 0x1,
  AI_SHAPE_INIT(4, 1, 300, 1, 3), AI_STRIDE_INIT(4, 1, 1, 300, 300),
  1, &curve_QuantizeLinear_Output_output_array, &curve_QuantizeLinear_Output_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  curve_Transpose_output, AI_STATIC,
  114, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 300), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &curve_Transpose_output_array, &curve_Transpose_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_0_Conv_output_0_0_conversion_output, AI_STATIC,
  74, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_0_Conv_output_0_0_conversion_output_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Constant_output_0_3D, AI_STATIC,
  83, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_stem_stem_1_Constant_output_0_3D_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_output, AI_STATIC,
  84, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Div_output_0_output_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Add_output_0_output, AI_STATIC,
  80, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Add_output_0_output_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_output, AI_STATIC,
  87, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 150), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &_stem_stem_1_Mul_output_0_output_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output, AI_STATIC,
  88, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_output, AI_STATIC,
  97, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Div_output_0_output_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Add_output_0_output, AI_STATIC,
  95, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Add_output_0_output_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_output, AI_STATIC,
  100, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 75), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_3_Mul_output_0_output_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output, AI_STATIC,
  101, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_output, AI_STATIC,
  108, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Div_output_0_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Add_output_0_output, AI_STATIC,
  107, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Add_output_0_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_output, AI_STATIC,
  112, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Mul_output_0_output_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_1_Constant_2_output_0_3D, AI_STATIC,
  82, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_stem_stem_1_Constant_2_output_0_3D_array, NULL)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  _stem_stem_5_Mul_1_output_0_output, AI_STATIC,
  111, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_stem_stem_5_Mul_1_output_0_output_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_conv1_Conv_output_0_output, AI_STATIC,
  4, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_res_res_0_conv1_Conv_output_0_output_array, &_res_res_0_conv1_Conv_output_0_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output, AI_STATIC,
  26, 0x1,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 1, 1, 38, 38),
  1, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array, &_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chlast_output0, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 1, 1, 304, 304),
  1, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array, &_res_res_0_norm1_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_bias, AI_STATIC,
  15, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_bias_array, NULL)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_output, AI_STATIC,
  16, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_scale, AI_STATIC,
  17, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 1), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_InstanceNormalization_output_0_scale_array, NULL)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  22, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  21, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  23, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Add_output_0_output, AI_STATIC,
  14, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm1_Add_output_0_output_array, NULL)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm1_Mul_output_0_output, AI_STATIC,
  19, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm1_Mul_output_0_output_array, NULL)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  onnxAdd_115, AI_STATIC,
  116, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &onnxAdd_115_array, NULL)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_conv2_Conv_output_0_output, AI_STATIC,
  9, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_res_res_0_conv2_Conv_output_0_output_array, &_res_res_0_conv2_Conv_output_0_output_array_intq)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output, AI_STATIC,
  37, 0x1,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 1, 1, 38, 38),
  1, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output, AI_STATIC,
  36, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array, &_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array_intq)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_output0, AI_STATIC,
  38, 0x1,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 1, 1, 304, 304),
  1, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array, &_res_res_0_norm2_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_output, AI_STATIC,
  28, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm2_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output, AI_STATIC,
  35, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output_array, NULL)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  33, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  32, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  34, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_Add_output_0_output, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_Add_output_0_output_array, NULL)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_0_norm2_Mul_output_0_output, AI_STATIC,
  30, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_0_norm2_Mul_output_0_output_array, NULL)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_conv1_Conv_output_0_output, AI_STATIC,
  42, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_res_res_1_conv1_Conv_output_0_output_array, &_res_res_1_conv1_Conv_output_0_output_array_intq)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output, AI_STATIC,
  61, 0x1,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 1, 1, 38, 38),
  1, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output, AI_STATIC,
  60, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array, &_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array_intq)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chlast_output0, AI_STATIC,
  62, 0x1,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 1, 1, 304, 304),
  1, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array, &_res_res_1_norm1_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_output, AI_STATIC,
  53, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm1_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output, AI_STATIC,
  59, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output_array, NULL)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  57, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  56, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  58, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Add_output_0_output, AI_STATIC,
  52, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm1_Add_output_0_output_array, NULL)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm1_Mul_output_0_output, AI_STATIC,
  54, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm1_Mul_output_0_output_array, NULL)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  onnxAdd_119, AI_STATIC,
  117, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &onnxAdd_119_array, NULL)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_conv2_Conv_output_0_output, AI_STATIC,
  47, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_res_res_1_conv2_Conv_output_0_output_array, &_res_res_1_conv2_Conv_output_0_output_array_intq)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output, AI_STATIC,
  72, 0x1,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 1, 1, 38, 38),
  1, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 1, 1, 8, 8),
  1, &_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array, &_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array_intq)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_output0, AI_STATIC,
  73, 0x1,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 1, 1, 304, 304),
  1, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array, &_res_res_1_norm2_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_output, AI_STATIC,
  63, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm2_InstanceNormalization_output_0_output_array, NULL)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output, AI_STATIC,
  70, 0x0,
  AI_SHAPE_INIT(4, 1, 8, 1, 304), AI_STRIDE_INIT(4, 4, 4, 32, 32),
  1, &_res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output_array, NULL)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  68, 0x0,
  AI_SHAPE_INIT(4, 1, 304, 1, 8), AI_STRIDE_INIT(4, 4, 4, 1216, 1216),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output, AI_STATIC,
  67, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array, NULL)

/* Tensor #62 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  69, 0x0,
  AI_SHAPE_INIT(4, 1, 38, 1, 64), AI_STRIDE_INIT(4, 4, 4, 152, 152),
  1, &_res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array, NULL)

/* Tensor #63 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_Add_output_0_output, AI_STATIC,
  39, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_Add_output_0_output_array, NULL)

/* Tensor #64 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_norm2_Mul_output_0_output, AI_STATIC,
  65, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_norm2_Mul_output_0_output_array, NULL)

/* Tensor #65 */
AI_TENSOR_OBJ_DECLARE(
  _pool_GlobalAveragePool_output_0_output0, AI_STATIC,
  0, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_pool_GlobalAveragePool_output_0_output0_array, NULL)

/* Tensor #66 */
AI_TENSOR_OBJ_DECLARE(
  _res_res_1_Relu_1_output_0_output, AI_STATIC,
  40, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 38), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_res_res_1_Relu_1_output_0_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  curve_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&curve_QuantizeLinear_Output_output),
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
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_0_Conv_output_0_0_conversion_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_1_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_1_Div_output_0_layer, 20,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_1_Div_output_0_chain,
  NULL, &_stem_stem_1_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_0_Conv_output_0_0_conversion_output, (ai_tensor *)&_stem_stem_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_1_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_1_Mul_output_0_layer, 23,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_1_Mul_output_0_chain,
  NULL, &_stem_stem_1_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_3_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_3_Div_output_0_layer, 29,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_3_Div_output_0_chain,
  NULL, &_stem_stem_3_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output, (ai_tensor *)&_stem_stem_3_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_3_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_3_Mul_output_0_layer, 32,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_3_Mul_output_0_chain,
  NULL, &_stem_stem_3_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output, (ai_tensor *)&_stem_stem_1_Constant_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_5_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_5_Div_output_0_layer, 38,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_5_Div_output_0_chain,
  NULL, &_stem_stem_5_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output, (ai_tensor *)&_stem_stem_5_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_5_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_5_Mul_output_0_layer, 41,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_5_Mul_output_0_chain,
  NULL, &_stem_stem_5_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _stem_stem_5_Mul_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_stem_stem_5_Mul_output_0_output, (ai_tensor *)&_stem_stem_1_Constant_2_output_0_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_stem_stem_5_Mul_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _stem_stem_5_Mul_1_output_0_layer, 42,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_stem_stem_5_Mul_1_output_0_chain,
  NULL, &_stem_stem_5_Mul_1_output_0_layer, AI_STATIC, 
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
  _res_res_0_norm1_Reshape_output_0_to_chlast_layer, 48,
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
  _res_res_0_norm1_Reshape_output_0_to_chfirst_layer, 48,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm1_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_InstanceNormalization_output_0_layer, 49,
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
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_layer, 50,
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
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer, 50,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_Mul_output_0_output, (ai_tensor *)&onnxAdd_115),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm1_Add_output_0_layer, 52,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_res_res_0_norm1_Add_output_0_chain,
  NULL, &_res_res_0_norm1_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_conv2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_Reshape_output_0_to_chlast_layer, 59,
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
  _res_res_0_norm2_Reshape_output_0_to_chfirst_layer, 59,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_0_norm2_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_0_norm2_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_0_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_0_norm2_InstanceNormalization_output_0_layer, 60,
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
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_layer, 61,
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
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_layer, 61,
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
  _res_res_0_Add_output_0_layer, 64,
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
  _res_res_1_norm1_Reshape_output_0_to_chlast_layer, 71,
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
  _res_res_1_norm1_Reshape_output_0_to_chfirst_layer, 71,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm1_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_InstanceNormalization_output_0_layer, 72,
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
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_layer, 73,
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
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer, 73,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_1_norm1_Mul_output_0_output, (ai_tensor *)&onnxAdd_119),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm1_Add_output_0_layer, 75,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_res_res_1_norm1_Add_output_0_chain,
  NULL, &_res_res_1_norm1_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_conv2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_Reshape_output_0_to_chlast_layer, 82,
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
  _res_res_1_norm2_Reshape_output_0_to_chfirst_layer, 82,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm2_Reshape_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_norm2_InstanceNormalization_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_scale, (ai_tensor *)&_res_res_0_norm1_InstanceNormalization_output_0_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_norm2_InstanceNormalization_output_0_layer, 83,
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
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_layer, 84,
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
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer, 84,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_chain,
  NULL, &_res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer, AI_STATIC, /* AI_LAYER_IS_STATEFUL flags=0x0 */
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _res_res_1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_res_res_1_norm2_Mul_output_0_output, (ai_tensor *)&_res_res_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_res_res_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _res_res_1_Add_output_0_layer, 87,
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
  _pool_GlobalAveragePool_output_0_layer, 89,
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
  curve_QuantizeLinear_Output_output_array.data = AI_PTR(net_ctx->_activations[0] + 21976);
  curve_QuantizeLinear_Output_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21976);
  curve_Transpose_output_array.data = AI_PTR(net_ctx->_activations[0] + 21076);
  curve_Transpose_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21076);
  _STAI_NETWORK_EVENT_NODE_START_CB(2, 1, { curve_QuantizeLinear_Output_output.data->data});
  forward_transpose((ai_layer *)&curve_Transpose_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(2, 1, { curve_Transpose_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_1_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_0_Conv_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_0_Conv_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_1_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(20, 2, { _stem_stem_0_Conv_output_0_0_conversion_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_1_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(20, 1, { _stem_stem_1_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_1_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_0_Conv_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_0_Conv_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _STAI_NETWORK_EVENT_NODE_START_CB(23, 2, { _stem_stem_0_Conv_output_0_0_conversion_output.data->data,_stem_stem_1_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_1_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(23, 1, { _stem_stem_1_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_3_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_3_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_3_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(29, 2, { _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_3_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(29, 1, { _stem_stem_3_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_3_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_3_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_3_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_3_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19200);
  _stem_stem_3_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19200);
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 2, { _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_output.data->data,_stem_stem_3_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_3_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, { _stem_stem_3_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_5_Div_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 2432);
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2432);
  _stem_stem_1_Constant_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_1_Constant_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _stem_stem_5_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _stem_stem_5_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _STAI_NETWORK_EVENT_NODE_START_CB(38, 2, { _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output.data->data,_stem_stem_1_Constant_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_5_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(38, 1, { _stem_stem_5_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_5_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 2432);
  _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2432);
  _stem_stem_5_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _stem_stem_5_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _stem_stem_5_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _stem_stem_5_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _STAI_NETWORK_EVENT_NODE_START_CB(41, 2, { _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_output.data->data,_stem_stem_5_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_5_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(41, 1, { _stem_stem_5_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__stem_stem_5_Mul_1_output_0_network(_stai_network_context* net_ctx)
{
  _stem_stem_5_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _stem_stem_5_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _stem_stem_1_Constant_2_output_0_3D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  _stem_stem_1_Constant_2_output_0_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  _stem_stem_5_Mul_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Mul_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(42, 2, { _stem_stem_5_Mul_output_0_output.data->data,_stem_stem_1_Constant_2_output_0_3D.data->data});
  forward_eltwise((ai_layer *)&_stem_stem_5_Mul_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(42, 1, { _stem_stem_5_Mul_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_conv1_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_conv1_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _STAI_NETWORK_EVENT_NODE_START_CB(48, 1, { _res_res_0_conv1_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(48, 1, { _res_res_0_norm1_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(48, 1, { _res_res_0_norm1_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(48, 1, { _res_res_0_norm1_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_0_norm1_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _STAI_NETWORK_EVENT_NODE_START_CB(49, 1, { _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_0_norm1_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(49, 1, { _res_res_0_norm1_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(50, 1, { _res_res_0_norm1_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(50, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm1_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(50, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm1_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(50, 1, { _res_res_0_norm1_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_0_norm1_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  onnxAdd_115_array.data = AI_PTR(net_ctx->_weights[0] + 37992);
  onnxAdd_115_array.data_start = AI_PTR(net_ctx->_weights[0] + 37992);
  _res_res_0_norm1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(52, 2, { _res_res_0_norm1_Mul_output_0_output.data->data,onnxAdd_115.data->data});
  forward_eltwise((ai_layer *)&_res_res_0_norm1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(52, 1, { _res_res_0_norm1_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_conv2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_conv2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _STAI_NETWORK_EVENT_NODE_START_CB(59, 1, { _res_res_0_conv2_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(59, 1, { _res_res_0_norm2_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(59, 1, { _res_res_0_norm2_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(59, 1, { _res_res_0_norm2_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_0_norm2_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 12160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _STAI_NETWORK_EVENT_NODE_START_CB(60, 1, { _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_0_norm2_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(60, 1, { _res_res_0_norm2_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 21888);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(61, 1, { _res_res_0_norm2_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(61, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_0_norm2_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(61, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_0_norm2_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(61, 1, { _res_res_0_norm2_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_0_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_0_norm2_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm2_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _stem_stem_5_Mul_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _stem_stem_5_Mul_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _STAI_NETWORK_EVENT_NODE_START_CB(64, 2, { _res_res_0_norm2_Mul_output_0_output.data->data,_stem_stem_5_Mul_1_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_res_res_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(64, 1, { _res_res_0_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_conv1_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_conv1_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 2432);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2432);
  _STAI_NETWORK_EVENT_NODE_START_CB(71, 1, { _res_res_1_conv1_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(71, 1, { _res_res_1_norm1_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 2432);
  _res_res_1_norm1_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2432);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(71, 1, { _res_res_1_norm1_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(71, 1, { _res_res_1_norm1_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_1_norm1_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(72, 1, { _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_1_norm1_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(72, 1, { _res_res_1_norm1_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(73, 1, { _res_res_1_norm1_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(73, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm1_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(73, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm1_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(73, 1, { _res_res_1_norm1_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_1_norm1_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  onnxAdd_119_array.data = AI_PTR(net_ctx->_weights[0] + 37736);
  onnxAdd_119_array.data_start = AI_PTR(net_ctx->_weights[0] + 37736);
  _res_res_1_norm1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(75, 2, { _res_res_1_norm1_Mul_output_0_output.data->data,onnxAdd_119.data->data});
  forward_eltwise((ai_layer *)&_res_res_1_norm1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(75, 1, { _res_res_1_norm1_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_conv2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9344);
  _res_res_1_conv2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9344);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(82, 1, { _res_res_1_conv2_Conv_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(82, 1, { _res_res_1_norm2_Reshape_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 2432);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 2432);
  _STAI_NETWORK_EVENT_NODE_START_CB(82, 1, { _res_res_1_norm2_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(82, 1, { _res_res_1_norm2_Reshape_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_instanceNormalization__res_res_1_norm2_InstanceNormalization_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_scale_array.data_start = AI_PTR(net_ctx->_weights[0] + 37160);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_0_norm1_InstanceNormalization_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 37192);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(83, 1, { _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_output.data->data});
  forward_instanceNormalization((ai_layer *)&_res_res_1_norm2_InstanceNormalization_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(83, 1, { _res_res_1_norm2_InstanceNormalization_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chlast_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_InstanceNormalization_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _STAI_NETWORK_EVENT_NODE_START_CB(84, 1, { _res_res_1_norm2_InstanceNormalization_output_0_output.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(84, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chlast_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_transpose__res_res_1_norm2_Reshape_1_output_0_to_chfirst_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(84, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_transpose((ai_layer *)&_res_res_1_norm2_Reshape_1_output_0_to_chfirst_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(84, 1, { _res_res_1_norm2_Reshape_1_output_0_to_chfirst_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__res_res_1_Add_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_norm2_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_norm2_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 19456);
  _res_res_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _res_res_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(87, 2, { _res_res_1_norm2_Mul_output_0_output.data->data,_res_res_0_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_res_res_1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(87, 1, { _res_res_1_Add_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_ap__pool_GlobalAveragePool_output_0_network(_stai_network_context* net_ctx)
{
  _res_res_1_Relu_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 9728);
  _res_res_1_Relu_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 9728);
  _pool_GlobalAveragePool_output_0_output0_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  _pool_GlobalAveragePool_output_0_output0_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(89, 1, { _res_res_1_Relu_1_output_0_output.data->data});
  forward_ap((ai_layer *)&_pool_GlobalAveragePool_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(89, 1, { _pool_GlobalAveragePool_output_0_output0.data->data});
}

/*****************************************************************************/


static const ai_u32 curve_QuantizeLinear_Output_t_out_0_shape_h_w_ch_d_prod_const_u32 = 900;
static const ai_float curve_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32 = 0.08599395304918289f;
static const ai_i8 curve_QuantizeLinear_Output_t_out_0_fmt_zero_const_s8 = -1;


static const ai_u16 _stem_stem_0_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_t_in_0_shape_h_const_u16 = 300;
static const ai_u16 _stem_stem_0_Conv_output_0_t_in_0_shape_ch_const_u16 = 3;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_u16 _stem_stem_0_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_t_weight_0_shape_h_const_u16 = 7;
static const ai_u16 _stem_stem_0_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i32 _stem_stem_0_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _stem_stem_0_Conv_output_0_l_pad_H_0_const_s32 = 3;
static const ai_i8 _stem_stem_0_Conv_output_0_t_in_0_fmt_zero_const_s8 = -1;
static const ai_i8 _stem_stem_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = 4;
static const ai_float _stem_stem_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.08599395304918289f;
static const ai_float _stem_stem_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0780797228217125f;
static const ai_float _stem_stem_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018780193058773875f, 0.001456048688851297f, 0.001937183435074985f, 0.0018342620460316539f, 0.0017048626905307174f, 0.0016571453306823969f, 0.001660663983784616f, 0.0015559521270915866f, 0.00172621535602957f, 0.0017087828600779176f, 0.001657022163271904f, 0.0016754840034991503f, 0.001774761127308011f, 0.001676803338341415f, 0.0014876064378768206f, 0.001704591210000217f, 0.0016620095120742917f, 0.001839747535996139f, 0.0017058287048712373f, 0.0016285381279885769f, 0.0016131122829392552f, 0.0016205532010644674f, 0.001489848829805851f, 0.0016426119254902005f, 0.0017686068313196301f, 0.0016824109479784966f, 0.0017757280729711056f, 0.0016977039631456137f, 0.0016427397495135665f, 0.0017453697510063648f, 0.0017431536689400673f, 0.0016657272353768349f);
static const ai_layer_format_type _stem_stem_0_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u16 = 150;

static const ai_u32 _stem_stem_0_Conv_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.0780797228217125f;
static const ai_i8 _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 4;


static const ai_i32 _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.07677365839481354f;
static const ai_i8 _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -124;

static const ai_i8 _stem_stem_2_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-124);
static const ai_i16 _stem_stem_2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _stem_stem_2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 150;

static const ai_u16 _stem_stem_2_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_t_in_0_shape_h_const_u16 = 154;
static const ai_u16 _stem_stem_2_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _stem_stem_2_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_t_weight_0_shape_h_const_u16 = 5;
static const ai_u16 _stem_stem_2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stem_stem_2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -124;
static const ai_i8 _stem_stem_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = 3;
static const ai_float _stem_stem_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.07677365839481354f;
static const ai_float _stem_stem_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03483831137418747f;
static const ai_float _stem_stem_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0003974821011070162f, 0.0003353262727614492f, 0.00043142822687514126f, 0.0004619430983439088f, 0.0006039001164026558f, 0.00035451012081466615f, 0.0004102075763512403f, 0.00036103924503549933f, 0.00040117898606695235f, 0.000346976681612432f, 0.0004820939793717116f, 0.0004935408942401409f, 0.00039019642281346023f, 0.00033665099181234837f, 0.00033303064992651343f, 0.00033501203870400786f, 0.0004936577170155942f, 0.0004632590862456709f, 0.0005660148453898728f, 0.00043557985918596387f, 0.0003360951377544552f, 0.00033773170434869826f, 0.0003590108535718173f, 0.0004731575900223106f, 0.00034596252953633666f, 0.0003689256263896823f, 0.00045895588118582964f, 0.0004022850189357996f, 0.0003699624794535339f, 0.00036312892916612327f, 0.00030445531592704356f, 0.0003036944835912436f, 0.00036681003984995186f, 0.00042143918108195066f, 0.0004496163164731115f, 0.00037051973049528897f, 0.00036021057167090476f, 0.00034437133581377566f, 0.00039182486943900585f, 0.00033127565984614193f, 0.0003515827120281756f, 0.0003929956292267889f, 0.000423496967414394f, 0.0003803007712122053f, 0.00039104087045416236f, 0.00035095232306048274f, 0.0003594761365093291f, 0.0003888244682457298f, 0.00045950867934152484f, 0.0003417459374759346f, 0.00041855775634758174f, 0.00032853984157554805f, 0.0003375906671863049f, 0.00044624830479733646f, 0.0003157581959385425f, 0.0003595921443775296f, 0.0004884829395450652f, 0.00039132696110755205f, 0.0003401084104552865f, 0.0003553649876266718f, 0.0004311508091632277f, 0.00040144441300071776f, 0.0004362048057373613f, 0.0003234492614865303f);
static const ai_layer_format_type _stem_stem_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u16 = 75;

static const ai_u32 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.03483831137418747f;
static const ai_i8 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = 3;


static const ai_i32 _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.03527851775288582f;
static const ai_i8 _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -118;

static const ai_i8 _stem_stem_4_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-118);
static const ai_i16 _stem_stem_4_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _stem_stem_4_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 75;

static const ai_u16 _stem_stem_4_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_t_in_0_shape_h_const_u16 = 77;
static const ai_u16 _stem_stem_4_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _stem_stem_4_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _stem_stem_4_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _stem_stem_4_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _stem_stem_4_Conv_output_0_t_out_0_fmt_zero_const_s8 = -15;
static const ai_float _stem_stem_4_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03527851775288582f;
static const ai_float _stem_stem_4_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.017076365649700165f;
static const ai_float _stem_stem_4_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0003148647374473512f, 0.00029721963801421225f, 0.0003486427303869277f, 0.0003091565740760416f, 0.0003507731016725302f, 0.0003110768157057464f, 0.00031061898334883153f, 0.00029528469895012677f, 0.00033533392706885934f, 0.0002927456807810813f, 0.00035074586048722267f, 0.00032267378992401063f, 0.00041070961742661893f, 0.00032113533234223723f, 0.0003186777757946402f, 0.0003648364800028503f, 0.0003368928737472743f, 0.00030636429437436163f, 0.000337721110554412f, 0.0003219813806936145f, 0.0003237179189454764f, 0.0003278914955444634f, 0.00030315102776512504f, 0.0003677240456454456f, 0.00032564924913458526f, 0.0003234777832403779f, 0.00033725632238201797f, 0.0003134963335469365f, 0.0003266522544436157f, 0.0003206633555237204f, 0.0003223903477191925f, 0.0003062507603317499f, 0.0003064334741793573f, 0.00032554229255765676f, 0.0003322250850033015f, 0.0003159242623951286f, 0.00032448116689920425f, 0.0002963572333101183f, 0.00031127387774176896f, 0.0002995938411913812f, 0.00036631160764954984f, 0.0003103863273281604f, 0.00039558805292472243f, 0.00030201300978660583f, 0.00032150145852938294f, 0.00034902888000942767f, 0.00032126635778695345f, 0.0004530927981249988f, 0.00032202000147663057f, 0.0003096782020293176f, 0.00031532259890809655f, 0.00029923467081971467f, 0.0003163589281029999f, 0.0004303649184294045f, 0.0003020200238097459f, 0.00038998579839244485f, 0.00039812500472180545f, 0.0003828852204605937f, 0.0003235612530261278f, 0.0003107519878540188f, 0.00041552356560714543f, 0.0003117969899903983f, 0.00032797869062051177f, 0.0003154599980916828f);
static const ai_layer_format_type _stem_stem_4_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u16 = 38;

static const ai_u32 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.017076365649700165f;
static const ai_i8 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = -15;


static const ai_i32 _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;




static const ai_u32 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.010129106231033802f;
static const ai_i8 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -111;

static const ai_i8 _res_res_0_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-111);
static const ai_i16 _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 38;

static const ai_u16 _res_res_0_conv1_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8 = -111;
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = 3;
static const ai_float _res_res_0_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.010129106231033802f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.00800393894314766f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0005851397872902453f, 0.0005860556266270578f, 0.0006918328581377864f, 0.000600065803155303f, 0.0006404988816939294f, 0.0006086959620006382f, 0.0006254900363273919f, 0.0005925501463934779f, 0.0005997950793243945f, 0.0006209590355865657f, 0.0006229462451301515f, 0.0005870551685802639f, 0.0006254272302612662f, 0.0007103479001671076f, 0.0006874187383800745f, 0.0006314852507784963f, 0.0006465524202212691f, 0.0006839599809609354f, 0.0006258820649236441f, 0.0006293238839134574f, 0.0005835330812260509f, 0.0006386483437381685f, 0.0006209418061189353f, 0.00058568554231897f, 0.0006982941995374858f, 0.0007292548543773592f, 0.0005751581047661602f, 0.0005853991024196148f, 0.0006790479528717697f, 0.0006381272687576711f, 0.000718522525858134f, 0.0005846807616762817f, 0.000589999312069267f, 0.0006345165311358869f, 0.0006374496733769774f, 0.0005829582223668694f, 0.0005771471769548953f, 0.0006228606798686087f, 0.000578875420615077f, 0.0005985116004012525f, 0.0005953163490630686f, 0.0007440341869369149f, 0.0005976614193059504f, 0.0005884758429601789f, 0.0006214822642505169f, 0.0006266893469728529f, 0.0005697188316844404f, 0.0006290845922194421f, 0.0006540148169733584f, 0.0006510680541396141f, 0.0006086499197408557f, 0.0006156812887638807f, 0.0005968664190731943f, 0.0005925244768150151f, 0.0005866059800609946f, 0.0007613201159983873f, 0.00062599714146927f, 0.0006386855966411531f, 0.000635060016065836f, 0.000656713789794594f, 0.0006561711779795587f, 0.000605291104875505f, 0.0005957806715741754f, 0.0007176700164563954f);
static const ai_layer_format_type _res_res_0_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.00800393894314766f;
static const ai_i8 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 3;






static const ai_u32 _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.020935801789164543f;
static const ai_i8 _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 _res_res_0_conv2_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 38;

static const ai_u16 _res_res_0_conv2_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _res_res_0_conv2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _res_res_0_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = 10;
static const ai_float _res_res_0_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.020935801789164543f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.020032256841659546f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001058974419720471f, 0.0009978117886930704f, 0.0007438851753249764f, 0.0009110060054808855f, 0.0006376190576702356f, 0.0006760351825505495f, 0.0009756659856066108f, 0.0007397838635370135f, 0.0006486618658527732f, 0.0007570384186692536f, 0.0010478986660018563f, 0.0008895252249203622f, 0.0006978497258387506f, 0.0008777723996900022f, 0.0007103618700057268f, 0.0008765552775003016f, 0.0006615286110900342f, 0.0005923545686528087f, 0.0007479977211914957f, 0.000766255718190223f, 0.0008066237787716091f, 0.0009969022357836366f, 0.0006950779934413731f, 0.0007721867295913398f, 0.0006886006449349225f, 0.0009962773183360696f, 0.0005883235717192292f, 0.0008347118273377419f, 0.0007298660930246115f, 0.0007398923626169562f, 0.0007116654887795448f, 0.0006288094446063042f, 0.0007764617912471294f, 0.0006964326021261513f, 0.0008400290389545262f, 0.0007456778548657894f, 0.0006950811948627234f, 0.000655798998195678f, 0.0006752326153218746f, 0.0006976264994591475f, 0.0006395179079845548f, 0.0005794059252366424f, 0.000770282931625843f, 0.0007760500302538276f, 0.0008222926990129054f, 0.0006765403668396175f, 0.0006354756187647581f, 0.000665209605358541f, 0.0006245407275855541f, 0.000825995986815542f, 0.0006846223259344697f, 0.0008531512576155365f, 0.0006475573754869401f, 0.0007038312032818794f, 0.0005803719977848232f, 0.0006389598711393774f, 0.0007068590493872762f, 0.0006094630225561559f, 0.0006636329926550388f, 0.0006134483264759183f, 0.0006476755952462554f, 0.0007807720103301108f, 0.0007682519499212503f, 0.0006566738011315465f);
static const ai_layer_format_type _res_res_0_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.020032256841659546f;
static const ai_i8 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 10;






static const ai_u32 _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.023293210193514824f;
static const ai_i8 _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 _res_res_1_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 38;

static const ai_u16 _res_res_1_conv1_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _res_res_1_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _res_res_1_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = -9;
static const ai_float _res_res_1_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.023293210193514824f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.01838567480444908f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0006059476872906089f, 0.0006699199439026415f, 0.0007016013259999454f, 0.0007132268510758877f, 0.0007162230322137475f, 0.0006227687117643654f, 0.0007243217551149428f, 0.0006477193674072623f, 0.0006214610766619444f, 0.0006725891144014895f, 0.0005799119244329631f, 0.0006320673855952919f, 0.0006503458134829998f, 0.0006398208788596094f, 0.0005845706909894943f, 0.0007148357690311968f, 0.0006758759845979512f, 0.0006022647139616311f, 0.000618764606770128f, 0.0008000179659575224f, 0.0006712102331221104f, 0.0006238887435756624f, 0.0006904519977979362f, 0.0007730607758276165f, 0.0006851931102573872f, 0.0006293341284617782f, 0.0008961466955952346f, 0.0007115201442502439f, 0.0009069701773114502f, 0.0007397633162327111f, 0.0006348692695610225f, 0.0006214352906681597f, 0.00064812385244295f, 0.0006797303212806582f, 0.0005777206970378757f, 0.0006214312743395567f, 0.0006255941698327661f, 0.0006855286774225533f, 0.0006647547124885023f, 0.0008631205419078469f, 0.0006170617998577654f, 0.000616077973973006f, 0.00060851318994537f, 0.0006010025972500443f, 0.0005827894783578813f, 0.0006578238680958748f, 0.000669369357638061f, 0.0005910911131650209f, 0.0006163937505334616f, 0.0008691682014614344f, 0.0006295541534200311f, 0.0006321610999293625f, 0.0006445691105909646f, 0.0005978931440040469f, 0.0008173736860044301f, 0.0007104150136001408f, 0.0007118159555830061f, 0.0007591714384034276f, 0.000755601329728961f, 0.0007737069972790778f, 0.0006998224416747689f, 0.0006106079672463238f, 0.0006325813010334969f, 0.00073074089596048f);
static const ai_layer_format_type _res_res_1_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.01838567480444908f;
static const ai_i8 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = -9;






static const ai_u32 _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.01758117415010929f;
static const ai_i8 _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -128;

static const ai_i8 _res_res_1_conv2_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-128);
static const ai_i16 _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 38;

static const ai_u16 _res_res_1_conv2_Conv_output_0_t_in_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_in_0_shape_h_const_u16 = 40;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_weight_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i8 _res_res_1_conv2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _res_res_1_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -18;
static const ai_float _res_res_1_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01758117415010929f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.01842520385980606f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0005965959280729294f, 0.0006538337911479175f, 0.0006930421222932637f, 0.0005911256303079426f, 0.0006627932307310402f, 0.0006004923488944769f, 0.0006928271031938493f, 0.0007416854496113956f, 0.0006109119858592749f, 0.0005835056072100997f, 0.0007522387895733118f, 0.0006120367324911058f, 0.0006370129995048046f, 0.0006607073592022061f, 0.0006455574184656143f, 0.0006862236768938601f, 0.0006207034457474947f, 0.0006183335208334029f, 0.0006036965060047805f, 0.0006999654578976333f, 0.0005797621561214328f, 0.0006384117295965552f, 0.0006828500190749764f, 0.0006181228091008961f, 0.0005864076083526015f, 0.0006884649628773332f, 0.0005942170391790569f, 0.0005899635143578053f, 0.0006066234200261533f, 0.0005870902095921338f, 0.0006174796144478023f, 0.0006972710834816098f, 0.0006571619305759668f, 0.0005889149033464491f, 0.0006321109249256551f, 0.0006915255216881633f, 0.0006886149640195072f, 0.0006908965879119933f, 0.000676231924444437f, 0.0005785454413853586f, 0.0006838474073447287f, 0.0006832176586613059f, 0.0006668618298135698f, 0.0006535273278132081f, 0.0006399981793947518f, 0.0009477284620516002f, 0.0006731670582666993f, 0.0007197396480478346f, 0.0005748613039031625f, 0.0007377039291895926f, 0.0005874523194506764f, 0.0006413553492166102f, 0.000675994495395571f, 0.0006982227205298841f, 0.000727541686501354f, 0.0008437680662609637f, 0.0006463797180913389f, 0.0005915127112530172f, 0.0005822156672365963f, 0.0005861087120138109f, 0.0005762125365436077f, 0.0006604730733670294f, 0.0006716472562402487f, 0.0006869277567602694f);
static const ai_layer_format_type _res_res_1_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.01842520385980606f;
static const ai_i8 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = -18;






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


  /* LITE_KERNEL_SECTION BEGIN curve_QuantizeLinear_Output */
  {
      const ai_float* curve_QuantizeLinear_Output_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_i8* curve_QuantizeLinear_Output_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 21976);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(3, 1, {(stai_ptr) curve_QuantizeLinear_Output_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(curve_QuantizeLinear_Output_t_in_0_ptr_const_f32, curve_QuantizeLinear_Output_t_out_0_ptr_s8, curve_QuantizeLinear_Output_t_out_0_shape_h_w_ch_d_prod_const_u32, curve_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32, curve_QuantizeLinear_Output_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(3, 1, {(stai_ptr) curve_QuantizeLinear_Output_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END curve_QuantizeLinear_Output */
  /* LITE_KERNEL_SECTION BEGIN curve_Transpose */
  {
    
  forward_lite_transpose_curve_Transpose_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END curve_Transpose */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_0_Conv_output_0 */
  {
      const ai_i8* _stem_stem_0_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 21076);
    const ai_i8* _stem_stem_0_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 8);
    const ai_i32* _stem_stem_0_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 680);
    ai_i8* _stem_stem_0_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 14400);
    ai_i16* _stem_stem_0_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_stem_stem_0_Conv_output_0_t_in_0_ptr_const_s8, _stem_stem_0_Conv_output_0_t_in_0_shape_w_const_u16, _stem_stem_0_Conv_output_0_t_in_0_shape_h_const_u16, _stem_stem_0_Conv_output_0_t_in_0_shape_ch_const_u16, _stem_stem_0_Conv_output_0_t_weight_0_ptr_const_s8, _stem_stem_0_Conv_output_0_t_out_0_shape_ch_const_u16, _stem_stem_0_Conv_output_0_t_weight_0_shape_w_const_u16, _stem_stem_0_Conv_output_0_t_weight_0_shape_h_const_u16, _stem_stem_0_Conv_output_0_l_stride_1_const_u16, _stem_stem_0_Conv_output_0_l_stride_0_const_u16, _stem_stem_0_Conv_output_0_l_pad_W_0_const_s32, _stem_stem_0_Conv_output_0_l_pad_H_0_const_s32, _stem_stem_0_Conv_output_0_t_weight_1_ptr_const_s32, _stem_stem_0_Conv_output_0_t_in_0_fmt_zero_const_s8, _stem_stem_0_Conv_output_0_t_out_0_fmt_zero_const_s8, _stem_stem_0_Conv_output_0_t_in_0_fmt_scale_const_f32, _stem_stem_0_Conv_output_0_t_out_0_fmt_scale_const_f32, _stem_stem_0_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stem_stem_0_Conv_output_0_l_out_ch_format_const_layer_format_type, _stem_stem_0_Conv_output_0_t_out_0_ptr_s8, _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u16, _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1876, _stem_stem_0_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_0_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_0_Conv_output_0_0_conversion */
  {
      const ai_i8* _stem_stem_0_Conv_output_0_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 14400);
    ai_float* _stem_stem_0_Conv_output_0_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_stem_stem_0_Conv_output_0_0_conversion_t_in_0_ptr_const_s8, _stem_stem_0_Conv_output_0_0_conversion_t_out_0_ptr_f32, _stem_stem_0_Conv_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_scale_const_f32, _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, {(stai_ptr) _stem_stem_0_Conv_output_0_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_0_Conv_output_0_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_1_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Erf_output_0 */
  {
      ai_handle _stem_stem_1_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(21, 1, {(stai_ptr) _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_1_Erf_output_0_t_out_0_ptr_handle, _stem_stem_1_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(21, 1, {(stai_ptr) _stem_stem_1_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Add_output_0 */
  {
      ai_float* _stem_stem_1_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_1_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 808);
    const ai_float* _stem_stem_1_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(22, 1, {(stai_ptr) _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_1_Add_output_0_t_out_0_ptr_f32, _stem_stem_1_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_1_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_1_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(4800), (ai_size)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(22, 1, {(stai_ptr) _stem_stem_1_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_1_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(23, 1, {(stai_ptr) _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(23, 1, {(stai_ptr) _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_2_Conv_output_0_pad_before */
  {
      const ai_ptr _stem_stem_2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
    ai_ptr _stem_stem_2_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 4800);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_stem_stem_2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _stem_stem_2_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_stem_stem_2_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _stem_stem_2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _stem_stem_2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(32), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_2_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_2_Conv_output_0 */
  {
      const ai_i8* _stem_stem_2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 4800);
    const ai_i8* _stem_stem_2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 1064);
    const ai_i32* _stem_stem_2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 11304);
    ai_i8* _stem_stem_2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_i16* _stem_stem_2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_stem_stem_2_Conv_output_0_t_in_0_ptr_const_s8, _stem_stem_2_Conv_output_0_t_in_0_shape_w_const_u16, _stem_stem_2_Conv_output_0_t_in_0_shape_h_const_u16, _stem_stem_2_Conv_output_0_t_in_0_shape_ch_const_u16, _stem_stem_2_Conv_output_0_t_weight_0_ptr_const_s8, _stem_stem_2_Conv_output_0_t_out_0_shape_ch_const_u16, _stem_stem_2_Conv_output_0_t_weight_0_shape_w_const_u16, _stem_stem_2_Conv_output_0_t_weight_0_shape_h_const_u16, _stem_stem_2_Conv_output_0_l_stride_1_const_u16, _stem_stem_2_Conv_output_0_l_stride_0_const_u16, _stem_stem_2_Conv_output_0_t_weight_1_ptr_const_s32, _stem_stem_2_Conv_output_0_t_in_0_fmt_zero_const_s8, _stem_stem_2_Conv_output_0_t_out_0_fmt_zero_const_s8, _stem_stem_2_Conv_output_0_t_in_0_fmt_scale_const_f32, _stem_stem_2_Conv_output_0_t_out_0_fmt_scale_const_f32, _stem_stem_2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stem_stem_2_Conv_output_0_l_out_ch_format_const_layer_format_type, _stem_stem_2_Conv_output_0_t_out_0_ptr_s8, _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u16, _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6656, _stem_stem_2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion */
  {
      const ai_i8* _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_ptr_const_s8, _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_ptr_f32, _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_scale_const_f32, _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(26, 1, {(stai_ptr) _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_3_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Erf_output_0 */
  {
      ai_handle _stem_stem_3_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 0);
    const ai_handle _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(30, 1, {(stai_ptr) _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_3_Erf_output_0_t_out_0_ptr_handle, _stem_stem_3_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(30, 1, {(stai_ptr) _stem_stem_3_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Add_output_0 */
  {
      ai_float* _stem_stem_3_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _stem_stem_3_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 11560);
    const ai_float* _stem_stem_3_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 11816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(31, 1, {(stai_ptr) _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_3_Add_output_0_t_out_0_ptr_f32, _stem_stem_3_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_3_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_3_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(4800), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(31, 1, {(stai_ptr) _stem_stem_3_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_3_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19200);
    ai_i8* _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(32, 1, {(stai_ptr) _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(32, 1, {(stai_ptr) _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_4_Conv_output_0_pad_before */
  {
      const ai_ptr _stem_stem_4_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
    ai_ptr _stem_stem_4_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 4800);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_stem_stem_4_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _stem_stem_4_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_stem_stem_4_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _stem_stem_4_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _stem_stem_4_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(64), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_4_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_4_Conv_output_0 */
  {
      const ai_i8* _stem_stem_4_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 4800);
    const ai_i8* _stem_stem_4_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 12072);
    const ai_i32* _stem_stem_4_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 24360);
    ai_i8* _stem_stem_4_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_i16* _stem_stem_4_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_stem_stem_4_Conv_output_0_t_in_0_ptr_const_s8, _stem_stem_4_Conv_output_0_t_in_0_shape_w_const_u16, _stem_stem_4_Conv_output_0_t_in_0_shape_h_const_u16, _stem_stem_4_Conv_output_0_t_in_0_shape_ch_const_u16, _stem_stem_4_Conv_output_0_t_weight_0_ptr_const_s8, _stem_stem_4_Conv_output_0_t_out_0_shape_ch_const_u16, _stem_stem_4_Conv_output_0_t_weight_0_shape_w_const_u16, _stem_stem_4_Conv_output_0_t_weight_0_shape_h_const_u16, _stem_stem_4_Conv_output_0_l_stride_1_const_u16, _stem_stem_4_Conv_output_0_l_stride_0_const_u16, _stem_stem_4_Conv_output_0_t_weight_1_ptr_const_s32, _stem_stem_4_Conv_output_0_t_in_0_fmt_zero_const_s8, _stem_stem_4_Conv_output_0_t_out_0_fmt_zero_const_s8, _stem_stem_4_Conv_output_0_t_in_0_fmt_scale_const_f32, _stem_stem_4_Conv_output_0_t_out_0_fmt_scale_const_f32, _stem_stem_4_Conv_output_0_t_weight_0_fmt_scale_const_f32, _stem_stem_4_Conv_output_0_l_out_ch_format_const_layer_format_type, _stem_stem_4_Conv_output_0_t_out_0_ptr_s8, _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u16, _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6784, _stem_stem_4_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_4_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion */
  {
      const ai_i8* _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 2432);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_ptr_const_s8, _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_ptr_f32, _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_scale_const_f32, _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(35, 1, {(stai_ptr) _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Div_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_5_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Erf_output_0 */
  {
      ai_handle _stem_stem_5_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 21888);
    const ai_handle _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 12160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(39, 1, {(stai_ptr) _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_stem_stem_5_Erf_output_0_t_out_0_ptr_handle, _stem_stem_5_Erf_output_0_t_in_0_ptr_const_handle, _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(39, 1, {(stai_ptr) _stem_stem_5_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Add_output_0 */
  {
      ai_float* _stem_stem_5_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 12160);
    const ai_float* _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 21888);
    const ai_float* _stem_stem_5_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 11560);
    const ai_float* _stem_stem_5_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 11816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(40, 1, {(stai_ptr) _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_stem_stem_5_Add_output_0_t_out_0_ptr_f32, _stem_stem_5_Add_output_0_t_in_0_ptr_const_f32, _stem_stem_5_Add_output_0_t_weight_0_ptr_const_f32, _stem_stem_5_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(40, 1, {(stai_ptr) _stem_stem_5_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Mul_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_5_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Mul_1_output_0 */
  {
    
  forward_lite_eltwise__stem_stem_5_Mul_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Mul_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(42, 1, {(stai_ptr) _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(42, 1, {(stai_ptr) _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv1_Conv_output_0_pad_before */
  {
      const ai_ptr _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 9728);
    ai_ptr _res_res_0_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 12160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(45, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_res_res_0_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _res_res_0_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_res_res_0_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _res_res_0_conv1_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(64), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(45, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_conv1_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv1_Conv_output_0 */
  {
      const ai_i8* _res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 12160);
    const ai_i8* _res_res_0_conv1_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 24616);
    const ai_i32* _res_res_0_conv1_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 36904);
    ai_i8* _res_res_0_conv1_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
    ai_i16* _res_res_0_conv1_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 14720);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(45, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_res_res_0_conv1_Conv_output_0_t_in_0_ptr_const_s8, _res_res_0_conv1_Conv_output_0_t_in_0_shape_w_const_u16, _res_res_0_conv1_Conv_output_0_t_in_0_shape_h_const_u16, _res_res_0_conv1_Conv_output_0_t_in_0_shape_ch_const_u16, _res_res_0_conv1_Conv_output_0_t_weight_0_ptr_const_s8, _res_res_0_conv1_Conv_output_0_t_out_0_shape_ch_const_u16, _res_res_0_conv1_Conv_output_0_t_weight_0_shape_w_const_u16, _res_res_0_conv1_Conv_output_0_t_weight_0_shape_h_const_u16, _res_res_0_conv1_Conv_output_0_l_stride_1_const_u16, _res_res_0_conv1_Conv_output_0_l_stride_0_const_u16, _res_res_0_conv1_Conv_output_0_t_weight_1_ptr_const_s32, _res_res_0_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8, _res_res_0_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8, _res_res_0_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32, _res_res_0_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32, _res_res_0_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32, _res_res_0_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type, _res_res_0_conv1_Conv_output_0_t_out_0_ptr_s8, _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u16, _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6784, _res_res_0_conv1_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(45, 1, {(stai_ptr) _res_res_0_conv1_Conv_output_0_t_out_0_ptr_s8});
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
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion */
  {
      const ai_i8* _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
    ai_float* _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 12160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(48, 1, {(stai_ptr) _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8, _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32, _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32, _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(48, 1, {(stai_ptr) _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion */
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
    const ai_float* _res_res_0_norm1_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 37224);
    const ai_float* _res_res_0_norm1_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 37480);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(51, 1, {(stai_ptr) _res_res_0_norm1_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_0_norm1_Mul_output_0_t_out_0_ptr_f32, _res_res_0_norm1_Mul_output_0_t_in_0_ptr_const_f32, _res_res_0_norm1_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_0_norm1_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(51, 1, {(stai_ptr) _res_res_0_norm1_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_0_norm1_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    ai_i8* _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(52, 1, {(stai_ptr) _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(52, 1, {(stai_ptr) _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv2_Conv_output_0_pad_before */
  {
      const ai_ptr _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 9728);
    ai_ptr _res_res_0_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 12160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(56, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_res_res_0_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _res_res_0_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_res_res_0_conv2_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _res_res_0_conv2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(64), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(56, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_conv2_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_conv2_Conv_output_0 */
  {
      const ai_i8* _res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 12160);
    const ai_i8* _res_res_0_conv2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 38248);
    const ai_i32* _res_res_0_conv2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 50536);
    ai_i8* _res_res_0_conv2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
    ai_i16* _res_res_0_conv2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 14720);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(56, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_res_res_0_conv2_Conv_output_0_t_in_0_ptr_const_s8, _res_res_0_conv2_Conv_output_0_t_in_0_shape_w_const_u16, _res_res_0_conv2_Conv_output_0_t_in_0_shape_h_const_u16, _res_res_0_conv2_Conv_output_0_t_in_0_shape_ch_const_u16, _res_res_0_conv2_Conv_output_0_t_weight_0_ptr_const_s8, _res_res_0_conv2_Conv_output_0_t_out_0_shape_ch_const_u16, _res_res_0_conv2_Conv_output_0_t_weight_0_shape_w_const_u16, _res_res_0_conv2_Conv_output_0_t_weight_0_shape_h_const_u16, _res_res_0_conv2_Conv_output_0_l_stride_1_const_u16, _res_res_0_conv2_Conv_output_0_l_stride_0_const_u16, _res_res_0_conv2_Conv_output_0_t_weight_1_ptr_const_s32, _res_res_0_conv2_Conv_output_0_t_in_0_fmt_zero_const_s8, _res_res_0_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8, _res_res_0_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32, _res_res_0_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32, _res_res_0_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _res_res_0_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type, _res_res_0_conv2_Conv_output_0_t_out_0_ptr_s8, _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u16, _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6784, _res_res_0_conv2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(56, 1, {(stai_ptr) _res_res_0_conv2_Conv_output_0_t_out_0_ptr_s8});
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
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion */
  {
      const ai_i8* _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
    ai_float* _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 12160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(59, 1, {(stai_ptr) _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8, _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32, _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32, _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(59, 1, {(stai_ptr) _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion */
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
      ai_float* _res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 50792);
    const ai_float* _res_res_0_norm2_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 51048);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(63, 1, {(stai_ptr) _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32, _res_res_0_norm2_Mul_output_0_t_in_0_ptr_const_f32, _res_res_0_norm2_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_0_norm2_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(63, 1, {(stai_ptr) _res_res_0_norm2_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_norm2_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_0_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 19456);
    ai_i8* _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(64, 1, {(stai_ptr) _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(64, 1, {(stai_ptr) _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv1_Conv_output_0_pad_before */
  {
      const ai_ptr _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
    ai_ptr _res_res_1_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 2432);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(68, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_res_res_1_conv1_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _res_res_1_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_res_res_1_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _res_res_1_conv1_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(64), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(68, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_conv1_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv1_Conv_output_0 */
  {
      const ai_i8* _res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 2432);
    const ai_i8* _res_res_1_conv1_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 51304);
    const ai_i32* _res_res_1_conv1_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 63592);
    ai_i8* _res_res_1_conv1_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_i16* _res_res_1_conv1_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 4992);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(68, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_res_res_1_conv1_Conv_output_0_t_in_0_ptr_const_s8, _res_res_1_conv1_Conv_output_0_t_in_0_shape_w_const_u16, _res_res_1_conv1_Conv_output_0_t_in_0_shape_h_const_u16, _res_res_1_conv1_Conv_output_0_t_in_0_shape_ch_const_u16, _res_res_1_conv1_Conv_output_0_t_weight_0_ptr_const_s8, _res_res_1_conv1_Conv_output_0_t_out_0_shape_ch_const_u16, _res_res_1_conv1_Conv_output_0_t_weight_0_shape_w_const_u16, _res_res_1_conv1_Conv_output_0_t_weight_0_shape_h_const_u16, _res_res_1_conv1_Conv_output_0_l_stride_1_const_u16, _res_res_1_conv1_Conv_output_0_l_stride_0_const_u16, _res_res_1_conv1_Conv_output_0_t_weight_1_ptr_const_s32, _res_res_1_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8, _res_res_1_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8, _res_res_1_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32, _res_res_1_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32, _res_res_1_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32, _res_res_1_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type, _res_res_1_conv1_Conv_output_0_t_out_0_ptr_s8, _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u16, _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6784, _res_res_1_conv1_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(68, 1, {(stai_ptr) _res_res_1_conv1_Conv_output_0_t_out_0_ptr_s8});
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
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion */
  {
      const ai_i8* _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_float* _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(71, 1, {(stai_ptr) _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8, _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32, _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32, _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(71, 1, {(stai_ptr) _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion */
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
    const ai_float* _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _res_res_1_norm1_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 63848);
    const ai_float* _res_res_1_norm1_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 37480);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(74, 1, {(stai_ptr) _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_1_norm1_Mul_output_0_t_out_0_ptr_f32, _res_res_1_norm1_Mul_output_0_t_in_0_ptr_const_f32, _res_res_1_norm1_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_1_norm1_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(74, 1, {(stai_ptr) _res_res_1_norm1_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_1_norm1_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion */
  {
      const ai_float* _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(75, 1, {(stai_ptr) _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_in_0_ptr_const_f32, _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8, _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32, _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(75, 1, {(stai_ptr) _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv2_Conv_output_0_pad_before */
  {
      const ai_ptr _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 9728);
    ai_ptr _res_res_1_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(79, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_res_res_1_conv2_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _res_res_1_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_res_res_1_conv2_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _res_res_1_conv2_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(64), (ai_i32)(64), (ai_i32)(64), (ai_i32)(0), (ai_i32)(0));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(79, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_conv2_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_conv2_Conv_output_0 */
  {
      const ai_i8* _res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    const ai_i8* _res_res_1_conv2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 64104);
    const ai_i32* _res_res_1_conv2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 76392);
    ai_i8* _res_res_1_conv2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 9344);
    ai_i16* _res_res_1_conv2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 2560);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(79, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_res_res_1_conv2_Conv_output_0_t_in_0_ptr_const_s8, _res_res_1_conv2_Conv_output_0_t_in_0_shape_w_const_u16, _res_res_1_conv2_Conv_output_0_t_in_0_shape_h_const_u16, _res_res_1_conv2_Conv_output_0_t_in_0_shape_ch_const_u16, _res_res_1_conv2_Conv_output_0_t_weight_0_ptr_const_s8, _res_res_1_conv2_Conv_output_0_t_out_0_shape_ch_const_u16, _res_res_1_conv2_Conv_output_0_t_weight_0_shape_w_const_u16, _res_res_1_conv2_Conv_output_0_t_weight_0_shape_h_const_u16, _res_res_1_conv2_Conv_output_0_l_stride_1_const_u16, _res_res_1_conv2_Conv_output_0_l_stride_0_const_u16, _res_res_1_conv2_Conv_output_0_t_weight_1_ptr_const_s32, _res_res_1_conv2_Conv_output_0_t_in_0_fmt_zero_const_s8, _res_res_1_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8, _res_res_1_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32, _res_res_1_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32, _res_res_1_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _res_res_1_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type, _res_res_1_conv2_Conv_output_0_t_out_0_ptr_s8, _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u16, _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6784, _res_res_1_conv2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(79, 1, {(stai_ptr) _res_res_1_conv2_Conv_output_0_t_out_0_ptr_s8});
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
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion */
  {
      const ai_i8* _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 2432);
    ai_float* _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(82, 1, {(stai_ptr) _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_ptr_const_s8, _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32, _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32, _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(82, 1, {(stai_ptr) _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion */
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
      ai_float* _res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 9728);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 76648);
    const ai_float* _res_res_1_norm2_Mul_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 76904);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(86, 1, {(stai_ptr) _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32, _res_res_1_norm2_Mul_output_0_t_in_0_ptr_const_f32, _res_res_1_norm2_Mul_output_0_t_weight_0_ptr_const_f32, _res_res_1_norm2_Mul_output_0_t_weight_1_ptr_const_f32, (ai_u32)(2432), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(86, 1, {(stai_ptr) _res_res_1_norm2_Mul_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _res_res_1_norm2_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_Add_output_0 */
  {
    
  forward_lite_eltwise__res_res_1_Add_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _res_res_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _res_res_1_Relu_1_output_0 */
  {
      ai_handle _res_res_1_Relu_1_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 9728);
    const ai_handle _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(88, 1, {(stai_ptr) _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_relu_if32of32(_res_res_1_Relu_1_output_0_t_out_0_ptr_handle, _res_res_1_Relu_1_output_0_t_in_0_ptr_const_handle, _res_res_1_Relu_1_output_0_t_in_0_shape_ch_h_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(88, 1, {(stai_ptr) _res_res_1_Relu_1_output_0_t_out_0_ptr_handle});
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
  net_ctx->_inputs[0] = activations[0] + 22876;

  net_ctx->_outputs[0] = activations[0] + 0;
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

