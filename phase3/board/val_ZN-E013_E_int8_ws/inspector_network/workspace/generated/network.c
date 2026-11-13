/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:32:05+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x88b13678d4e8c47e50a022a53c6be9e4"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:32:05+0700"
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
    AI_PACK_INTQ_SCALE(0.0847187489271164f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(curve_Transpose_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0847187489271164f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00909456331282854f),
    AI_PACK_INTQ_ZP(19)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00909456331282854f),
    AI_PACK_INTQ_ZP(19)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00909456331282854f),
    AI_PACK_INTQ_ZP(19)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015120810829102993f),
    AI_PACK_INTQ_ZP(1)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015120810829102993f),
    AI_PACK_INTQ_ZP(1)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_0_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015120810829102993f),
    AI_PACK_INTQ_ZP(1)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv1_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014488385990262032f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014488385990262032f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm1_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014488385990262032f),
    AI_PACK_INTQ_ZP(-27)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_conv2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017388399690389633f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017388399690389633f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_res_res_1_norm2_Reshape_output_0_to_chfirst_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017388399690389633f),
    AI_PACK_INTQ_ZP(-4)))



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
static const ai_float curve_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32 = 0.0847187489271164f;
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
static const ai_i8 _stem_stem_0_Conv_output_0_t_out_0_fmt_zero_const_s8 = 2;
static const ai_float _stem_stem_0_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0847187489271164f;
static const ai_float _stem_stem_0_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07890111207962036f;
static const ai_float _stem_stem_0_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017450862796977162f, 0.0014521178090944886f, 0.0017182445153594017f, 0.0017279795138165355f, 0.0016224839491769671f, 0.00168073910754174f, 0.001647189841605723f, 0.001597322174347937f, 0.0017916037468239665f, 0.0016486970707774162f, 0.001643733587116003f, 0.0016893501160666347f, 0.0017761136405169964f, 0.0016996203921735287f, 0.0015140355098992586f, 0.0017059787642210722f, 0.0016936426982283592f, 0.0017601262079551816f, 0.0016978011699393392f, 0.001683868933469057f, 0.0016184498090296984f, 0.0016395874554291368f, 0.0014635678380727768f, 0.0017430622829124331f, 0.001756810350343585f, 0.001720221247524023f, 0.0017217750428244472f, 0.0017001533415168524f, 0.0016427038935944438f, 0.0016984279500320554f, 0.0016662046546116471f, 0.0016375976847484708f);
static const ai_layer_format_type _stem_stem_0_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_0_Conv_output_0_t_out_0_shape_h_const_u16 = 150;

static const ai_u32 _stem_stem_0_Conv_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.07890111207962036f;
static const ai_i8 _stem_stem_0_Conv_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 2;


static const ai_i32 _stem_stem_1_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_1_Mul_output_0_0_0__stem_stem_2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.0785495936870575f;
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
static const ai_i8 _stem_stem_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -16;
static const ai_float _stem_stem_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0785495936870575f;
static const ai_float _stem_stem_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.034803591668605804f;
static const ai_float _stem_stem_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00034104075166396797f, 0.0003343361313454807f, 0.000361663376679644f, 0.0003653154708445072f, 0.0003409883356653154f, 0.00031547684920951724f, 0.0003404764283914119f, 0.0003443322784733027f, 0.0003199545608367771f, 0.0003104418283328414f, 0.0003393715887796134f, 0.0005118148401379585f, 0.000322394393151626f, 0.00033211559639312327f, 0.0004037698090542108f, 0.00031066813971847296f, 0.0003693446342367679f, 0.00036715439637191594f, 0.00037375843385234475f, 0.0003305479767732322f, 0.0003446234331931919f, 0.0003837311815004796f, 0.00032224453752860427f, 0.00033334415638819337f, 0.00033480714773759246f, 0.0003298793744761497f, 0.0003297592047601938f, 0.0004108531284146011f, 0.00042739525088109076f, 0.000321007362799719f, 0.00034314498770982027f, 0.00034635511110536754f, 0.0003508100635372102f, 0.000400417426135391f, 0.0003935708082281053f, 0.0003139487816952169f, 0.0003167220565956086f, 0.0003194769087713212f, 0.0003339137474540621f, 0.00037389551289379597f, 0.000353105366230011f, 0.0003247224958613515f, 0.00039849933818913996f, 0.00034384350874461234f, 0.0003258561191614717f, 0.00036092029768042266f, 0.00034739423426799476f, 0.00038016069447621703f, 0.00033291324507445097f, 0.0003357917012181133f, 0.00035027688136324286f, 0.0004101767553947866f, 0.0003915104316547513f, 0.0003586449602153152f, 0.00031062751077115536f, 0.0003899642324540764f, 0.0003857247647829354f, 0.00042054185178130865f, 0.0003798354009632021f, 0.00036283116787672043f, 0.00038288073847070336f, 0.0003426971088629216f, 0.0003702443791553378f, 0.0003117748419754207f);
static const ai_layer_format_type _stem_stem_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_2_Conv_output_0_t_out_0_shape_h_const_u16 = 75;

