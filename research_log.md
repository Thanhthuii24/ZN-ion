# Evidence-Led Research Log — v6_auto (ZN-coin)

## Round Status

- Project created 2026-11-07 from the v4_auto workflow (no Li-ion results carried over).
- Data: BatteryLife processed ZN-coin, paper splits 2021/42/2024 (env `BL_SPLIT`).
- Next: `ZN-E000` CPMLP baseline.

## Run Entry Template

### Before training

- Experiment ID:
- Model and matched reference:
- Direct observations:
- Main hypothesis:
- One main change:
- Fixed parts:
- Expected evidence:
- Reject rule:
- Required diagnostics:
- Stage and budget:

### After training

- Run: `VALID` or `INVALID`.
- Decision: `KEEP` or `DISCARD` for a valid run; blank for an invalid run.
- Selected checkpoint and metrics:
- Aggregate learning, gradient, activation, prediction, group, mask, and
  compute findings:
- Comparison with reference:
- Main limit:
- Cause ledger with supporting, contradicting, and missing evidence:
- Block decisions in plain language:
- Conclusion:
- What this run does not prove:
- Causal intervention map:
- Next step:

Do not add the next proposal until the current run has a complete reviewed
entry, a final result row, and a matching `state.json` update.

The active Round 4 log uses the simplified Run/Decision schema throughout.
Raw experiment artifacts remain unchanged for reproducibility.

## ZN-E000 — CPMLP paper config, split 2021 (Kaggle T4)

- Run: `VALID`. Decision: `DISCARD` (as candidate; kept as the local split-2021 reference).
- Model: CPMLP d_model 64, d_ff 64, e_layers 5, d_layers 9, dropout 0.1, 585,665 params. Relative Huber δ 0.15, Adam lr 5e-4, batch 128, seed 2021; 9 epochs (patience stop), best epoch 1.
- Metrics: MAPE 0.4346, Acc@15 0.100 (2/20 cells). Predictions are almost constant (mean 140.4, SD 1.5) while targets span 104–1402 cycles (mean 414); long-life cells (>700) have ~0.85–0.90 error.
- Gradient: WARNING, head/intra_mlp.1 ratio 5081.9× (gate FAIL); activations finite, no dead blocks.
- Cause: representation collapse; the fixed 100-slot inter flatten (409,664 params, 70% of the model) gives static weights to padded slots (architecture.md §4) and does not separate cells.
- Codex: VALID + DISCARD; agreed after one round that the baseline recipe is not repaired (program.md §2b).
- User amendment A3-ZN: no paper-baseline re-runs on 42/2024; paper Zn-ion numbers are the reference (CPMLP 0.558±0.034 / 0.297±0.084, CPTransformer 0.515±0.067 / 0.202±0.084).
- Next: ZN-E001 = same CPMLP cycle encoder, inter flatten replaced by masked mean/last/last−first pooling (one change).

## ZN-E001 — MaskedSummaryCPMLP, split 2021 (Kaggle T4)

- Run: `VALID`. Decision: `DISCARD` (CPMLP family excluded by user rule A4-ZN; gradient gate FAIL).
- One change vs ZN-E000: inter flatten -> masked mean/last/last-first pooling. 188,353 params; 15 epochs, best epoch 10.
- Metrics: MAPE 0.3451, Acc@15 0.3405, RMSE 456, MAE 248. Pred SD 101 vs target SD 407, corr 0.26 — collapse fixed.
- EOL bands: <500 (15 cells) MAPE 0.205 / Acc@15 0.454; 500–1000 (2) MAPE 0.616, bias −511; >1000 (3) MAPE 0.863, bias −1071.
- Conclusion: mask-aware pooling over observed cycles is the right degradation-encoder mechanism; the remaining limit is long-life underprediction.
- Next: ZN-E002 = first non-CPMLP family (residual Conv1D curve encoder + masked causal TCN + the same masked summary).

## ZN-E002 — ResConv-MaskedSummary, split 2021 (Kaggle T4)

- Run: `VALID`. Decision: `DISCARD` (gradient gate FAIL: inter_mlp.0 has 55% near-zero late gradients). Direction promising (A2).
- One change vs ZN-E001: per-cycle MLP -> residual Conv1D curve encoder. 161,729 params; 12 epochs, best epoch 7.
- Metrics: MAPE 0.3368 (−0.0083), RMSE 428 (−28), Acc@15 0.318 (−0.0225), corr 0.43 (was 0.26), pred SD 119.
- EOL bands: <500 MAPE 0.208 / Acc@15 0.424; 500–1000 MAPE 0.588, bias −489; >1000 MAPE 0.813, bias −1000.
- Reviewer: Codex (after usage-limit reset). Main limit: no temporal model of fade; long-life cells still underpredicted.
- Next: ZN-E003 = add masked causal TCN with mask-aware GroupNorm (stats over observed cycles only).

