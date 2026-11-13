/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:35:04+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xeffc1f16ec9f08e340d471135161208d"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:35:04+0700"
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
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 64),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_2_NAME,
      STAI_NETWORK_IN_2_FLAGS,
      STAI_NETWORK_IN_2_FORMAT,
      STAI_NETWORK_IN_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 64),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 2, 1, 64),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 1344),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 13320),
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
    NULL,NULL},
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
AI_INTQ_INFO_LIST_OBJ_DECLARE(h_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02224217914044857f),
    AI_PACK_INTQ_ZP(-40)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Sub_output_0_0_1__Concat_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02224217914044857f),
    AI_PACK_INTQ_ZP(-40)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02224217914044857f),
    AI_PACK_INTQ_ZP(-40)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.035735901445150375f),
    AI_PACK_INTQ_ZP(15)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0016638186061754823f, 0.00078153470531106f, 0.0015927936183288693f, 0.0011564615415409207f, 0.0008445432176813483f, 0.0013846211368218064f, 0.0014386059483513236f, 0.0012317075161263347f, 0.0021467027254402637f, 0.0011014732299372554f, 0.0008369023562408984f, 0.0011324743973091245f, 0.0012613381259143353f, 0.00157704867888242f, 0.0012793787755072117f, 0.0010427250526845455f, 0.0010641260305419564f, 0.0017814484890550375f, 0.0009479717700742185f, 0.0011567336041480303f, 0.0011694618733599782f, 0.0012837485410273075f, 0.001046278397552669f, 0.0013379025040194392f, 0.0009971308754757047f, 0.0012253524037078023f, 0.0011163963936269283f, 0.001175971352495253f, 0.0011563999578356743f, 0.0009162758942693472f, 0.0010060679633170366f, 0.0012570623075589538f, 0.0010090976720675826f, 0.0015155691653490067f, 0.0011606191983446479f, 0.0018052695086225867f, 0.0010296448599547148f, 0.0017417060444131494f, 0.0008263526833616197f, 0.0012297838693484664f, 0.00153214274905622f, 0.0016186503926292062f, 0.0019175009801983833f, 0.0018602287163957953f, 0.0007377052679657936f, 0.001086508622393012f, 0.0017311993287876248f, 0.0015673699090257287f, 0.0011934296926483512f, 0.001326742465607822f, 0.0009122997871600091f, 0.0010118751088157296f, 0.0012306197313591838f, 0.0010414475109428167f, 0.0011808068957179785f, 0.001834279508329928f, 0.0010799875017255545f, 0.0011237732833251357f, 0.0011817971244454384f, 0.0017596816178411245f, 0.001118843094445765f, 0.001506781205534935f, 0.0009829950286075473f, 0.0018016663379967213f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016388138756155968f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02153967134654522f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009772107005119324f, 0.0010663744760677218f, 0.0010206223232671618f, 0.0009497778955847025f, 0.0012378513347357512f, 0.0010259125847369432f, 0.001295887166634202f, 0.0010054458398371935f, 0.0010634739883244038f, 0.0010337118292227387f, 0.0010987974237650633f, 0.001217597397044301f, 0.001103508984670043f, 0.0011925859143957496f, 0.0009635763126425445f, 0.0010976187186315656f, 0.0009961724281311035f, 0.000992614310234785f, 0.0010305345058441162f, 0.001102913054637611f, 0.0012047181371599436f, 0.0009992951527237892f, 0.0011640962911769748f, 0.0010775309056043625f, 0.0011224965564906597f, 0.0009586996166035533f, 0.0011895507341250777f, 0.0012173042632639408f, 0.0010301187867298722f, 0.0010935408063232899f, 0.0009368680184707046f, 0.0009865480242297053f, 0.0010359317529946566f, 0.0010656900703907013f, 0.0009673634776845574f, 0.0010645276634022593f, 0.0010301190195605159f, 0.0010595610365271568f, 0.0012965310597792268f, 0.0011467484291642904f, 0.0012004404561594129f, 0.0010299764107912779f, 0.001111410092562437f, 0.001027835300192237f, 0.0012306058779358864f, 0.0014296419685706496f, 0.0012012261431664228f, 0.0010709735797718167f, 0.0010486884275451303f, 0.0010159476660192013f, 0.0010157217038795352f, 0.0010978194186463952f, 0.0011414267355576158f, 0.0011886779684573412f, 0.0013590832240879536f, 0.0011399636277928948f, 0.001042982330545783f, 0.000990416039712727f, 0.0012431100476533175f, 0.0010689768241718411f, 0.0009414774249307811f, 0.0012361217522993684f, 0.0011766545940190554f, 0.001164651126600802f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  h1_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 64, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  h_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 64, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _Sub_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  h_0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  _Sub_output_0_0_1__Concat_output_0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  _Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 8192, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 448, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_0_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Div_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Constant_output_0_2D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Add_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Mul_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Constant_2_output_0_2D_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 1, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4096, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 384, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_0_0_g_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  g_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 64, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  _Sub_output_0_output, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_Sub_output_0_output_array, NULL)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  h1_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &h1_output_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  h_output, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &h_output_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _Concat_output_0_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &_Concat_output_0_output_array, &_Concat_output_0_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  _Sub_output_0_0_1__Concat_output_0_conversion_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_Sub_output_0_0_1__Concat_output_0_conversion_output_array, &_Sub_output_0_0_1__Concat_output_0_conversion_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  h_0_conversion_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &h_0_conversion_output_array, &h_0_conversion_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_bias, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_0_Gemm_output_0_bias_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_output, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_fade_fade_0_Gemm_output_0_output_array, &_fade_fade_0_Gemm_output_0_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_scratch0, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 448, 1, 1), AI_STRIDE_INIT(4, 2, 2, 896, 896),
  1, &_fade_fade_0_Gemm_output_0_scratch0_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_weights, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 128, 64, 1, 1), AI_STRIDE_INIT(4, 1, 128, 8192, 8192),
  1, &_fade_fade_0_Gemm_output_0_weights_array, &_fade_fade_0_Gemm_output_0_weights_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_0_conversion_output, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_0_Gemm_output_0_0_conversion_output_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Constant_output_0_2D, AI_STATIC,
  12, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_fade_fade_1_Constant_output_0_2D_array, NULL)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Div_output_0_output, AI_STATIC,
  13, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_1_Div_output_0_output_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Add_output_0_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_1_Add_output_0_output_array, NULL)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Mul_output_0_output, AI_STATIC,
  17, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_1_Mul_output_0_output_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Constant_2_output_0_2D, AI_STATIC,
  11, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_fade_fade_1_Constant_2_output_0_2D_array, NULL)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_output, AI_STATIC,
  16, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_1_Mul_1_output_0_output_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array, &_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_bias, AI_STATIC,
  19, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_2_Gemm_output_0_bias_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_output, AI_STATIC,
  20, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_fade_fade_2_Gemm_output_0_output_array, &_fade_fade_2_Gemm_output_0_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_scratch0, AI_STATIC,
  21, 0x0,
  AI_SHAPE_INIT(4, 1, 384, 1, 1), AI_STRIDE_INIT(4, 2, 2, 768, 768),
  1, &_fade_fade_2_Gemm_output_0_scratch0_array, NULL)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_weights, AI_STATIC,
  22, 0x1,
  AI_SHAPE_INIT(4, 64, 64, 1, 1), AI_STRIDE_INIT(4, 1, 64, 4096, 4096),
  1, &_fade_fade_2_Gemm_output_0_weights_array, &_fade_fade_2_Gemm_output_0_weights_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_0_0_g_conversion_output, AI_STATIC,
  18, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &_fade_fade_2_Gemm_output_0_0_0_g_conversion_output_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  g_output, AI_STATIC,
  23, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &g_output_array, NULL)