static const ai_u32 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.034803591668605804f;
static const ai_i8 _stem_stem_2_Conv_output_0_0_0__stem_stem_3_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = -16;


static const ai_i32 _stem_stem_3_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 4800;



static const ai_u32 _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 4800;
static const ai_float _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.0403890535235405f;
static const ai_i8 _stem_stem_3_Mul_output_0_0_0__stem_stem_4_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -120;

static const ai_i8 _stem_stem_4_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-120);
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
static const ai_i8 _stem_stem_4_Conv_output_0_t_in_0_fmt_zero_const_s8 = -120;
static const ai_i8 _stem_stem_4_Conv_output_0_t_out_0_fmt_zero_const_s8 = -21;
static const ai_float _stem_stem_4_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0403890535235405f;
static const ai_float _stem_stem_4_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.01829615980386734f;
static const ai_float _stem_stem_4_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00029443507082760334f, 0.0003059783484786749f, 0.0003071078099310398f, 0.0003419203276280314f, 0.00030484673334285617f, 0.0003036410198546946f, 0.0003097906883340329f, 0.0003041796444449574f, 0.00032866274705156684f, 0.0003141092602163553f, 0.0003290734894108027f, 0.0003211379225831479f, 0.0003086832002736628f, 0.00029619617271237075f, 0.0003215579781681299f, 0.0003061564057134092f, 0.0003328467719256878f, 0.0003319260140415281f, 0.0003476065176073462f, 0.0003127378586214036f, 0.00032562512205913663f, 0.0003158100589644164f, 0.0003148599644191563f, 0.0003229210851714015f, 0.00032069021835923195f, 0.00033980843727476895f, 0.0003347044112160802f, 0.0003237282217014581f, 0.0003078739682678133f, 0.00031592659070156515f, 0.00028864931664429605f, 0.00030530139338225126f, 0.00032602131250314415f, 0.00030650419648736715f, 0.0003238099452573806f, 0.0002924103173427284f, 0.0003205908287782222f, 0.0003387411998119205f, 0.0003129828255623579f, 0.000314104079734534f, 0.0003100116446148604f, 0.00030439585680142045f, 0.0003100140602327883f, 0.00029475436895154417f, 0.0003167108225170523f, 0.00031804858008399606f, 0.00033309022546745837f, 0.0004045636160299182f, 0.00031472573755308986f, 0.00031340878922492266f, 0.0003176455502398312f, 0.0002962569997180253f, 0.00031110073905438185f, 0.00031400335137732327f, 0.0002782578521873802f, 0.00032506766729056835f, 0.00030772972968406975f, 0.00030580360908061266f, 0.0002992379595525563f, 0.0002947339089587331f, 0.00032218341948464513f, 0.00029223886667750776f, 0.0003130364348180592f, 0.0003989306860603392f);
static const ai_layer_format_type _stem_stem_4_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _stem_stem_4_Conv_output_0_t_out_0_shape_h_const_u16 = 38;

static const ai_u32 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.01829615980386734f;
static const ai_i8 _stem_stem_4_Conv_output_0_0_0__stem_stem_5_Div_output_0_conversion_t_in_0_fmt_zero_const_s8 = -21;


static const ai_i32 _stem_stem_5_Erf_output_0_t_in_0_shape_ch_h_prod_const_s32 = 2432;




static const ai_u32 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.01124605257064104f;
static const ai_i8 _stem_stem_5_Mul_1_output_0_0_0__res_res_0_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_zero_const_s8 = -113;