## ZN-E003 — ResConv + masked causal TCN (masked GroupNorm), split 2021

- Run: `VALID`. Decision: `DISCARD` (primary rule fails on every metric; gradient gate PASS).
- One change vs ZN-E002: 6 masked causal TCN blocks (dilations 1..32) with mask-aware GroupNorm (T001). 188,993 params; 12 epochs, best epoch 7.
- Metrics: MAPE 0.3635 (+0.027), Acc@15 0.203, RMSE 454.8, DB-MAPE 0.3635. Pred SD 72 (was 119), corr 0.23 (was 0.43). Bands: 500–1000 bias −540, >1000 bias −1057.
- Process: Codex required DB-MAPE, one numeric rule, a tracked test and a CUDA smoke (ZN-E003S, T4, 92 steps, HEALTHY) before APPROVE. Kaggle rejected a 1.3 MB script; remote payload now excludes diagnostics/ and tests/.
- Conclusion: a temporal operator does not help on ZN-coin with 46 steps/epoch; it shrinks prediction spread. Long-life underprediction looks more like a loss/label-imbalance limit (15/20 val cells <500) — Phase 2 candidate (tail/cell-balanced weighting).
- Next: ZN-E004 = ZN-E002 with a 2-block head (A2 follow-up 2).

## ZN-E004 — ResConv-MaskedSummary, 2-block head, split 2021

- Run: `VALID`. Decision: `DISCARD` (§7: RMSE 442.4 > 437.8 on every path; gradient gate FAIL at inter_mlp.0, 55.08% near-zero late grads).
- One change vs ZN-E002: inter_mlp 9 -> 2 blocks. 102,593 params; 11 epochs, best epoch 6.
- Metrics: MAPE 0.3266 (best so far), Acc@15 0.355 (best), RMSE 442.4, DB-MAPE 0.3266; pred SD 117, corr 0.35.
- Bands: <500 MAPE 0.192 / Acc@15 0.473; 500–1000 bias −452; >1000 bias −1050. 3 cells >1000 cycles = 86.65% of squared error.
- Next: ZN-E005 = no inter_mlp (last A2 follow-up; a DISCARD closes the ResConv family). Code: diagnostic_modules skips an empty inter_mlp.

## ZN-E005 — ResConv-MaskedSummary, no MLP head (H0), split 2021 — KEEP

- Run: `VALID`. Decision: `KEEP` (§7 path C vs ZN-E004; gradient gate PASS, HEALTHY). First KEEP of v6; active_architecture_parent = ZN-E005.
- One change vs ZN-E004: inter_mlp 2 -> 0; head = Linear(192->64) -> ReLU -> Linear(64->1). 85,697 params; 13 epochs, best epoch 8.
- Metrics: MAPE 0.3216, Acc@15 0.4055, RMSE 445.0, DB-MAPE 0.3216; pred SD 116, corr 0.28. Paper Zn-ion 3-split: CPMLP 0.558 / 0.297, CPTransformer 0.515 / 0.202.
- Bands: <500 MAPE 0.186 / Acc@15 0.541; 500–1000 bias −469; >1000 bias −1049 (main limit).
- Next: confirm on splits 42 (ZN-E006) and 2024 (ZN-E007); then a Zn-ion charge/discharge phase-split curve encoder.

## ZN-E006 — ZN-E005 config, split 42 (confirmation)

- Run: `VALID`. Decision: `DISCARD` (gradient gate FAIL: head and inter_summary NEAR_ZERO_DOMINANT). Reviewer: Gemini 4 Argon (acting Codex; user switched reviewer 2026-11-09).
- Metrics: MAPE 0.3386, Acc@15 0.2475, RMSE 347.7, seen 0.324 / unseen 0.366. Bands: <500 (17) MAPE 0.268; 500–1000 (1) bias −596; >1000 (2) bias −932.
- Aggregate: ZN-E005 NOT CONFIRMED (needs all three KEEP). ResConv family closed (A2 follow-ups used).
- Next: ZN-E007 split 2024 for the 3-split record only; new family = phase-split curve encoder (T002 -> ZN-E008).

## ZN-E007 — ZN-E005 config, split 2024 (record)

