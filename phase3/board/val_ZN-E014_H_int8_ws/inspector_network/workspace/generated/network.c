/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:36:01+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x96722757b719226df91132f9029ab6a7"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:36:01+0700"
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
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 192),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 1),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 1280),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 13132),
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
AI_INTQ_INFO_LIST_OBJ_DECLARE(summary_QuantizeLinear_Output_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.019368916749954224f),
    AI_PACK_INTQ_ZP(-75)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_summ_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02638871595263481f),
    AI_PACK_INTQ_ZP(19)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_summ_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0007035833550617099f, 0.0010564593831077218f, 0.000952411734033376f, 0.0012831324711441994f, 0.0009014388779178262f, 0.0007497663027606905f, 0.0008416534401476383f, 0.0007774073164910078f, 0.0006740991957485676f, 0.0007001530029810965f, 0.0010161611717194319f, 0.0008205863414332271f, 0.0009877628181129694f, 0.001105956151150167f, 0.000899030826985836f, 0.0011430536396801472f, 0.0007846725056879222f, 0.0015687948325648904f, 0.0010886219097301364f, 0.0011843180982396007f, 0.0007701131398789585f, 0.0007934353197924793f, 0.0008618544088676572f, 0.0006776749505661428f, 0.0006751651526428759f, 0.000757154484745115f, 0.0010756350820884109f, 0.000762134266551584f, 0.0013590259477496147f, 0.0007201377884484828f, 0.001338193891569972f, 0.0008843672112561762f, 0.0009976906003430486f, 0.0007165701244957745f, 0.000705129757989198f, 0.0007849704124964774f, 0.0006363261491060257f, 0.0013578766956925392f, 0.0014848285354673862f, 0.000705796992406249f, 0.0009596143499948084f, 0.0007675347733311355f, 0.0007892317371442914f, 0.0007853676215745509f, 0.0008754112641327083f, 0.0009276746422983706f, 0.0007078835042193532f, 0.0010493622394278646f, 0.000721836811862886f, 0.000765844015404582f, 0.000881594605743885f, 0.0007010333356447518f, 0.0006315578939393163f, 0.0007676394307054579f, 0.0011098405811935663f, 0.0008751327986828983f, 0.0010268368059769273f, 0.0011961540440097451f, 0.0016044389922171831f, 0.0011889555025845766f, 0.0006525896606035531f, 0.001295381924137473f, 0.0007090894505381584f, 0.0006605723174288869f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  summary_QuantizeLinear_Output_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  _summ_Gemm_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _summ_Gemm_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 12288, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  _summ_Gemm_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  _summ_Gemm_output_0_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 512, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  _summ_Gemm_output_0_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  _Div_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  _Constant_output_0_2D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  _Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  _Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  _Mul_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  _Constant_2_output_0_2D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  _summ_Gemm_output_0_bias, AI_STATIC,
  11, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_summ_Gemm_output_0_bias_array, NULL)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  _summ_Gemm_output_0_output, AI_STATIC,
  12, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_summ_Gemm_output_0_output_array, &_summ_Gemm_output_0_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  _summ_Gemm_output_0_scratch0, AI_STATIC,
  13, 0x0,
  AI_SHAPE_INIT(4, 1, 512, 1, 1), AI_STRIDE_INIT(4, 2, 2, 1024, 1024),
  1, &_summ_Gemm_output_0_scratch0_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _summ_Gemm_output_0_weights, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 192, 64, 1, 1), AI_STRIDE_INIT(4, 1, 192, 12288, 12288),
  1, &_summ_Gemm_output_0_weights_array, &_summ_Gemm_output_0_weights_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  summary_QuantizeLinear_Output_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 192, 1, 1), AI_STRIDE_INIT(4, 1, 1, 192, 192),
  1, &summary_QuantizeLinear_Output_output_array, &summary_QuantizeLinear_Output_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  _Constant_output_0_2D, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_Constant_output_0_2D_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _Div_output_0_output, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_Div_output_0_output_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _summ_Gemm_output_0_0_conversion_output, AI_STATIC,
  10, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_summ_Gemm_output_0_0_conversion_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _Add_output_0_output, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_Add_output_0_output_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _Mul_output_0_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_Mul_output_0_output_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _Constant_2_output_0_2D, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_Constant_2_output_0_2D_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _Mul_1_output_0_output, AI_STATIC,
  8, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_Mul_1_output_0_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  _summ_Gemm_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&summary_QuantizeLinear_Output_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_summ_Gemm_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_summ_Gemm_output_0_weights, (ai_tensor *)&_summ_Gemm_output_0_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_summ_Gemm_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _summ_Gemm_output_0_layer, 7,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &_summ_Gemm_output_0_chain,
  NULL, &_summ_Gemm_output_0_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_summ_Gemm_output_0_0_conversion_output, (ai_tensor *)&_Constant_output_0_2D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _Div_output_0_layer, 10,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_Div_output_0_chain,
  NULL, &_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_summ_Gemm_output_0_0_conversion_output, (ai_tensor *)&_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _Mul_output_0_layer, 13,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_Mul_output_0_chain,
  NULL, &_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _Mul_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_Mul_output_0_output, (ai_tensor *)&_Constant_2_output_0_2D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Mul_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _Mul_1_output_0_layer, 14,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_Mul_1_output_0_chain,
  NULL, &_Mul_1_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)