AI_TENSOR_CHAIN_OBJ_DECLARE(
  _Sub_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&h_output, (ai_tensor *)&h1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Sub_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _Sub_output_0_layer, 5,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_Sub_output_0_chain,
  NULL, &_Sub_output_0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&h_0_conversion_output, (ai_tensor *)&_Sub_output_0_0_1__Concat_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _Concat_output_0_layer, 6,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_Concat_output_0_chain,
  NULL, &_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_0_Gemm_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_0_Gemm_output_0_weights, (ai_tensor *)&_fade_fade_0_Gemm_output_0_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_0_Gemm_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _fade_fade_0_Gemm_output_0_layer, 9,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &_fade_fade_0_Gemm_output_0_chain,
  NULL, &_fade_fade_0_Gemm_output_0_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _fade_fade_1_Div_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_0_Gemm_output_0_0_conversion_output, (ai_tensor *)&_fade_fade_1_Constant_output_0_2D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_1_Div_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _fade_fade_1_Div_output_0_layer, 12,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_fade_fade_1_Div_output_0_chain,
  NULL, &_fade_fade_1_Div_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _fade_fade_1_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_0_Gemm_output_0_0_conversion_output, (ai_tensor *)&_fade_fade_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_1_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _fade_fade_1_Mul_output_0_layer, 15,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_fade_fade_1_Mul_output_0_chain,
  NULL, &_fade_fade_1_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_1_Mul_output_0_output, (ai_tensor *)&_fade_fade_1_Constant_2_output_0_2D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_1_Mul_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _fade_fade_1_Mul_1_output_0_layer, 16,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &_fade_fade_1_Mul_1_output_0_chain,
  NULL, &_fade_fade_1_Mul_1_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_f32, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_2_Gemm_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_2_Gemm_output_0_weights, (ai_tensor *)&_fade_fade_2_Gemm_output_0_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&_fade_fade_2_Gemm_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _fade_fade_2_Gemm_output_0_layer, 19,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &_fade_fade_2_Gemm_output_0_chain,
  NULL, &_fade_fade_2_Gemm_output_0_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  g_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, (ai_tensor *)&_fade_fade_2_Gemm_output_0_0_0_g_conversion_output, (ai_tensor *)&h_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, (ai_tensor *)&g_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  g_layer, 22,
  ELTWISE_TYPE, 0x0, NULL,
  eltwise, forward_eltwise,
  &g_chain,
  NULL, &g_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_f32, 
)
/**  Hybrid layers declarations section  *************************************/