static const ai_i8 _res_res_0_conv1_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-113);
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
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_in_0_fmt_zero_const_s8 = -113;
static const ai_i8 _res_res_0_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = 19;
static const ai_float _res_res_0_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01124605257064104f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.00909456331282854f;
static const ai_float _res_res_0_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0005840035737492144f, 0.0007267555338330567f, 0.0007202444248832762f, 0.0006182878860272467f, 0.0005725264200009406f, 0.0005849302979186177f, 0.0005956561071798205f, 0.000593343866057694f, 0.0006158484029583633f, 0.0005724321235902607f, 0.0006196516915224493f, 0.0006204432575032115f, 0.0005903407582081854f, 0.0007075842004269361f, 0.0006919438019394875f, 0.0006066838395781815f, 0.000641814898699522f, 0.00059713568771258f, 0.0006260132067836821f, 0.0006193571025505662f, 0.0005859195371158421f, 0.0006021672743372619f, 0.0005931159830652177f, 0.0005882076802663505f, 0.0005623880424536765f, 0.0006223917589522898f, 0.0005981269059702754f, 0.000589732953812927f, 0.0006317653460428119f, 0.0006827966426499188f, 0.0007427845848724246f, 0.0006129028042778373f, 0.0005733606521971524f, 0.0006063491455279291f, 0.0005785560933873057f, 0.0005905446596443653f, 0.000584762601647526f, 0.0005965367308817804f, 0.0005912218475714326f, 0.0005791353760287166f, 0.0005978131084702909f, 0.0006356444209814072f, 0.0005776268662884831f, 0.0006363752763718367f, 0.0005878984811715782f, 0.000603954482357949f, 0.0006078056758269668f, 0.0006325013237074018f, 0.0005904066492803395f, 0.0006679322686977684f, 0.0006022753659635782f, 0.0005961501155979931f, 0.0006101765320636332f, 0.0005872611654922366f, 0.0005829849396832287f, 0.0006922160391695797f, 0.0006516596185974777f, 0.0005772564327344298f, 0.00057862454559654f, 0.0006126195075921714f, 0.0006802519783377647f, 0.0006080955499783158f, 0.0005955101805739105f, 0.0006966898799873888f);
static const ai_layer_format_type _res_res_0_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.00909456331282854f;
static const ai_i8 _res_res_0_norm1_Reshape_output_0_to_chfirst_0_0__res_res_0_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 19;






static const ai_u32 _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm1_Add_output_0_0_0__res_res_0_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.01349214930087328f;
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
static const ai_i8 _res_res_0_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = 1;
static const ai_float _res_res_0_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01349214930087328f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.015120810829102993f;
static const ai_float _res_res_0_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00065791787346825f, 0.0006420356221497059f, 0.0007134079933166504f, 0.0006129080429673195f, 0.0006922868196852505f, 0.0006393424118869007f, 0.0006009845528751612f, 0.0006179417250677943f, 0.0005877699004486203f, 0.0006080662715248764f, 0.0007325828191824257f, 0.0005930059123784304f, 0.0006691762828268111f, 0.0006858808919787407f, 0.0005953339859843254f, 0.0006061086896806955f, 0.0006608281400986016f, 0.0006342863198369741f, 0.00061078678118065f, 0.0007876644958741963f, 0.000715897767804563f, 0.000626161927357316f, 0.0006095948629081249f, 0.0006539957830682397f, 0.0006175396265462041f, 0.0007381678442470729f, 0.0005753241712227464f, 0.0007116818451322615f, 0.0005843670223839581f, 0.0005827904678881168f, 0.0006422357400879264f, 0.0006510202074423432f, 0.0007816984434612095f, 0.0006291894824244082f, 0.0005939950351603329f, 0.0007116748602129519f, 0.0006124053034000099f, 0.0006046612979844213f, 0.0006355535006150603f, 0.0006797467940486968f, 0.0006318310042843223f, 0.0006737535004504025f, 0.00067850825143978f, 0.0007765233749523759f, 0.0006075632991269231f, 0.0006360590923577547f, 0.0005759494961239398f, 0.0006967485533095896f, 0.0006578871980309486f, 0.0007312826346606016f, 0.0006506983190774918f, 0.0007029465050436556f, 0.0006355867371894419f, 0.0006974851712584496f, 0.0006171147688291967f, 0.0007159435772337019f, 0.0006239988142624497f, 0.0006800793344154954f, 0.0006421802099794149f, 0.0006092799594625831f, 0.0007500599604099989f, 0.0006492997636087239f, 0.0006212888401933014f, 0.000693121924996376f);
static const ai_layer_format_type _res_res_0_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_0_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.015120810829102993f;
static const ai_i8 _res_res_0_norm2_Reshape_output_0_to_chfirst_0_0__res_res_0_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = 1;






