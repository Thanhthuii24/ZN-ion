/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:34:39+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x922b9a56803618bc8bf825dd67668092"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:34:39+0700"
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
    AI_PACK_INTQ_SCALE(0.016646970063447952f),
    AI_PACK_INTQ_ZP(-81)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Sub_output_0_0_1__Concat_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016646970063447952f),
    AI_PACK_INTQ_ZP(-81)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016646970063447952f),
    AI_PACK_INTQ_ZP(-81)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013067829422652721f),
    AI_PACK_INTQ_ZP(17)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0010895516024902463f, 0.0012239547213539481f, 0.001304021687246859f, 0.000960394914727658f, 0.0008078282116912305f, 0.0011693621054291725f, 0.0011838988866657019f, 0.0008174749673344195f, 0.0012258508941158652f, 0.0009296388598158956f, 0.0008715161238797009f, 0.0008440379751846194f, 0.0010973109165206552f, 0.0010415747528895736f, 0.0009872564114630222f, 0.0013605435378849506f, 0.0012744063278660178f, 0.0012058268766850233f, 0.0011329526314511895f, 0.0009252328309230506f, 0.0008358701597899199f, 0.0008279182948172092f, 0.0009874675888568163f, 0.0008623699541203678f, 0.0012756704818457365f, 0.0008719986653886735f, 0.001714471261948347f, 0.0011449891608208418f, 0.0013185304123908281f, 0.0007945430697873235f, 0.0008274289430119097f, 0.0009668874554336071f, 0.0009432474034838378f, 0.0011685302015393972f, 0.000843718065880239f, 0.0008825701079331338f, 0.0009169347467832267f, 0.0009028605418279767f, 0.0008683688356541097f, 0.0012278370559215546f, 0.0009263330139219761f, 0.0008382311789318919f, 0.0010293914237990975f, 0.0007483225781470537f, 0.0009710981394164264f, 0.0011012443574145436f, 0.0008791901054792106f, 0.0014126870082691312f, 0.0010607679141685367f, 0.000914305099286139f, 0.0010524323442950845f, 0.000843640067614615f, 0.001205902430228889f, 0.0010481523349881172f, 0.001223270082846284f, 0.0011302399216219783f, 0.0010765851475298405f, 0.0008886864525265992f, 0.0013491250574588776f, 0.00093866913812235f, 0.0008210183004848659f, 0.0010308632627129555f, 0.0010041085770353675f, 0.0012621264904737473f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005891700740903616f),
    AI_PACK_INTQ_ZP(-99)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.005210926290601492f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009752047481015325f, 0.0009438676061108708f, 0.0010794195113703609f, 0.0009285015403293073f, 0.0010122853564098477f, 0.0009685183758847415f, 0.0009868510533124208f, 0.0010015073930844665f, 0.0010446739615872502f, 0.001034927787259221f, 0.0009779402753338218f, 0.0010353834368288517f, 0.001068156328983605f, 0.0010453914292156696f, 0.000964401348028332f, 0.0010086948750540614f, 0.0010753036476671696f, 0.0009596475283615291f, 0.0009622275829315186f, 0.0010058835614472628f, 0.0010447086533531547f, 0.0009754274506121874f, 0.0011329184053465724f, 0.0011071127373725176f, 0.0010489530395716429f, 0.0009834152879193425f, 0.0009743552072905004f, 0.0010253273649141192f, 0.0009909135987982154f, 0.0010535609908401966f, 0.0009489541407674551f, 0.0009685509139671922f, 0.0009533825796097517f, 0.0010051665594801307f, 0.0008987536421045661f, 0.00102835928555578f, 0.0010541804367676377f, 0.0010727342450991273f, 0.0010601484682410955f, 0.0011770757846534252f, 0.0009912174427881837f, 0.001013491302728653f, 0.0010463750222697854f, 0.0010326580377295613f, 0.0011412325548008084f, 0.0010900214547291398f, 0.0009125705109909177f, 0.001141914282925427f, 0.0010391074465587735f, 0.0009630830027163029f, 0.0010413199197500944f, 0.0010127333225682378f, 0.0012805351288989186f, 0.001003870158456266f, 0.001023701741360128f, 0.001101479516364634f, 0.0010022042552009225f, 0.0009966252837330103f, 0.001019728253595531f, 0.0009670722647570074f, 0.0009594992734491825f, 0.0011930688051506877f, 0.0009992019040510058f, 0.00102840771432966f),
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
static const ai_float h_0_conversion_t_out_0_fmt_scale_const_f32 = 0.016646970063447952f;
static const ai_i8 h_0_conversion_t_out_0_fmt_zero_const_s8 = -81;


static const ai_u32 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.016646970063447952f;
static const ai_i8 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_zero_const_s8 = -81;



static const ai_u32 _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.013067829422652721f;
static const ai_i8 _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 17;


static const ai_i32 _fade_fade_1_Erf_output_0_t_in_0_shape_ch_prod_const_s32 = 64;




static const ai_u32 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.005891700740903616f;
static const ai_i8 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_zero_const_s8 = -99;


static const ai_u32 _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_scale_const_f32 = 0.005210926290601492f;
static const ai_i8 _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_zero_const_s8 = -3;

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