/**  Hybrid layers declarations section  *************************************/

LITE_FORCE_INLINE void forward_lite_dense_integer_SSSA_ch__summ_Gemm_output_0_network(_stai_network_context* net_ctx)
{
  summary_QuantizeLinear_Output_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  summary_QuantizeLinear_Output_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _summ_Gemm_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  _summ_Gemm_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  _summ_Gemm_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 12296);
  _summ_Gemm_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 12296);
  _summ_Gemm_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 192);
  _summ_Gemm_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 192);
  _summ_Gemm_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 1216);
  _summ_Gemm_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 1216);
  _STAI_NETWORK_EVENT_NODE_START_CB(7, 1, { summary_QuantizeLinear_Output_output.data->data});
  forward_dense_integer_SSSA_ch((ai_layer *)&_summ_Gemm_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(7, 1, { _summ_Gemm_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__Div_output_0_network(_stai_network_context* net_ctx)
{
  _summ_Gemm_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _summ_Gemm_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _Constant_output_0_2D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _Constant_output_0_2D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(10, 2, { _summ_Gemm_output_0_0_conversion_output.data->data,_Constant_output_0_2D.data->data});
  forward_eltwise((ai_layer *)&_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(10, 1, { _Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__Mul_output_0_network(_stai_network_context* net_ctx)
{
  _summ_Gemm_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _summ_Gemm_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 512);
  _Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 512);
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 2, { _summ_Gemm_output_0_0_conversion_output.data->data,_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, { _Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__Mul_1_output_0_network(_stai_network_context* net_ctx)
{
  _Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 512);
  _Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 512);
  _Constant_2_output_0_2D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  _Constant_2_output_0_2D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  _Mul_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _Mul_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 2, { _Mul_output_0_output.data->data,_Constant_2_output_0_2D.data->data});
  forward_eltwise((ai_layer *)&_Mul_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, { _Mul_1_output_0_output.data->data});
}

/*****************************************************************************/


static const ai_u32 summary_QuantizeLinear_Output_t_out_0_shape_h_w_ch_d_prod_const_u32 = 192;
static const ai_float summary_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32 = 0.019368916749954224f;
static const ai_i8 summary_QuantizeLinear_Output_t_out_0_fmt_zero_const_s8 = -75;


static const ai_u32 _summ_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _summ_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.02638871595263481f;
static const ai_i8 _summ_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 19;


static const ai_i32 _Erf_output_0_t_in_0_shape_ch_prod_const_s32 = 64;




static const ai_u32 _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_fmt_scale_const_f32 = 0.011787270195782185f;
static const ai_i8 _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_fmt_zero_const_s8 = -114;

static const ai_i8 y_QuantizeLinear_Input_t_in_0_fmt_zero_const_s8 = -114;
static const ai_i8 y_QuantizeLinear_Input_t_out_0_fmt_zero_const_s8 = -38;
static const ai_u16 y_QuantizeLinear_Input_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 y_QuantizeLinear_Input_t_out_0_shape_ch_const_u16 = 1;
static const ai_u32 y_QuantizeLinear_Input_t_out_0_shape_h_w_prod_const_u32 = 1;
static const ai_float y_QuantizeLinear_Input_t_in_0_fmt_scale_const_f32 = 0.011787270195782185f;
static const ai_float y_QuantizeLinear_Input_t_out_0_fmt_scale_const_f32 = 0.00740639865398407f;
static const ai_float y_QuantizeLinear_Input_t_weight_0_fmt_scale_const_f32 = 0.0009511242969892919f;

static const ai_u32 y_QuantizeLinear_Input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 1;
static const ai_float y_QuantizeLinear_Input_0_conversion_t_in_0_fmt_scale_const_f32 = 0.00740639865398407f;
static const ai_i8 y_QuantizeLinear_Input_0_conversion_t_in_0_fmt_zero_const_s8 = -38;
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


  /* LITE_KERNEL_SECTION BEGIN summary_QuantizeLinear_Output */
  {
      const ai_float* summary_QuantizeLinear_Output_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_i8* summary_QuantizeLinear_Output_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(5, 1, {(stai_ptr) summary_QuantizeLinear_Output_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(summary_QuantizeLinear_Output_t_in_0_ptr_const_f32, summary_QuantizeLinear_Output_t_out_0_ptr_s8, summary_QuantizeLinear_Output_t_out_0_shape_h_w_ch_d_prod_const_u32, summary_QuantizeLinear_Output_t_out_0_fmt_scale_const_f32, summary_QuantizeLinear_Output_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(5, 1, {(stai_ptr) summary_QuantizeLinear_Output_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END summary_QuantizeLinear_Output */
  /* LITE_KERNEL_SECTION BEGIN _summ_Gemm_output_0 */
  {
    
  forward_lite_dense_integer_SSSA_ch__summ_Gemm_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _summ_Gemm_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _summ_Gemm_output_0_0_conversion */
  {
      const ai_i8* _summ_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 1216);
    ai_float* _summ_Gemm_output_0_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(7, 1, {(stai_ptr) _summ_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_summ_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8, _summ_Gemm_output_0_0_conversion_t_out_0_ptr_f32, _summ_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _summ_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32, _summ_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(7, 1, {(stai_ptr) _summ_Gemm_output_0_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _summ_Gemm_output_0_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _Div_output_0 */
  {
    
  forward_lite_eltwise__Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Erf_output_0 */
  {
      ai_handle _Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 512);
    const ai_handle _Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 256);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(11, 1, {(stai_ptr) _Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_Erf_output_0_t_out_0_ptr_handle, _Erf_output_0_t_in_0_ptr_const_handle, _Erf_output_0_t_in_0_shape_ch_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(11, 1, {(stai_ptr) _Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Add_output_0 */
  {
      ai_float* _Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 256);
    const ai_float* _Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 512);
    const ai_float* _Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 12552);
    const ai_float* _Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 12808);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(12, 1, {(stai_ptr) _Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_Add_output_0_t_out_0_ptr_f32, _Add_output_0_t_in_0_ptr_const_f32, _Add_output_0_t_weight_0_ptr_const_f32, _Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(64), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(12, 1, {(stai_ptr) _Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Mul_output_0 */
  {
    
  forward_lite_eltwise__Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Mul_1_output_0 */
  {
    
  forward_lite_eltwise__Mul_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _Mul_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion */
  {
      const ai_float* _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 0);
    ai_i8* _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 256);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 1, {(stai_ptr) _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_in_0_ptr_const_f32, _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_ptr_s8, _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_fmt_scale_const_f32, _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, {(stai_ptr) _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _Mul_1_output_0_0_0_y_QuantizeLinear_Input_conversion */
  /* LITE_KERNEL_SECTION BEGIN y_QuantizeLinear_Input */
  {
      ai_i8* y_QuantizeLinear_Input_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 128);
    const ai_i8* y_QuantizeLinear_Input_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 256);
    const ai_i8* y_QuantizeLinear_Input_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 13064);
    const ai_i32* y_QuantizeLinear_Input_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 13128);
    ai_i16* y_QuantizeLinear_Input_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 1, {(stai_ptr) y_QuantizeLinear_Input_t_in_0_ptr_const_s8});
    
  forward_lite_dense_is8os8ws8(y_QuantizeLinear_Input_t_out_0_ptr_s8, y_QuantizeLinear_Input_t_in_0_ptr_const_s8, y_QuantizeLinear_Input_t_weight_0_ptr_const_s8, y_QuantizeLinear_Input_t_weight_1_ptr_const_s32, y_QuantizeLinear_Input_t_in_0_fmt_zero_const_s8, y_QuantizeLinear_Input_t_out_0_fmt_zero_const_s8, y_QuantizeLinear_Input_t_in_0_shape_ch_const_u16, y_QuantizeLinear_Input_t_out_0_shape_ch_const_u16, y_QuantizeLinear_Input_t_out_0_shape_h_w_prod_const_u32, y_QuantizeLinear_Input_t_in_0_fmt_scale_const_f32, y_QuantizeLinear_Input_t_out_0_fmt_scale_const_f32, y_QuantizeLinear_Input_t_weight_0_fmt_scale_const_f32, y_QuantizeLinear_Input_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, {(stai_ptr) y_QuantizeLinear_Input_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END y_QuantizeLinear_Input */
  /* LITE_KERNEL_SECTION BEGIN y_QuantizeLinear_Input_0_conversion */
  {
      const ai_i8* y_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 128);
    ai_float* y_QuantizeLinear_Input_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_outputs[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(17, 1, {(stai_ptr) y_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(y_QuantizeLinear_Input_0_conversion_t_in_0_ptr_const_s8, y_QuantizeLinear_Input_0_conversion_t_out_0_ptr_f32, y_QuantizeLinear_Input_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, y_QuantizeLinear_Input_0_conversion_t_in_0_fmt_scale_const_f32, y_QuantizeLinear_Input_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(17, 1, {(stai_ptr) y_QuantizeLinear_Input_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END y_QuantizeLinear_Input_0_conversion */
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
  net_ctx->_inputs[0] = activations[0] + 0;

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

