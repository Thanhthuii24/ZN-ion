/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-13T15:35:29+0700
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
#define _STAI_NETWORK_MODEL_SIGNATURE     "0x15bab09db10a5e2a7cb6b2a67b456cd3"
#define _STAI_NETWORK_DATETIME            "2026-11-13T15:35:29+0700"
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
    AI_PACK_INTQ_SCALE(0.018280602991580963f),
    AI_PACK_INTQ_ZP(-39)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Sub_output_0_0_1__Concat_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.018280602991580963f),
    AI_PACK_INTQ_ZP(-39)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.018280602991580963f),
    AI_PACK_INTQ_ZP(-39)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016072848811745644f),
    AI_PACK_INTQ_ZP(28)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_0_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0008881730027496815f, 0.0010793749243021011f, 0.001195060438476503f, 0.000924740161281079f, 0.0009056052076630294f, 0.0014782084617763758f, 0.0008786428370513022f, 0.0009176724124699831f, 0.001094282721169293f, 0.0009608716936782002f, 0.0010555234039202332f, 0.0012067455099895597f, 0.0011245387140661478f, 0.0009571070550009608f, 0.0010733595117926598f, 0.0012510378146544099f, 0.0010613722261041403f, 0.0012149512767791748f, 0.0013306059408932924f, 0.001062452094629407f, 0.000933194940444082f, 0.0010875023435801268f, 0.0010338211432099342f, 0.0009563945350237191f, 0.001319895964115858f, 0.0009191668359562755f, 0.0012036124244332314f, 0.0012910105288028717f, 0.001234295777976513f, 0.0010106816189363599f, 0.0011389844585210085f, 0.0011572360526770353f, 0.0010645556030794978f, 0.000842480338178575f, 0.0009439072455279529f, 0.0011240034364163876f, 0.001213000388815999f, 0.00081535370554775f, 0.0011494223726913333f, 0.001361601403914392f, 0.0009540542960166931f, 0.0011411089217290282f, 0.0010235034860670567f, 0.0007682795985601842f, 0.0011744628427550197f, 0.000948890985455364f, 0.001074825064279139f, 0.0013061432400718331f, 0.0010678934631869197f, 0.001157857128418982f, 0.0012166404630988836f, 0.000747193000279367f, 0.0014698868617415428f, 0.0010611464967951179f, 0.0008887972799129784f, 0.0010928462725132704f, 0.0012916334671899676f, 0.0009527896181680262f, 0.0012958977604284883f, 0.0013436699518933892f, 0.0011073743226006627f, 0.0013573206961154938f, 0.0009627051185816526f, 0.0012031277874484658f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.006561816204339266f),
    AI_PACK_INTQ_ZP(-102)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.008573701605200768f),
    AI_PACK_INTQ_ZP(3)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_fade_fade_2_Gemm_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0009501524036750197f, 0.0009377344977110624f, 0.0010195482755079865f, 0.0009296481730416417f, 0.0010449924739077687f, 0.0009991137776523829f, 0.0011182969901710749f, 0.0009582720813341439f, 0.0009954324923455715f, 0.0009808135218918324f, 0.0011164258467033505f, 0.001103757880628109f, 0.0010808532824739814f, 0.0010625723516568542f, 0.000980698736384511f, 0.0009963922202587128f, 0.0010067197727039456f, 0.0009617596515454352f, 0.0011466286377981305f, 0.0009893985697999597f, 0.0010705600725486875f, 0.000951017253100872f, 0.000985965016297996f, 0.0010209041647613049f, 0.0010407953523099422f, 0.0009851868962869048f, 0.001008099177852273f, 0.001178534235805273f, 0.0009657758055254817f, 0.0010752629023045301f, 0.0009513466502539814f, 0.0009644557721912861f, 0.0009942816104739904f, 0.0009652228327468038f, 0.0009942087344825268f, 0.0010710067581385374f, 0.0010859026806429029f, 0.0009955053683370352f, 0.0010555911576375365f, 0.0010518424678593874f, 0.0010017029708251357f, 0.0010480679338797927f, 0.0009695048211142421f, 0.0010889475233852863f, 0.001230622990988195f, 0.0009823058499023318f, 0.0009598773904144764f, 0.0009886374464258552f, 0.0009713023318909109f, 0.0009697419009171426f, 0.0010896415915340185f, 0.0009598605101928115f, 0.0010285422904416919f, 0.0009651195141486824f, 0.001123621710576117f, 0.0011769438860937953f, 0.0010108186397701502f, 0.0009773537749424577f, 0.0010060245404019952f, 0.000967198982834816f, 0.0009552367846481502f, 0.0010635443031787872f, 0.0009811122436076403f, 0.0010084225796163082f),
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
static const ai_float h_0_conversion_t_out_0_fmt_scale_const_f32 = 0.018280602991580963f;
static const ai_i8 h_0_conversion_t_out_0_fmt_zero_const_s8 = -39;


static const ai_u32 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.018280602991580963f;
static const ai_i8 _Sub_output_0_0_1__Concat_output_0_conversion_t_out_0_fmt_zero_const_s8 = -39;



static const ai_u32 _fade_fade_0_Gemm_output_0_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_scale_const_f32 = 0.016072848811745644f;
static const ai_i8 _fade_fade_0_Gemm_output_0_0_conversion_t_in_0_fmt_zero_const_s8 = 28;


static const ai_i32 _fade_fade_1_Erf_output_0_t_in_0_shape_ch_prod_const_s32 = 64;




static const ai_u32 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.006561816204339266f;
static const ai_i8 _fade_fade_1_Mul_1_output_0_0_0__fade_fade_2_Gemm_output_0_conversion_t_out_0_fmt_zero_const_s8 = -102;


static const ai_u32 _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 64;
static const ai_float _fade_fade_2_Gemm_output_0_0_0_g_conversion_t_in_0_fmt_scale_const_f32 = 0.008573701605200768f;
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

