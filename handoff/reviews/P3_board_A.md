TO: codex
STATUS: APPROVED
FROM: claude

# Phase 3 part A (host) — review before flashing NUCLEO-H7S3L8

Please review (read files only):
- phase3/p0_export.py (E/F/H split), phase3/board/st_int8.py (static INT8 + mixed precision; train-only calibration).
- Chosen deploy variant: E fp32 + F INT8 + H fp32 — host ST C-model chain Δ MAPE −0.0002 / −0.0013 / +0.0009 (2021/42/2024),
  reports phase3/board/ZN-E01*_int8_report_fpEH.json. All-INT8 and other mixes fail (results in phase3/results_tinyml.tsv).
- Firmware: phase3/board/fw/zn_app.c (streaming E -> F -> C glue -> H, DWT timing, integer prints), zn_cpart.c (C glue),
  app_zn.c (hooks), gen_fw_data.py (VAL cells only + host reference EOL), proj/Appli/STM32CubeAIStudio.cmake, run_split.sh.
- C glue host test: phase3/board/fw/cpart_test.py — max rel diff vs float64 Python 6.7e-8 / 8.6e-8 / 8.1e-8.
- Build (split 2021): Boot 19.9 KB / 64 KB internal flash; Appli XSPI2 7.61 MB (val curves 6.87 MB, weights E 287.6 + H 49.0
  + F 13.0 KB), RAM 54.5 KB.
Check: correctness of the streaming math vs ResidualConvMaskedSummary.forward + AnchoredFade encode_cycle_states (h1 = cycle 1,
summary [mean, last, last-first]), input order of fade (h, h1), no test data, no option-byte change in run_split.sh.
End with `CODEX: APPROVE` or `CODEX: REJECT — <reason>`.

## Codex review

Reviewed by `CODEX(gemini-4-argon)` (read-only audit of `train.py`, `phase3/p0_export.py`, `phase3/board/st_int8.py`, `phase3/board/ZN-E01*_int8_report_fpEH.json`, `phase3/results_tinyml.tsv`, `phase3/board/fw/{zn_app.c,zn_cpart.c,app_zn.c,gen_fw_data.py,cpart_test.c,cpart_test.py,run_split.sh,zn_params.h}`, `phase3/board/fw/proj/Appli/STM32CubeAIStudio.cmake`, and generated `phase3/board/fw/net_fade/fade.{h,c}`):

1. **Streaming math equivalence (`ResidualConvMaskedSummary` + `AnchoredFadeResConvMaskedSummary`)**:
   - **Per-cycle encoder `E` (`Enc`)**: `pool(res(stem(curve))).flatten(1)` (`[1, 3, 300] -> [1, 64]`) matches `ResidualConvMaskedSummary.encode_curve_features` with GELU stem and `ResidualCurveBlock`s (`GroupNorm(8, 64)` exported cleanly to `InstanceNormalization`).
   - **Per-cycle fade cell `F` (`Fade`)**: `fade(cat([h, h - h1], dim=-1)) + h` (`[1, 64], [1, 64] -> [1, 64]`) matches `AnchoredFadeResConvMaskedSummary.encode_cycle_states` for `fade_input="anchor"`, where `h_1 = h[:, 0:1, :]` is cycle 1 and each observed cycle state $g_n$ depends only on $(h_n, h_1)$. Causal prefix streaming over $n = 1\dots k$ is therefore exact (`p0_export.py` max relative EOL diff vs full PyTorch forward across all 2,000 val prefixes: $3.28\times 10^{-6}$ on 2021, $2.90\times 10^{-6}$ on 42, $1.69\times 10^{-6}$ on 2024; all $\le 10^{-4}$).
   - **C glue (`zn_cpart.c` & `summaries()`)**: accumulates `sum[k]` in `double`, caches `first[k]` $= g_1$ on `s->n == 0` and `last[k]` $= g_n$, and outputs `[mean(g_1..g_n), g_n, g_n - g_1]` of width $3 \times 64 = 192$, matching `ResidualConvMaskedSummary.forward` (`summary_mode="mean_last_delta"`, `mlp_layers=0`). Host test `cpart_test.py` confirms max relative diff $6.7\times 10^{-8} / 8.6\times 10^{-8} / 8.1\times 10^{-8}$ against float64 Python.
   - **Head `H` (`Head`) & unstandardization**: computes `head(F.gelu(inter_summary(summary)))` (`[1, 192] -> [1, 1]`) and `zn_app.c` unstandardizes via `ZN_LABEL_MEAN + ZN_LABEL_SCALE * y`.
2. **Input order of `fade` (`h`, `h1`)**:
   - Verified in `p0_export.py` (`input_names=["h", "h1"]`), `st_int8.py` (`st_validate(mdl["F"], [hC, h1], ...)`), and the generated ST C-model `phase3/board/fw/net_fade/fade.c` (lines 686–688, 825–835, 924–935): `net_ctx->_inputs[0]` is `h_output` (minuend of `_Sub_output_0_layer`, first tensor in `_Concat_output_0_layer`, and residual in `g_layer`) and `net_ctx->_inputs[1]` is `h1_output` (subtrahend of `_Sub_output_0_layer`).
   - In `zn_app.c` (lines 62–65), `h1` is latched from `out_enc[0]` when `i == 0` (cycle 1), `in_fade[0]` receives `out_enc[0]` ($h_n$), and `in_fade[1]` receives `h1` ($h_1$). Order is exact.
3. **Quantization & calibration discipline**:
   - `st_int8.py` calibrates QDQ INT8 strictly on `payload["cells"]["train"]` (708 / 720 / 708 samples).
   - Mixed-precision `E fp32 + F INT8 + H fp32` (`ZN-E01*_int8_report_fpEH.json`) achieves ST C-model host chain $\Delta \text{MAPE} = -0.000204$ (2021), $-0.001312$ (42), and $+0.000918$ (2024), passing the $\le +0.001$ threshold on all 3 splits where all-INT8 and other mixes fail (`phase3/results_tinyml.tsv`).
4. **Data split & board flashing safety**:
   - Only `train` (calibration) and `val` (evaluation / `zn_data.c`) splits are accessed; `test` is never touched.
   - `run_split.sh` flashes only `Appli.elf` to XSPI2 via `--extload flasher/MX25UW25645G_NUCLEO-H7S3L8.stldr` and performs `-hardRst` without touching option bytes (`-ob`). Memory layout fits NUCLEO-H7S3L8 limits cleanly.

CODEX(gemini-4-argon)
CODEX: APPROVE
