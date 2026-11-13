/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:31:40+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xf831272a921b27bb8f8c0c1c8fbdab3b"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:31:40+0700"
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
    AI_PACK_INTQ_SCALE(0.08212293684482574f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(curve_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08212293684482574f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007034171838313341f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007034171838313341f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.007034171838313341f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027399824932217598f),
    AI_PACK_INTQ_ZP(32)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027399824932217598f),
    AI_PACK_INTQ_ZP(32)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027399824932217598f),
    AI_PACK_INTQ_ZP(32)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021648453548550606f),
    AI_PACK_INTQ_ZP(-2)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021648453548550606f),
    AI_PACK_INTQ_ZP(-2)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021648453548550606f),
    AI_PACK_INTQ_ZP(-2)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020529065281152725f),
    AI_PACK_INTQ_ZP(23)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020529065281152725f),
    AI_PACK_INTQ_ZP(23)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020529065281152725f),
    AI_PACK_INTQ_ZP(23)))



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
static const ai_float curve_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32 = 0.08212293684482574f;
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
static const ai_i8 _stem_stem_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = 5;
static const ai_float _stem_stem_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.08212293684482574f;
static const ai_float _stem_stem_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07296039909124374f;
static const ai_float _stem_stem_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017952953930944204f, 0.0014246967621147633f, 0.0017830460565164685f, 0.0017727509839460254f, 0.001630409387871623f, 0.0016740424325689673f, 0.0016517712501809f, 0.0016003531636670232f, 0.0018711547600105405f, 0.0016073692822828889f, 0.0016415492864325643f, 0.0016666434239596128f, 0.0017750976840034127f, 0.0017029005102813244f, 0.0015220571076497436f, 0.0018347174627706409f, 0.0017297243466600776f, 0.0018064992036670446f, 0.0016564656980335712f, 0.0017300564795732498f, 0.0015782701084390283f, 0.0016670820768922567f, 0.0015086840139701962f, 0.0017368578119203448f, 0.001756252720952034f, 0.001792536466382444f, 0.001967968652024865f, 0.0017230728408321738f, 0.0016242384444922209f, 0.0017416536575183272f, 0.0017356084426864982f, 0.001702843583188951f);
static const ai_layer_format_type _stem_stem_0_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u16 = 150;

static const ai_u32 _stem_stem_0_Conv_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.07296039909124374f;
static const ai_i8 _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 5;


static const ai_i32 _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.07141376286745071f;
static const ai_i8 _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -123;

static const ai_i8 _stem_stem_2_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-123);
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
static const ai_i8 _stem_stem_2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _stem_stem_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -4;
static const ai_float _stem_stem_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.07141376286745071f;
static const ai_float _stem_stem_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03160155564546585f;
static const ai_float _stem_stem_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00039856566581875086f, 0.00036916989483870566f, 0.0004473252047318965f, 0.0004618712409865111f, 0.00038881791988387704f, 0.0003373389190528542f, 0.0003761633997783065f, 0.00039947853656485677f, 0.00037139852065593004f, 0.00037473635165952146f, 0.00038853203295730054f, 0.0005873562186025083f, 0.00033094309037551284f, 0.00034252641489729285f, 0.00032171086058951914f, 0.0004796198336407542f, 0.0003671701706480235f, 0.0003554190625436604f, 0.0003924113989342004f, 0.0004279350978322327f, 0.0003528126690071076f, 0.0004931690054945648f, 0.0003794397634919733f, 0.0004239558766130358f, 0.00038446459802798927f, 0.00035212523653171957f, 0.0003418338601477444f, 0.00044785416685044765f, 0.00046640183427371085f, 0.00041941439849324524f, 0.00040928617818281054f, 0.0003521715698298067f, 0.0004288279451429844f, 0.0003461479500401765f, 0.00031683468841947615f, 0.0004220192786306143f, 0.0004953474272042513f, 0.00038199342088773847f, 0.0003389996709302068f, 0.00036550319055095315f, 0.00034012008109129965f, 0.00033060662099160254f, 0.0003291957837063819f, 0.00037197605706751347f, 0.00038686199695803225f, 0.00034635671181604266f, 0.000354889256414026f, 0.000349028647178784f, 0.0003459874424152076f, 0.0004864501825068146f, 0.0005128202610649168f, 0.000332448398694396f, 0.0003881121228914708f, 0.0004911513533443213f, 0.0003503135230857879f, 0.0003956139844376594f, 0.0005048974417150021f, 0.0003468019422143698f, 0.00039626468787901103f, 0.00043149819248355925f, 0.00036035216180607677f, 0.00037400345900096f, 0.00039182076579891145f, 0.00034096132731065154f);
static const ai_layer_format_type _stem_stem_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u16 = 75;

