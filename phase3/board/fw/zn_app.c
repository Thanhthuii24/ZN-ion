/* Phase 3 board (v6 Zn-ion, ZN-E010 config): on-board streaming EOL prediction on NUCLEO-H7S3L8.
 * For every VAL cell and every new cycle n = 1..k:
 *   E (ST fp32 C-model "enc"):   curve[3][300] -> h_n[64]            (once per new cycle; h_1 cached)
 *   F (ST INT8 C-model "fade"):  (h_n, h_1)   -> g_n[64]            (fp32 in/out)
 *   C (zn_cpart.c):              running sum / first / last of g -> summary[192] = [mean, last, last-first]
 *   H (ST fp32 C-model "head"):  summary      -> y;  EOL = ZN_LABEL_MEAN + ZN_LABEL_SCALE * y
 * Timing with the DWT cycle counter; one line per prefix over the ST-LINK VCP UART:
 *   P,<cell>,<n>,<eol_board*1e4>,<eol_host_ref*1e4>,<eol_true*1e4>,<cyc_E>,<cyc_F>,<cyc_C+H>
 * and a summary line S,<n_prefix>,<mape_board*1e6>,<mape_ref*1e6>,<max_abs_diff*1e6>,<ms E*1e4>,<ms F*1e4>,<ms C+H*1e4>.
 * newlib-nano has no %f, so values are printed as scaled integers (as in v5 naion_app.c). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "stm32h7rsxx.h"
#include "enc.h"
#include "fade.h"
#include "head.h"
#include "zn_params.h"
#include "zn_cpart.c"

STAI_NETWORK_CONTEXT_DECLARE(net_enc, STAI_ENC_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(net_fade, STAI_FADE_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(net_head, STAI_HEAD_CONTEXT_SIZE)
STAI_ALIGNED(STAI_ENC_ACTIVATION_1_ALIGNMENT) static uint8_t act_enc[STAI_ENC_ACTIVATION_1_SIZE_BYTES];
STAI_ALIGNED(STAI_FADE_ACTIVATION_1_ALIGNMENT) static uint8_t act_fade[STAI_FADE_ACTIVATION_1_SIZE_BYTES];
STAI_ALIGNED(STAI_HEAD_ACTIVATION_1_ALIGNMENT) static uint8_t act_head[STAI_HEAD_ACTIVATION_1_SIZE_BYTES];
static stai_ptr in_enc[STAI_ENC_IN_NUM], out_enc[STAI_ENC_OUT_NUM];
static stai_ptr in_fade[STAI_FADE_IN_NUM], out_fade[STAI_FADE_OUT_NUM];
static stai_ptr in_head[STAI_HEAD_IN_NUM], out_head[STAI_HEAD_OUT_NUM];

static float h1[ZN_D];
static zn_glue_t glue;

static int ai_init(void)
{
  stai_ptr a[1]; stai_size n;
  if (stai_enc_init(net_enc) != STAI_SUCCESS || stai_fade_init(net_fade) != STAI_SUCCESS || stai_head_init(net_head) != STAI_SUCCESS)
    return -1;
  a[0] = (stai_ptr)act_enc;  stai_enc_set_activations(net_enc, a, 1);
  a[0] = (stai_ptr)act_fade; stai_fade_set_activations(net_fade, a, 1);
  a[0] = (stai_ptr)act_head; stai_head_set_activations(net_head, a, 1);
  stai_enc_get_inputs(net_enc, in_enc, &n);    stai_enc_get_outputs(net_enc, out_enc, &n);
  stai_fade_get_inputs(net_fade, in_fade, &n); stai_fade_get_outputs(net_fade, out_fade, &n);
  stai_head_get_inputs(net_head, in_head, &n); stai_head_get_outputs(net_head, out_head, &n);
  return 0;
}

void zn_app_run(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  if (ai_init()) { printf("E,ai_init failed\r\n"); return; }
  printf("I,%s streaming E->F->C->H, SystemCoreClock=%lu\r\n", ZN_TAG, (unsigned long)SystemCoreClock);
  double ape_b = 0, ape_r = 0, maxd = 0, cyc_e = 0, cyc_f = 0, cyc_h = 0; int np = 0;
  for (int c = 0; c < zn_ncells; c++) {
    const zn_cell_t *cell = &zn_cells[c];
    zn_glue_reset(&glue);
    for (int n = 1; n <= cell->k; n++) {
      int i = n - 1; const float *curve = cell->curves + (size_t)i * 900;
      uint32_t t0 = DWT->CYCCNT;
      memcpy(in_enc[0], curve, 900 * sizeof(float));                     /* E: encode the new cycle */
      stai_enc_run(net_enc, STAI_MODE_SYNC);
      if (i == 0) memcpy(h1, out_enc[0], ZN_D * sizeof(float));
      uint32_t t1 = DWT->CYCCNT;
      memcpy(in_fade[0], out_enc[0], ZN_D * sizeof(float));             /* F: g = fade([h, h - h1]) + h */
      memcpy(in_fade[1], h1, ZN_D * sizeof(float));
      stai_fade_run(net_fade, STAI_MODE_SYNC);
      uint32_t t2 = DWT->CYCCNT;
      zn_glue_add(&glue, (const float *)out_fade[0]);                    /* C: running summary */
      zn_glue_summary(&glue, (float *)in_head[0]);
      stai_head_run(net_head, STAI_MODE_SYNC);                           /* H: head */
      double eol = ZN_LABEL_MEAN + ZN_LABEL_SCALE * (double)((float *)out_head[0])[0];
      uint32_t t3 = DWT->CYCCNT;
      double ref = cell->ref_eol[i];
      ape_b += fabs(eol - cell->eol) / cell->eol; ape_r += fabs(ref - cell->eol) / cell->eol;
      if (fabs(eol - ref) > maxd) maxd = fabs(eol - ref);
      cyc_e += t1 - t0; cyc_f += t2 - t1; cyc_h += t3 - t2; np++;
      printf("P,%d,%d,%ld,%ld,%ld,%lu,%lu,%lu\r\n", c, n, (long)lround(eol * 1e4), (long)lround(ref * 1e4),
             (long)lround(cell->eol * 1e4), (unsigned long)(t1 - t0), (unsigned long)(t2 - t1), (unsigned long)(t3 - t2));
    }
  }
  double k = 1000.0 / SystemCoreClock / np;
  printf("S,%d,%ld,%ld,%ld,%ld,%ld,%ld\r\n", np, lround(ape_b / np * 1e6), lround(ape_r / np * 1e6), lround(maxd * 1e6),
         lround(cyc_e * k * 1e4), lround(cyc_f * k * 1e4), lround(cyc_h * k * 1e4));
}
