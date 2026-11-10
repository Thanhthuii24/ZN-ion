# ZN-E010 held-out what-if violation rates (split 2021 val, 2026-11-10)

Frozen ZN-E010 checkpoint, local CPU, train.evaluate_whatif_violation_rate, bounds from train cells
(alpha_max P0/P0c 0.150, P2 beta_max 0.0079, P4b kappa_max 1.0). Note: run took 1,472 s, above the
program.md §9 two-minute cap for read-only diagnostics (lead error; future what-if diagnostics run on Kaggle GPU).

| Rule | alpha 0.25 | alpha 0.5 | alpha 1.0 |
|---|---|---|---|
| P0 uniform fade | 0.1485 | 0.1485 | 0.1500 |
| P0c knee | 0.1460 | 0.1470 | 0.1490 |
| P4b falling-trend acceleration | 0.2883 | 0.2975 | 0.3031 |
| P2 voltage sag | 0.2250 | 0.2255 | 0.2255 |

The ZN-E016S smoke reported P0 violation 0 and active fraction 0 because it is a 92-step model; the trained
frozen model violates P0 on ~15% of validation samples, so P0 has work to do.
