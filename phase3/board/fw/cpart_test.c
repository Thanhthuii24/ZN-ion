/* Host test of zn_cpart.c: reads K cycles x ZN_D float32 g states (g.bin, K = file size / (4*ZN_D)),
 * writes the summary for every prefix n = 1..K to s.bin (K x 3*ZN_D float32). */
#include <stdio.h>
#include <stdlib.h>
#include "zn_cpart.c"
int main(int argc, char **argv)
{
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long nb = ftell(f); fseek(f, 0, SEEK_SET);
  int K = (int)(nb / (4 * ZN_D)); float *g = malloc(nb); fread(g, 1, nb, f); fclose(f);
  FILE *o = fopen(argv[2], "wb"); zn_glue_t s; zn_glue_reset(&s); float out[3 * ZN_D];
  for (int n = 0; n < K; n++) { zn_glue_add(&s, g + n * ZN_D); zn_glue_summary(&s, out); fwrite(out, 4, 3 * ZN_D, o); }
  fclose(o); free(g); return 0;
}
