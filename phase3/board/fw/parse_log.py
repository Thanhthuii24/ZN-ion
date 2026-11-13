"""Parse an on-board stream log: keep only the lines after the last 'I,' header (drops leftovers of a previous firmware)."""
import sys, json
import numpy as np
L = open(sys.argv[1]).read().splitlines(); i0 = max(i for i, l in enumerate(L) if l.startswith("I,"))
P = np.array([[int(x) for x in l.split(",")[1:]] for l in L[i0 + 1:] if l.startswith("P,")], dtype=np.float64)
S = [l for l in L[i0 + 1:] if l.startswith("S,")]
clk = 600e6; c, n, eb, er, et, ce, cf, ch = P.T
eb, er, et = eb / 1e4, er / 1e4, et / 1e4
ms = lambda cyc: {"mean": round(float(np.mean(cyc) / clk * 1e3), 3), "min": round(float(np.min(cyc) / clk * 1e3), 3), "max": round(float(np.max(cyc) / clk * 1e3), 3),
                  "mean_excl_first_call": round(float(np.mean(cyc[1:]) / clk * 1e3), 3)}
out = {"header": L[i0], "prefixes": int(len(P)), "board_mape": float(np.mean(np.abs(eb - et) / et)), "host_ref_mape": float(np.mean(np.abs(er - et) / et)),
       "max_abs_board_minus_ref_cycles": float(np.max(np.abs(eb - er))), "acc15_board": float(np.mean(np.abs(eb - et) / et <= 0.15)),
       "ms_E": ms(ce), "ms_F": ms(cf), "ms_C_plus_H": ms(ch), "ms_total": ms(ce + cf + ch), "summary_line": S[-1] if S else None}
print(json.dumps(out, indent=1))