LITE_FORCE_INLINE void forward_lite_eltwise__Sub_output_0_network(_stai_network_context* net_ctx)
{
  h_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  h_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  h1_output_array.data = AI_PTR(net_ctx->_inputs[1] + 0);
  h1_output_array.data_start = AI_PTR(net_ctx->_inputs[1] + 0);
  _Sub_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _Sub_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(5, 2, { h_output.data->data,h1_output.data->data});
  forward_eltwise((ai_layer *)&_Sub_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(5, 1, { _Sub_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_concat__Concat_output_0_network(_stai_network_context* net_ctx)
{
  h_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 512);
  h_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 512);
  _Sub_output_0_0_1__Concat_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _Sub_output_0_0_1__Concat_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 320);
  _Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 320);
  _STAI_NETWORK_EVENT_NODE_START_CB(6, 2, { h_0_conversion_output.data->data,_Sub_output_0_0_1__Concat_output_0_conversion_output.data->data});
  forward_concat((ai_layer *)&_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(6, 1, { _Concat_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_dense_integer_SSSA_ch__fade_fade_0_Gemm_output_0_network(_stai_network_context* net_ctx)
{
  _Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 320);
  _Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_0_Gemm_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 8);
  _fade_fade_0_Gemm_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 8);
  _fade_fade_0_Gemm_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 8200);
  _fade_fade_0_Gemm_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 8200);
  _fade_fade_0_Gemm_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 448);
  _fade_fade_0_Gemm_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 448);
  _fade_fade_0_Gemm_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _fade_fade_0_Gemm_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(9, 1, { _Concat_output_0_output.data->data});
  forward_dense_integer_SSSA_ch((ai_layer *)&_fade_fade_0_Gemm_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(9, 1, { _fade_fade_0_Gemm_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__fade_fade_1_Div_output_0_network(_stai_network_context* net_ctx)
{
  _fade_fade_0_Gemm_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_0_Gemm_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_1_Constant_output_0_2D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _fade_fade_1_Constant_output_0_2D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _fade_fade_1_Div_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 576);
  _fade_fade_1_Div_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 576);
  _STAI_NETWORK_EVENT_NODE_START_CB(12, 2, { _fade_fade_0_Gemm_output_0_0_conversion_output.data->data,_fade_fade_1_Constant_output_0_2D.data->data});
  forward_eltwise((ai_layer *)&_fade_fade_1_Div_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(12, 1, { _fade_fade_1_Div_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__fade_fade_1_Mul_output_0_network(_stai_network_context* net_ctx)
{
  _fade_fade_0_Gemm_output_0_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_0_Gemm_output_0_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 576);
  _fade_fade_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 576);
  _fade_fade_1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 832);
  _fade_fade_1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 832);
  _STAI_NETWORK_EVENT_NODE_START_CB(15, 2, { _fade_fade_0_Gemm_output_0_0_conversion_output.data->data,_fade_fade_1_Add_output_0_output.data->data});
  forward_eltwise((ai_layer *)&_fade_fade_1_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(15, 1, { _fade_fade_1_Mul_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise__fade_fade_1_Mul_1_output_0_network(_stai_network_context* net_ctx)
{
  _fade_fade_1_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 832);
  _fade_fade_1_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 832);
  _fade_fade_1_Constant_2_output_0_2D_array.data = AI_PTR(net_ctx->_weights[0] + 4);
  _fade_fade_1_Constant_2_output_0_2D_array.data_start = AI_PTR(net_ctx->_weights[0] + 4);
  _fade_fade_1_Mul_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _fade_fade_1_Mul_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 2, { _fade_fade_1_Mul_output_0_output.data->data,_fade_fade_1_Constant_2_output_0_2D.data->data});
  forward_eltwise((ai_layer *)&_fade_fade_1_Mul_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, { _fade_fade_1_Mul_1_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_dense_integer_SSSA_ch__fade_fade_2_Gemm_output_0_network(_stai_network_context* net_ctx)
{
  _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 512);
  _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 512);
  _fade_fade_2_Gemm_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 8968);
  _fade_fade_2_Gemm_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 8968);
  _fade_fade_2_Gemm_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 13064);
  _fade_fade_2_Gemm_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 13064);
  _fade_fade_2_Gemm_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 576);
  _fade_fade_2_Gemm_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 576);
  _fade_fade_2_Gemm_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 256);
  _fade_fade_2_Gemm_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 256);
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 1, { _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output.data->data});
  forward_dense_integer_SSSA_ch((ai_layer *)&_fade_fade_2_Gemm_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, { _fade_fade_2_Gemm_output_0_output.data->data});
}