static const ai_u32 _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_0_Add_output_0_0_0__res_res_1_conv1_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.014524401165544987f;
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
static const ai_i8 _res_res_1_conv1_Conv_output_0_t_out_0_fmt_zero_const_s8 = -27;
static const ai_float _res_res_1_conv1_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.014524401165544987f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.014488385990262032f;
static const ai_float _res_res_1_conv1_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.000595636258367449f, 0.0006594819133169949f, 0.0006446613115258515f, 0.0007437322637997568f, 0.0007648937753401697f, 0.0006052840035408735f, 0.000762412790209055f, 0.0006670842994935811f, 0.0007801675237715244f, 0.0006890225922688842f, 0.0006390607450157404f, 0.0005909923929721117f, 0.0008311649435199797f, 0.0006662039668299258f, 0.0008661881438456476f, 0.0007020170451141894f, 0.0008155112736858428f, 0.0006778831593692303f, 0.0006222272641025484f, 0.0007972678868100047f, 0.0006264349794946611f, 0.0006315280334092677f, 0.0006594820297323167f, 0.0006327151786535978f, 0.000751232320908457f, 0.0007463065558113158f, 0.0006651886506006122f, 0.0006639948696829379f, 0.0006911711534485221f, 0.0008900893735699356f, 0.0006874623941257596f, 0.000593941833358258f, 0.0006501649040728807f, 0.0007086897967383265f, 0.0005878153024241328f, 0.0008205688209272921f, 0.0007407687371596694f, 0.0006422216538339853f, 0.0007327048224397004f, 0.0007436495507135987f, 0.0007152175530791283f, 0.0006424241000786424f, 0.0007027780520729721f, 0.0006221893709152937f, 0.0006608588737435639f, 0.0007113598403520882f, 0.0007551939343102276f, 0.000718906638212502f, 0.0006345834699459374f, 0.0007116859778761864f, 0.0007303260499611497f, 0.0006400885758921504f, 0.0006932772812433541f, 0.0005819442449137568f, 0.0007447749376296997f, 0.0007428689277730882f, 0.0006230549770407379f, 0.000681668461766094f, 0.0006168433465063572f, 0.000716666632797569f, 0.0006314616184681654f, 0.0006167434039525688f, 0.000649183988571167f, 0.000640491081867367f);
static const ai_layer_format_type _res_res_1_conv1_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv1_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.014488385990262032f;
static const ai_i8 _res_res_1_norm1_Reshape_output_0_to_chfirst_0_0__res_res_1_norm1_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = -27;






static const ai_u32 _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm1_Add_output_0_0_0__res_res_1_conv2_Conv_output_0_pad_before_conversion_t_out_0_fmt_scale_const_f32 = 0.015305640175938606f;
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
static const ai_i8 _res_res_1_conv2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -4;
static const ai_float _res_res_1_conv2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.015305640175938606f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.017388399690389633f;
static const ai_float _res_res_1_conv2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0007075900794006884f, 0.0005993861705064774f, 0.0007916517788544297f, 0.0008199409930966794f, 0.0008384648826904595f, 0.0006530032260343432f, 0.0006017958512529731f, 0.000818160769995302f, 0.0007130331941880286f, 0.0006737661897204816f, 0.0006027239141985774f, 0.0006098168669268489f, 0.0006858964334242046f, 0.000728970451746136f, 0.0008520889678038657f, 0.0007545851985923946f, 0.0005956838722340763f, 0.0007364454213529825f, 0.0006667525740340352f, 0.0007296713883988559f, 0.0005924753495492041f, 0.0008361689397133887f, 0.0007198645616881549f, 0.000706906896084547f, 0.0006695269839838147f, 0.0006512043764814734f, 0.0005566839245148003f, 0.0007994056795723736f, 0.0007727601914666593f, 0.0005928610917180777f, 0.0006093033589422703f, 0.0007052401779219508f, 0.0006955478456802666f, 0.0006968749221414328f, 0.0008308703545480967f, 0.0007788584916852415f, 0.000607672322075814f, 0.0007243078434839845f, 0.0006455712136812508f, 0.0006179420743137598f, 0.0007413417333737016f, 0.000638758996501565f, 0.0007265464519150555f, 0.0006167253013700247f, 0.0007572969770990312f, 0.0006556377629749477f, 0.0006649158312939107f, 0.0006826317403465509f, 0.0006442299927584827f, 0.0007939185597933829f, 0.0006353645003400743f, 0.0008632849203422666f, 0.0005924574797973037f, 0.0007403516792692244f, 0.0007557914941571653f, 0.0005782778607681394f, 0.000646148924715817f, 0.0006533034611493349f, 0.000584648281801492f, 0.0006656658370047808f, 0.0006851702928543091f, 0.0007013592403382063f, 0.0005988989723846316f, 0.0005861609824933112f);
static const ai_layer_format_type _res_res_1_conv2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_w_const_u16 = 1;
static const ai_u16 _res_res_1_conv2_Conv_output_0_t_out_0_shape_h_const_u16 = 38;



static const ai_u32 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 2432;
static const ai_float _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.017388399690389633f;
static const ai_i8 _res_res_1_norm2_Reshape_output_0_to_chfirst_0_0__res_res_1_norm2_InstanceNormalization_output_0_conversion_t_in_0_fmt_zero_const_s8 = -4;






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