static const ai_u32 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.03160155564546585f;
static const ai_i8 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = -4;


static const ai_i32 _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.033753421157598495f;
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
static const ai_i8 _stem_stem_4_Conv_output_0_t_out_0_fmt_zero_const_s8 = 19;
static const ai_float _stem_stem_4_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.033753421157598495f;
static const ai_float _stem_stem_4_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.01975473389029503f;
static const ai_float _stem_stem_4_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.000357316603185609f, 0.0004075741453561932f, 0.0003060776798520237f, 0.00029917762731201947f, 0.00030159385642036796f, 0.0003351783670950681f, 0.00032785735675133765f, 0.00035857921466231346f, 0.0003406819305382669f, 0.00030221042106859386f, 0.00030959496507421136f, 0.00038852973375469446f, 0.00030459664412774146f, 0.0003313348279334605f, 0.00033745571272447705f, 0.0003554474387783557f, 0.00033124329638667405f, 0.00032637675758451223f, 0.00038671749643981457f, 0.00031123790540732443f, 0.0003732570621650666f, 0.00035269747604615986f, 0.000339258840540424f, 0.0003082945477217436f, 0.0003892070963047445f, 0.0003328218590468168f, 0.000338857586029917f, 0.0003391526697669178f, 0.0003456945123616606f, 0.0003058795409742743f, 0.00031713800854049623f, 0.0002953797811642289f, 0.000357891054591164f, 0.00034591922303661704f, 0.00032453573658131063f, 0.0003190776042174548f, 0.0003092274710070342f, 0.00037974040606059134f, 0.000337571109412238f, 0.00038815755397081375f, 0.0003568505053408444f, 0.00031370652141049504f, 0.0003332385385874659f, 0.00033954158425331116f, 0.00032455872860737145f, 0.00036851316690444946f, 0.0004236097738612443f, 0.00030105392215773463f, 0.0003508420195430517f, 0.00032347478554584086f, 0.0004110365698579699f, 0.00032638892298564315f, 0.00030480779241770506f, 0.00037043672637082636f, 0.00029690362862311304f, 0.00033720757346600294f, 0.0003346751327626407f, 0.00032678176648914814f, 0.00032601249404251575f, 0.0003219977079425007f, 0.0003324457793496549f, 0.0003566746017895639f, 0.0003407957556191832f, 0.0003536090371198952f);
static const ai_layer_format_type _stem_stem_4_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u16 = 38;

static const ai_u32 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.01975473389029503f;
static const ai_i8 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = 19;


static const ai_i32 _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;




static const ai_u32 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.008915613405406475f;
static const ai_i8 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -109;

