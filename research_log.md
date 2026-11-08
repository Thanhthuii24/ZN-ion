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