LITE_FORCE_INLINE void forward_lite_eltwise_g_network(_stai_network_context* net_ctx)
{
  _fade_fade_2_Gemm_output_0_0_0_g_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 320);
  _fade_fade_2_Gemm_output_0_0_0_g_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 320);
  h_output_array.data = AI_PTR(net_ctx->_inputs[0] + 0);
  h_output_array.data_start = AI_PTR(net_ctx->_inputs[0] + 0);
  g_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  g_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(22, 2, { _fade_fade_2_Gemm_output_0_0_0_g_conversion_output.data->data,h_output.data->data});
  forward_eltwise((ai_layer *)&g_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(22, 1, { g_output.data->data});
}

/*****************************************************************************/


static const ai_u32 h_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float h_0_conversion_t_out_0_fmt_scale_const_f32 = 0.02224217914044857f;
static const ai_i8 h_0_conversion_t_out_0_fmt_zero_const_s8 = -40;


static const ai_u32 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.02224217914044857f;
static const ai_i8 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_zero_const_s8 = -40;



static const ai_u32 _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.035735901445150375f;
static const ai_i8 _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 15;


static const ai_i32 _fade_fade_1_Erf_output_0_t_in_0_shape_ch_prod_const_s32 = 64;




static const ai_u32 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.016388138756155968f;
static const ai_i8 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_zero_const_s8 = -118;