static const ai_i8 _res_res_0_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-109);
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
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8 = -109;
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = 3;
static const ai_float _res_res_0_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.008915613405406475f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.007034171838313341f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0006464396719820797f, 0.0006322364206425846f, 0.0006125057116150856f, 0.0006640709470957518f, 0.0005700424662791193f, 0.0007020192570053041f, 0.000621261540800333f, 0.0006116078002378345f, 0.0006310887401923537f, 0.000605653680395335f, 0.000683968944940716f, 0.000655321404337883f, 0.0006783008575439453f, 0.0005877063376829028f, 0.0006485491758212447f, 0.0006550983525812626f, 0.0006465889746323228f, 0.0006836226675659418f, 0.0006303853588178754f, 0.0006188799161463976f, 0.0006297457730397582f, 0.0005973788793198764f, 0.0006468126666732132f, 0.0006117419106885791f, 0.0006037095445208251f, 0.0006241330993361771f, 0.0006532638217322528f, 0.0006218464695848525f, 0.000696304370649159f, 0.0008803935488685966f, 0.0006555034196935594f, 0.0006419455748982728f, 0.0007434002473019063f, 0.0006283812690526247f, 0.0006620516651310027f, 0.000610095972660929f, 0.0005919858231209219f, 0.0006122075137682259f, 0.0006576935993507504f, 0.0007803242770023644f, 0.0006128457607701421f, 0.0007038931944407523f, 0.0006297426880337298f, 0.00062222545966506f, 0.0005982968723401427f, 0.000676840718369931f, 0.0006972076371312141f, 0.000684244791045785f, 0.0006471996894106269f, 0.0006582144997082651f, 0.0005928354221396148f, 0.0005916755762882531f, 0.0006654464523307979f, 0.0006602389039471745f, 0.000601700390689075f, 0.0007176665239967406f, 0.0007974529871717095f, 0.0006693871109746397f, 0.0006564685027115047f, 0.000625995104201138f, 0.000653263065032661f, 0.0006236245972104371f, 0.0006025817128829658f, 0.0006291183526627719f);
static const ai_layer_format_type _res_res_0_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.007034171838313341f;
static const ai_i8 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 3;






static const ai_u32 _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.023048780858516693f;
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
static const ai_i8 _res_res_0_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = 32;
static const ai_float _res_res_0_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.023048780858516693f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.027399824932217598f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0007678819820284843f, 0.0007764296024106443f, 0.0008177527342922986f, 0.0008346627000719309f, 0.0009022955782711506f, 0.0008275932632386684f, 0.0007214828510768712f, 0.0006853808299638331f, 0.0008957134559750557f, 0.000980420270934701f, 0.0007337018032558262f, 0.0009323965641669929f, 0.0007809271919541061f, 0.0007835307624191046f, 0.0007423689821735024f, 0.000653887924272567f, 0.0006356821395456791f, 0.0007853847346268594f, 0.000648766232188791f, 0.0006854443927295506f, 0.0010948864510282874f, 0.0008521043928340077f, 0.0007096921908669174f, 0.0006856885156594217f, 0.0009247261332347989f, 0.0007245413726195693f, 0.0007901350618340075f, 0.0007118536159396172f, 0.0006194457528181374f, 0.0006768478197045624f, 0.0008342345827259123f, 0.000799149158410728f, 0.0006762416451238096f, 0.0006241293740458786f, 0.0006114949355833232f, 0.0009580786572769284f, 0.0007297311676666141f, 0.000650186906568706f, 0.0006886624614708126f, 0.0006641647196374834f, 0.0007821189356036484f, 0.0008554300293326378f, 0.0006675690528936684f, 0.0009219006169587374f, 0.0008115518721751869f, 0.0010455375304445624f, 0.0009696232154965401f, 0.0007075218600220978f, 0.0007964159012772143f, 0.0009042822639457881f, 0.000736887683160603f, 0.0007794821867719293f, 0.0007292131194844842f, 0.0008702484774403274f, 0.0008823555544950068f, 0.0006971588591113687f, 0.0007675843662582338f, 0.0008925523143261671f, 0.0007685745367780328f, 0.000634568918030709f, 0.0007886661333031952f, 0.0006966883083805442f, 0.0007562608807347715f, 0.0007411760161630809f);
static const ai_layer_format_type _res_res_0_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.027399824932217598f;
static const ai_i8 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 32;