- Run: `VALID`. Decision: `DISCARD` (gradient gate FAIL: head, inter_summary NEAR_ZERO_DOMINANT). Reviewer: Gemini 4 Argon.
- Metrics: MAPE 0.3360, Acc@15 0.3475, RMSE 356.8, seen 0.356 / unseen 0.158.
- 3-split ZN-E005 config (2021/42/2024): MAPE 0.332 ± 0.009, Acc@15 0.334 ± 0.080, RMSE 383 ± 54 — beats paper Zn-ion CPTransformer (0.515 ± 0.067 / 0.202) and CPMLP (0.558 ± 0.034 / 0.297), but NOT CONFIRMED by the gate rule.
- Family-level issue: the ReLU between inter_summary and head gives NEAR_ZERO_DOMINANT on 2 of 3 splits.
- T002 (executor Gemini 3.8): PhaseSplitResConvMaskedSummary added; refactor of ResidualCurveBlock/ResidualConvMaskedSummary verified behaviour-preserving (same state_dict and outputs for ZN-E002/E003/E005 configs; E005 checkpoint loads).

## ZN-E008 — Phase-split ResConv + masked summary + linear head, split 2021 (new family)

- Run: `VALID`. Decision: `DISCARD` (§7 paths fail; gradient gate FAIL: charge_stem 0.589, discharge_stem 0.629 near-zero). Reviewer: Gemini 4 Argon.
- One change vs ZN-E005: curve encoder split into charge/discharge branches. 49,857 params; 18 epochs, best epoch 13.
- Metrics: MAPE 0.3313, Acc@15 0.3185, RMSE 441.1, DB-MAPE 0.3313; pred SD 119, corr 0.32. Bands: <500 MAPE 0.200; 500–1000 bias −441; >1000 bias −1053 (87.5% of squared error).
- Cross-run finding: >1000-cycle cells are underpredicted by ~930–1070 cycles on every architecture and split. Relative Huber gives dL/dŷ = ±δ/y, so these cells get 6–13× smaller gradients — a loss property for Phase 2 (stat_tail_balance, cell-balanced weighting, physics P0/P4b/P2).
- Phase 1 continues (7 valid architecture runs, 3 families; needs ≥20 and ≥4).
- Next: ZN-E009 = one A2 repair: ReLU -> GELU in the phase-split stems and the readout (dead-ReLU near-zero gradients are the recurring gate failure).

## ZN-E009 — Phase-split ResConv + GELU, split 2021 (one A2 repair)

- Run: `VALID`. Decision: `DISCARD` (§7: path B RMSE 421.6 passes but MAPE 0.3369 > 0.3246; Acc@15 0.3305 < 0.3755). Gradient gate PASS.
- One change vs ZN-E008: ReLU -> GELU in stems and readout (T003). Stem near-zero 0.589/0.629 -> 0.000/0.000; readout ~0.01.
- Bands: <500 MAPE 0.221 bias +7; 500–1000 bias −414; >1000 bias −995; pred SD 141 (widest so far), corr 0.37.
- Phase-split family closed. GELU is the evidence-backed fix for the recurring dead-ReLU gate failure.
- Next: Family 4 = cycle-1 anchor / relative-fade encoder (architecture.md §2.2) on the ResConv backbone, GELU default.

## ZN-E010 — AnchoredFade-ResConv-GELU, split 2021 (Family 4 reference)

- Run: `VALID`. Decision: `DISCARD` (§7: MAPE 0.3325 > 0.3246; RMSE 425.0 misses 424.0 by 1.0; Acc@15 guard 0.3665 < 0.3755). Gradient gate PASS.
- Mechanism: per-cycle non-linear fade encoder on [h_t, h_t − h_1] before the masked summary; GELU carried default. 98,113 params; best epoch 12, stopped 17.
- Metrics: MAPE 0.3325, Acc@15 0.3665, RMSE 425.0; corr 0.48 (highest of all runs), pred SD 124. Bands: <500 MAPE 0.200 / Acc@15 0.489; 500–1000 bias −533; >1000 bias −970 (RMSE −75 vs ZN-E005).
- Reviewer (Gemini): Family 4 PROMISING under A2 (up to 3 follow-ups). Phase 1 now covers 4 families (8 valid architecture runs).
- Next: ZN-E011 = rate-aware fade input [h_t, h_t − h_1, (h_t − h_1)/t] (one change; Gemini also suggested zero-init, deferred to keep one mechanism).

## ZN-E011 — AnchoredFade + fade-rate input, split 2021 (Family 4 follow-up 1)

