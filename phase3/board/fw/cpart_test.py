"""Host check: zn_cpart.c (compiled with the host cc) == summaries() of phase3/board/st_int8.py on real val g states."""
import subprocess, sys, tempfile
from pathlib import Path
import numpy as np, onnxruntime as ort
FW = Path(__file__).resolve().parent; ROOT = FW.parents[2]; sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(FW.parent))
import prepare
from st_int8 import summaries, RUNS
eid = RUNS[prepare.SPLIT]; X = ROOT / "phase3" / "export"
sE = ort.InferenceSession(str(X / f"{eid}_E.onnx")); sF = ort.InferenceSession(str(FW.parent / f"{eid}_F_int8.onnx"))
td = Path(tempfile.mkdtemp()); exe = td / "cpart_test"
subprocess.run(["cc", "-O2", "-o", str(exe), str(FW / "cpart_test.c")], check=True)
worst = 0.0; n_pref = 0
for c in prepare._load_locked_cache()["cells"]["val"]:
    T = np.asarray(c["tensor"], np.float32); k = min(100, int(c["observed_cycles"]), int(np.ceil(c["eol"])) - 1)
    h = np.concatenate([sE.run(None, {"curve": T[t:t + 1]})[0] for t in range(k)])
    g = np.concatenate([sF.run(None, {"h": h[t:t + 1], "h1": h[:1]})[0] for t in range(k)]).astype(np.float32)
    g.tofile(td / "g.bin"); subprocess.run([str(exe), str(td / "g.bin"), str(td / "s.bin")], check=True)
    sc = np.fromfile(td / "s.bin", np.float32).reshape(k, -1); sp = summaries(g.astype(np.float64)).astype(np.float32)   # float64 reference (the C glue sums in double)
    worst = max(worst, float(np.max(np.abs(sc - sp) / np.maximum(np.abs(sp), 1e-3)))); n_pref += k
print(f"{eid} split {prepare.SPLIT}: zn_cpart.c vs Python summaries on {n_pref} val prefixes: max rel diff {worst:.2e}")