static const ai_u32 _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.020001860335469246f;
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
static const ai_i8 _res_res_1_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = -2;
static const ai_float _res_res_1_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.020001860335469246f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.021648453548550606f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.000708853651303798f, 0.0008097907993942499f, 0.0006981505430303514f, 0.0008002511458471417f, 0.0007002466008998454f, 0.000817086489405483f, 0.0007900792988948524f, 0.0007668290054425597f, 0.000686494167894125f, 0.0007840516045689583f, 0.0006892895326018333f, 0.0006797689129598439f, 0.0006524891359731555f, 0.0007980727823451161f, 0.0007352525717578828f, 0.0005980139831081033f, 0.0008964381995610893f, 0.0009151188423857093f, 0.0010220621479675174f, 0.000834314851090312f, 0.000611990864854306f, 0.0006482991157099605f, 0.0011015641503036022f, 0.001112850965000689f, 0.0007085382821969688f, 0.0006788599421270192f, 0.0006684647523798048f, 0.0006938535952940583f, 0.000785952724982053f, 0.0006838004919700325f, 0.0007143282564356923f, 0.0007495537283830345f, 0.0009494825499132276f, 0.0008109686896204948f, 0.0006658388301730156f, 0.0006883136811666191f, 0.0007395559805445373f, 0.0009315676288679242f, 0.0009000206482596695f, 0.0007188463350757957f, 0.00064536661375314f, 0.0009308854932896793f, 0.001013855915516615f, 0.0007174495840445161f, 0.0006726805004291236f, 0.0006895508267916739f, 0.0006674741161987185f, 0.0010867215460166335f, 0.0006113974377512932f, 0.0006746373255737126f, 0.0007831762777641416f, 0.00093805231153965f, 0.000738964939955622f, 0.0008291822159662843f, 0.0006915722624398768f, 0.0006760611431673169f, 0.0008830532897263765f, 0.0008190031512640417f, 0.0008370744180865586f, 0.0007199676474556327f, 0.000607362890150398f, 0.0006728167645633221f, 0.000817545922473073f, 0.000644092564471066f);
static const ai_layer_format_type _res_res_1_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.021648453548550606f;
static const ai_i8 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = -2;






static const ai_u32 _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.020968476310372353f;
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
static const ai_i8 _res_res_1_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = 23;
static const ai_float _res_res_1_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.020968476310372353f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.020529065281152725f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0007194414502009749f, 0.0007766148773953319f, 0.0007902919896878302f, 0.0006303712143562734f, 0.0006613427540287375f, 0.0006482195458374918f, 0.0006401315913535655f, 0.000626922061201185f, 0.0005908982711844146f, 0.000749498896766454f, 0.0007093104068189859f, 0.0009176317835226655f, 0.0008064007852226496f, 0.0006848929915577173f, 0.0006942058098502457f, 0.0006663638632744551f, 0.0006813985528424382f, 0.0006363795837387443f, 0.0008248367230407894f, 0.0008372639422304928f, 0.0007041044882498682f, 0.0006497076828964055f, 0.0007445078226737678f, 0.0006061939639039338f, 0.0007150667952373624f, 0.0006977964658290148f, 0.0006497452850453556f, 0.0006462504388764501f, 0.000705936283338815f, 0.0006347125745378435f, 0.000604377593845129f, 0.000678120763041079f, 0.0007315417169593275f, 0.0006149415276013315f, 0.0006205482059158385f, 0.0007585767307318747f, 0.0006850162171758711f, 0.000594444340094924f, 0.0006177512113936245f, 0.000750700302887708f, 0.0007029401022009552f, 0.0007654863293282688f, 0.0007062391377985477f, 0.000882438849657774f, 0.0006452277302742004f, 0.0007021508645266294f, 0.0007524089887738228f, 0.0006917010759934783f, 0.000802926835604012f, 0.0007040791097097099f, 0.000717445625923574f, 0.0008571845246478915f, 0.0007512803422287107f, 0.0006941347965039313f, 0.0006561354384757578f, 0.0007431379053741693f, 0.000721863703802228f, 0.0007529016584157944f, 0.0005847127176821232f, 0.0007060783100314438f, 0.00080606818664819f, 0.0006547354278154671f, 0.0008101938292384148f, 0.0006906490307301283f);
static const ai_layer_format_type _res_res_1_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.020529065281152725f;
static const ai_i8 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 23;






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