- Run: `VALID`. Decision: `DISCARD` (§7: RMSE 415.8 passes path B but MAPE 0.3327 > 0.3246; Acc@15 0.3045 < 0.3755). Gate PASS.
- One change vs ZN-E010: fade input [h_t, d_t] -> [h_t, d_t, d_t/max(t,1)]. 102,209 params; best epoch 8.
- Metrics: lowest RMSE (415.8) and MAE (223.5) of all runs; corr 0.46, pred SD 142. Bands: 500–1000 bias −425; >1000 bias −978.
- Reviewer finding: split-2021 MAPE of 8 architectures sits at 0.32–0.34 (pairwise per-cell MAPE corr 0.90) while RMSE improves 40 cycles — a Relative-Huber 1/y floor. Candidate for A5 early Phase-1 closure (needs user agreement).
- Next: ZN-E012 (follow-up 2) = identity-preserving relative-fade residual.

## ZN-E012 — identity-preserving relative-fade residual, split 2021 (Family 4 follow-up 2)

- Run: `VALID`. Decision: `DISCARD` (§7 path B RMSE 423.4 passes; MAPE 0.3333, Acc@15 0.3225, DB-MAPE guards fail). Gate PASS.
- Phase 1 closes early (A5-ZN, user 2026-11-09; diagnostics/phase1_breakthrough_diagnosis.md).
- Freeze candidate (Gemini, §7 among gradient-healthy runs): ZN-E010 AnchoredFade-ResConv-GELU. ZN-E005 is NOT CONFIRMED (gate FAIL on 42/2024).
- Next: confirm ZN-E010 on splits 42 (ZN-E013) and 2024 (ZN-E014); if CONFIRMED, freeze and unlock Phase 2.

## ZN-E013 — ZN-E010 config, split 42 (confirmation)

- Run: `VALID`. Decision: `KEEP` (confirmation evidence; gradient gate PASS — the GELU model passes where the ReLU ZN-E006 failed).
- Metrics: MAPE 0.3471, Acc@15 0.2470, RMSE 350.3, seen 0.337 / unseen 0.365. Bands: <500 (17) MAPE 0.275; 500–1000 bias −603; >1000 bias −949.
- Next: ZN-E014 split 2024, then the aggregate decision.

## ZN-E014 — ZN-E010 config, split 2024 (confirmation) — Phase 1 closed

- Run: `VALID`. Decision: `KEEP` (gate PASS). MAPE 0.3162, Acc@15 0.3575, RMSE 400.8, seen 0.318 / unseen 0.304. >1000-cycle Acc@15 0.123 (first non-zero), bias −827.
- Aggregate ZN-E010/E013/E014 (splits 2021/42/2024): MAPE 0.332 ± 0.016, Acc@15 0.324 ± 0.067, RMSE 392 ± 38 -> CONFIRMED (Gemini adjudication: ZN-E010's DISCARD was only vs the ZN-E005 parent thresholds; all three are VALID with gate PASS).
- Phase 1 CLOSED (A5-ZN), TARGET_NOT_MET vs 0.20 / 0.68. Frozen model: ZN-E010 AnchoredFade-ResConv-GELU (98,113 params). Beats paper Zn-ion CPTransformer (0.515 ± 0.067) and CPMLP (0.558 ± 0.034 / 0.297 ± 0.084).
- Phase 2 (loss) ACTIVE. First target: Relative-Huber dL/dŷ = ±δ/y underweights long-life cells.

## ZN-E015 — Phase 2 loss 1: frozen ZN-E010 + stat_tail_balance, split 2021

- Run: `VALID`. Decision: `DISCARD` (§7: MAPE 0.3279 misses 0.3245; Acc@15 0.2995 < 0.3365). Gradient gate PASS. Reviewer: Gemini.
- Loss: RelativeHuber_0.15 + λ2·L_tail, τ 0.55, w = clip(y/μ,1,3), λ2 = 0.1386 (ρ2 0.25, batch ratios 0.55). Active fraction 0.59 at step 250.
- Bands: 500–1000 bias −405 (ZN-E010 −533); >1000 bias −1051 (ZN-E010 −970); <500 Acc@15 0.399 (−0.09).
- Cause: d e⁻/dŷ = −1/y, so the tail term keeps the 1/y scaling; the clipped weight adds only ~1.19× pull on long-life cells.
- Process gap: summary.loss_balance is computed only for the primary physics term, so physics2 R/cosine were not logged (T007).
- Next: ZN-E016 = KDE rarity weighting (α 0.5) on the main loss (multiplicative, registered in loss.md §4.7).
