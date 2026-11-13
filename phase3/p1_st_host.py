"""Phase 3 P1 (host): ST Edge AI Core analyze + chained host validation of the streaming E/F/H C-models.

Chain on all split-2021 val prefixes (val only, never test):
  E C-model on every observed cycle -> h_c;  F C-model on (h_c[t], h_c[0]) -> g_c;
  C glue (sum/first/last of g_c) -> summary;  H C-model -> y_c;  EOL = mu + s*y_c.
Reports ROM (weights), RAM (activations), MACC per part (from `stedgeai analyze`) and the end-to-end val MAPE of the
C-model chain vs the PyTorch reference. Compression via env ST_COMP (none | lossless | low | medium).

Usage: BL_SPLIT=2021 ST_COMP=none python phase3/p1_st_host.py ZN-E010
"""
import json, os, re, subprocess, sys
from pathlib import Path

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(Path(__file__).parent))
import prepare, train  # noqa: E402

ST = "/Applications/ST/STEdgeAI/4.1/Utilities/macarm/stedgeai"
X = ROOT / "phase3" / "export"
COMP = os.environ.get("ST_COMP", "none")


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def analyze(onnx, tag):
    out = X / f"st_{tag}_{COMP}"
    rc, txt = run([ST, "analyze", "-m", str(onnx), "--target", "stm32h7", "-c", COMP, "-O", "balanced",
                   "--workspace", str(out) + "_ws", "--output", str(out)])
    def grab(pat):
        m = re.search(pat, txt)
        return m.group(1).strip() if m else None
    info = {"rc": rc, "macc": grab(r"macc\s*:\s*([\d,]+)"), "weights_ro": grab(r"weights \(ro\)\s*:\s*([^\n]+)"),
            "activations_rw": grab(r"activations \(rw\)\s*:\s*([^\n]+)"), "ram_total": grab(r"ram \(total\)\s*:\s*([^\n]+)"),
            "rom_total": grab(r"rom \(total\)\s*:\s*([^\n]+)")}
    if rc:
        info["error"] = "\n".join(l for l in txt.splitlines() if "E" in l[:6] or "rror" in l)[-800:]
    return info


def validate(onnx, tag, inputs, ref):
    npz = X / f"st_{tag}_val_in.npz"
    np.savez(npz, **{f"m_inputs_{i + 1}": a.astype(np.float32) for i, a in enumerate(inputs)}, m_outputs_1=ref.astype(np.float32))
    out = X / f"st_val_{tag}_{COMP}"
    rc, txt = run([ST, "validate", "-m", str(onnx), "--target", "stm32h7", "-c", COMP, "-vi", str(npz),
                   "--workspace", str(out) + "_ws", "--output", str(out)])
    if rc:
        raise RuntimeError(f"validate {tag} failed:\n{txt[-1500:]}")
    z = np.load(out / "network_val_io.npz")
    key = [k for k in z.files if k.startswith("c_outputs")][0]
    return z[key].reshape(ref.shape)


def main(eid):
    ck = torch.load(ROOT / "experiments" / eid / "best_checkpoint.pt", map_location="cpu", weights_only=False)
    model = train.build_model(ck["model"]); model.load_state_dict(ck["state_dict"]); model.eval()
    sys.path.insert(0, str(ROOT / "phase3")); from p0_export import Enc, Fade, Head  # noqa: E402
    enc, fade, head = Enc(model).eval(), Fade(model).eval(), Head(model).eval()
    payload = prepare._load_locked_cache(); mu, s = float(payload["label_scaler"]["mean"]), float(payload["label_scaler"]["scale"])
    cells = []
    for c in payload["cells"]["val"]:
        T = np.asarray(c["tensor"], np.float32); kmax = min(100, int(c["observed_cycles"]), int(np.ceil(c["eol"])) - 1)
        cells.append((T[:kmax], float(c["eol"]), kmax))
    curves = np.concatenate([t for t, _, _ in cells])                                   # [N,3,300]
    with torch.no_grad():
        h_ref = enc(torch.from_numpy(curves)).numpy()
    parts = {p: analyze(X / f"{eid}_{p}.onnx", f"{eid}_{p}") for p in "EFH"}
    h_c = validate(X / f"{eid}_E.onnx", f"{eid}_E", [curves], h_ref)
    off = np.cumsum([0] + [k for _, _, k in cells])
    h1 = np.concatenate([np.repeat(h_c[off[i]:off[i] + 1], k, 0) for i, (_, _, k) in enumerate(cells)])
    with torch.no_grad():
        g_ref = fade(torch.from_numpy(h_c), torch.from_numpy(h1)).numpy()
    g_c = validate(X / f"{eid}_F.onnx", f"{eid}_F", [h_c, h1], g_ref)
    summ, tgt, idx = [], [], []
    for i, (_, eol, k) in enumerate(cells):
        g = g_c[off[i]:off[i] + k]; cs = np.cumsum(g, 0)
        for n in range(1, k + 1):
            summ.append(np.concatenate([cs[n - 1] / n, g[n - 1], g[n - 1] - g[0]])); tgt.append(eol); idx.append((i, n))
    summ = np.stack(summ).astype(np.float32); tgt = np.array(tgt)
    with torch.no_grad():
        y_ref = head(torch.from_numpy(summ)).numpy()
    y_c = validate(X / f"{eid}_H.onnx", f"{eid}_H", [summ], y_ref)
    eol_c = mu + s * y_c.reshape(-1)
    # PyTorch full-model reference on the same prefixes
    xs, ms = [], []
    for i, n in idx:
        T = np.zeros((100, 3, 300), np.float32); T[:n] = cells[i][0][:n]; m = np.zeros(100, np.float32); m[:n] = 1
        xs.append(T); ms.append(m)
    with torch.no_grad():
        eol_t = np.concatenate([model(torch.from_numpy(np.stack(xs[j:j + 256])), torch.from_numpy(np.stack(ms[j:j + 256]))).numpy()
                                for j in range(0, len(xs), 256)]) * s + mu
    rel = np.abs(eol_c - eol_t) / np.maximum(np.abs(eol_t), 1.0)
    rep = {"experiment": eid, "split": prepare.SPLIT, "compression": COMP, "parts": parts, "val_prefixes": len(idx),
           "val_mape_c_chain": float(np.mean(np.abs(eol_c - tgt) / tgt)), "val_mape_torch": float(np.mean(np.abs(eol_t - tgt) / tgt)),
           "max_rel_eol_diff_c_vs_torch": float(rel.max()), "mean_rel_eol_diff": float(rel.mean())}
    (X / f"{eid}_p1_{COMP}.json").write_text(json.dumps(rep, indent=1))
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main(sys.argv[1])