static const ai_u32 _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_scale_const_f32 = 0.02153967134654522f;
static const ai_i8 _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_zero_const_s8 = 3;

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


  /* LITE_KERNEL_SECTION BEGIN h_0_conversion */
  {
      const ai_float* h_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_inputs[0] + 0);
    ai_i8* h_0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(0, 1, {(stai_ptr) h_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(h_0_conversion_t_in_0_ptr_const_f32, h_0_conversion_t_out_0_ptr_s8, h_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, h_0_conversion_t_out_0_fmt_scale_const_f32, h_0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(0, 1, {(stai_ptr) h_0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END h_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _Sub_output_0 */
  {
    
  forward_lite_eltwise__Sub_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _Sub_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _Sub_output_0_0_1__Concat_output_0_conversion */
  {
      const ai_float* _Sub_output_0_0_1__Concat_output_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 256);
    ai_i8* _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 256);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(5, 1, {(stai_ptr) _Sub_output_0_0_1__Concat_output_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_Sub_output_0_0_1__Concat_output_0_conversion_t_in_0_ptr_const_f32, _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_ptr_s8, _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_scale_const_f32, _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(5, 1, {(stai_ptr) _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _Sub_output_0_0_1__Concat_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _Concat_output_0 */
  {
    
  forward_lite_concat__Concat_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_0_Gemm_output_0 */
  {
    
  forward_lite_dense_integer_SSSA_ch__fade_fade_0_Gemm_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _fade_fade_0_Gemm_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_0_Gemm_output_0_0_conversion */
  {
      const ai_i8* _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 256);
    ai_float* _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 320);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(9, 1, {(stai_ptr) _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_fade_fade_0_Gemm_output_0_0_conversion_t_in_0_ptr_const_s8, _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_ptr_f32, _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32, _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(9, 1, {(stai_ptr) _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _fade_fade_0_Gemm_output_0_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Div_output_0 */
  {
    
  forward_lite_eltwise__fade_fade_1_Div_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Div_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Erf_output_0 */
  {
      ai_handle _fade_fade_1_Erf_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 832);
    const ai_handle _fade_fade_1_Erf_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 576);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(13, 1, {(stai_ptr) _fade_fade_1_Erf_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_erf_if32of32(_fade_fade_1_Erf_output_0_t_out_0_ptr_handle, _fade_fade_1_Erf_output_0_t_in_0_ptr_const_handle, _fade_fade_1_Erf_output_0_t_in_0_shape_ch_prod_const_s32, NULL);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(13, 1, {(stai_ptr) _fade_fade_1_Erf_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Erf_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Add_output_0 */
  {
      ai_float* _fade_fade_1_Add_output_0_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 576);
    const ai_float* _fade_fade_1_Add_output_0_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 832);
    const ai_float* _fade_fade_1_Add_output_0_t_weight_0_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 8456);
    const ai_float* _fade_fade_1_Add_output_0_t_weight_1_ptr_const_f32 = (ai_float*)(net_ctx->_weights[0] + 8712);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(14, 1, {(stai_ptr) _fade_fade_1_Add_output_0_t_in_0_ptr_const_f32});
    
  forward_lite_bn_if32of32wf32(_fade_fade_1_Add_output_0_t_out_0_ptr_f32, _fade_fade_1_Add_output_0_t_in_0_ptr_const_f32, _fade_fade_1_Add_output_0_t_weight_0_ptr_const_f32, _fade_fade_1_Add_output_0_t_weight_1_ptr_const_f32, (ai_u32)(64), (ai_size)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(14, 1, {(stai_ptr) _fade_fade_1_Add_output_0_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Mul_output_0 */
  {
    
  forward_lite_eltwise__fade_fade_1_Mul_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Mul_1_output_0 */
  {
    
  forward_lite_eltwise__fade_fade_1_Mul_1_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Mul_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion */
  {
      const ai_float* _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 256);
    ai_i8* _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(16, 1, {(stai_ptr) _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_in_0_ptr_const_f32, _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_ptr_s8, _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_scale_const_f32, _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(16, 1, {(stai_ptr) _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_2_Gemm_output_0 */
  {
    
  forward_lite_dense_integer_SSSA_ch__fade_fade_2_Gemm_output_0_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _fade_fade_2_Gemm_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _fade_fade_2_Gemm_output_0_0_0_g_conversion */
  {
      const ai_i8* _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 256);
    ai_float* _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 320);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(19, 1, {(stai_ptr) _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_ptr_const_s8, _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_ptr_f32, _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_scale_const_f32, _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(19, 1, {(stai_ptr) _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _fade_fade_2_Gemm_output_0_0_0_g_conversion */
  /* LITE_KERNEL_SECTION BEGIN g */
  {
    
  forward_lite_eltwise_g_network(net_ctx);
  }
  /* LITE_KERNEL_SECTION END g */
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

  net_ctx->_inputs[1] = activations[0] + 256;

  net_ctx->_outputs[0] = activations[0] + 576;
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

