/* C part of the v6 Zn-ion streaming model (AnchoredFade-ResConv-GELU, ZN-E010 config).
 * Mirrors summaries() in phase3/board/st_int8.py and the masked summary of ResidualConvMaskedSummary.forward:
 *   after n observed cycles with per-cycle states g_1..g_n (output of the F model),
 *   summary = [ mean(g_1..g_n), g_n, g_n - g_1 ]            (3 * ZN_D values)
 *   EOL     = ZN_LABEL_MEAN + ZN_LABEL_SCALE * y              (y = output of the H model)
 * Portable C (no HAL): compiled on the host by cpart_test.c and on the board by zn_app.c. */
#ifndef ZN_D
#define ZN_D 64
#endif

typedef struct {
  double sum[ZN_D];   /* running sum of g (double: exact enough for 100 cycles) */
  float first[ZN_D];  /* g_1 */
  float last[ZN_D];   /* g_n */
  int n;              /* observed cycles so far */
} zn_glue_t;

static void zn_glue_reset(zn_glue_t *s)
{
  for (int k = 0; k < ZN_D; k++) { s->sum[k] = 0.0; s->first[k] = 0.0f; s->last[k] = 0.0f; }
  s->n = 0;
}

/* add the state g of the newest observed cycle */
static void zn_glue_add(zn_glue_t *s, const float *g)
{
  for (int k = 0; k < ZN_D; k++) {
    s->sum[k] += g[k];
    s->last[k] = g[k];
    if (s->n == 0) s->first[k] = g[k];
  }
  s->n++;
}

/* write the 3*ZN_D summary for the current prefix into out (the H model input) */
static void zn_glue_summary(const zn_glue_t *s, float *out)
{
  double inv = 1.0 / (double)(s->n > 0 ? s->n : 1);
  for (int k = 0; k < ZN_D; k++) {
    out[k] = (float)(s->sum[k] * inv);
    out[ZN_D + k] = s->last[k];
    out[2 * ZN_D + k] = s->last[k] - s->first[k];
  }
}
