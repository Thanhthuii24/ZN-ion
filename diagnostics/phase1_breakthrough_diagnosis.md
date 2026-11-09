# Phase-1 breakthrough diagnosis — v6_auto (ZN-coin), 2026-11-09

Purpose: evidence for early Phase-1 closure under `program.md §10` (amendment A5; user agreement A5-ZN, 2026-11-09).

## 1. Shared error that architecture changes do not move

Runs: ZN-E001…E012 on split 2021 (plus ZN-E006/E007 on splits 42/2024). Nine architectures in four families:
CPMLP-derived (E001; excluded as candidate), residual Conv1D curve encoder (E002, E004, E005), masked causal TCN (E003),
phase-split encoder (E008, E009), anchored fade encoder (E010–E012).

| Evidence | Value |
|---|---|
| Split-2021 MAPE across architectures (E002–E011) | 0.3216 – 0.3635; 8 of 9 within 0.32–0.34 |
| Mean pairwise correlation of per-cell MAPE (9 architectures, 20 val cells) | 0.905 |
| Share of total MAPE from the 5 cells with EOL > 500 (25% of cells) | 0.51 – 0.57 on every architecture |
| Bias of the 3 cells with EOL > 1000 | −930 … −1070 cycles on every architecture and split; Acc@15 = 0 |
| Bias of the 1–2 cells with EOL 500–1000 | −414 … −600 cycles |
| RMSE meanwhile | 456 → 416 (E001 → E011) |

Architectures change RMSE and correlation (0.26 → 0.48) but not this per-cell error pattern.

## 2. Why: a loss property, not an architecture property

The locked Phase-1 loss is Relative Huber on e = (ŷ − y)/y with δ = 0.15. In the linear regime its gradient w.r.t.
the prediction is dL/dŷ = ±δ / y. For the long-life cells (mean y ≈ 1235) the per-sample gradient is 6–13× smaller than
for the short-life cells (mean y ≈ 192, 75% of the cohort). The optimiser therefore trades long-life accuracy for
short-life accuracy; every model converges to underpredicting the tail. The bias is physically inconsistent: cells with
slow observed fade are predicted to fail as early as fast-fading cells.

## 3. A4 directions

- D1-like (trajectory relative to the cell's own early state): tested as the anchored fade family (E010–E012; cycle-1
  relative fade and fade rate). It improved RMSE and correlation, not the shared MAPE pattern.
- D2 (reference-pair inter-cell learning) and D3 (learned-shape degradation head): not run in v6. The user waived them
  in the A5 agreement (A5-ZN).

## 4. Consequence

Phase 1 closes early (A5). The best candidate by `program.md §7` must still receive a three-seed confirmation before
freezing; Phase 2 then tests losses that rebalance the tail (e.g. `stat_tail_balance`, cell-balanced weighting) and
label-free physics terms (loss.md), which target exactly the mechanism in §2.
