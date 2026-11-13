"""Phase 3 P0 (host): ST-compatible streaming export of the frozen AnchoredFade-ResConv-GELU model.

Split (no `If`, no dynamic gather, static batch 1, opset 17):
  E  per-cycle encoder   curve[1,3,300]          -> h[1,64]
  F  per-cycle fade cell h[1,64], h1[1,64]        -> g[1,64]   (g = fade([h, h-h1]) + h)
  H  head (once)         summary[1,192]          -> y[1,1]    (standardized EOL)
  C  glue                sum/first/last of g, summary = [mean, last, last-first], EOL = mu + s*y

Check: the streaming pipeline run with onnxruntime reproduces the PyTorch model on every val prefix
(max relative EOL difference <= 1e-4). Val split only; the test split is never read.

Usage: BL_SPLIT=2021 python phase3/p0_export.py ZN-E010
"""
import json, sys, time
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import torch
import torch.nn as nn
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import prepare, train  # noqa: E402

OUT = ROOT / "phase3" / "export"


class Enc(nn.Module):
    def __init__(self, m):
        super().__init__(); self.stem, self.res, self.pool = m.cycle_stem, m.residual_blocks, m.curve_pool

    def forward(self, curve):                      # [1,3,300]
        return self.pool(self.res(self.stem(curve))).flatten(1)   # [1,64]


class Fade(nn.Module):
    def __init__(self, m):
        super().__init__(); self.fade = m.fade_encoder

    def forward(self, h, h1):                      # [1,64], [1,64]
        return self.fade(torch.cat([h, h - h1], dim=-1)) + h


class Head(nn.Module):
    def __init__(self, m):
        super().__init__(); self.summ, self.head, self.gelu = m.inter_summary, m.head, m.activation == "gelu"

    def forward(self, summary):                    # [1,192]
        v = self.summ(summary)
        v = F.gelu(v) if self.gelu else F.relu(v)
        return self.head(v)                        # [1,1]


def export(module, args, names, outs, path):
    torch.onnx.export(module, args, str(path), input_names=names, output_names=outs, opset_version=17,
                      dynamic_axes=None, do_constant_folding=True, dynamo=False)
    model = onnx.load(str(path))
    try:
        from onnxsim import simplify
        model, ok = simplify(model)
        if ok:
            onnx.save(model, str(path))
    except Exception as exc:  # simplification is optional
        print("onnxsim skipped:", exc)
    onnx.checker.check_model(model)
    ops = sorted({n.op_type for n in model.graph.node})
    return ops


def main(eid):
    ck = torch.load(ROOT / "experiments" / eid / "best_checkpoint.pt", map_location="cpu", weights_only=False)
    cfg = ck["model"]
    assert cfg["variant"] == "anchored_fade_resconv_masked_summary" and cfg.get("fade_input", "anchor") == "anchor"
    assert cfg.get("mlp_layers", 0) == 0 and cfg.get("summary_mode") == "mean_last_delta"
    model = train.build_model(cfg); model.load_state_dict(ck["state_dict"]); model.eval()
    OUT.mkdir(parents=True, exist_ok=True)
    d = cfg["d_model"]
    paths = {k: OUT / f"{eid}_{k}.onnx" for k in "EFH"}
    ops = {
        "E": export(Enc(model), (torch.zeros(1, 3, 300),), ["curve"], ["h"], paths["E"]),
        "F": export(Fade(model), (torch.zeros(1, d), torch.zeros(1, d)), ["h", "h1"], ["g"], paths["F"]),
        "H": export(Head(model), (torch.zeros(1, 3 * d),), ["summary"], ["y"], paths["H"]),
    }
    banned = {"If", "Loop", "MatMulInteger", "ConvInteger", "DynamicQuantizeLinear", "Shape", "Range",
              "ConstantOfShape", "GatherElements", "NonZero"}
    found = {k: sorted(set(v) & banned) for k, v in ops.items()}

    payload = prepare._load_locked_cache()
    scaler = payload["label_scaler"]; mu, s = float(scaler["mean"]), float(scaler["scale"])
    sess = {k: ort.InferenceSession(str(p), providers=["CPUExecutionProvider"]) for k, p in paths.items()}
    t0 = time.time(); rel, n = [], 0
    calib_e, calib_f, calib_h, val_io = [], [], [], []
    for cell in payload["cells"]["val"]:
        T = torch.as_tensor(np.asarray(cell["tensor"]), dtype=torch.float32)          # [100,3,300]
        kmax = min(100, int(cell["observed_cycles"]), int(np.ceil(cell["eol"])) - 1)
        hs = [sess["E"].run(None, {"curve": T[t:t + 1].numpy()})[0] for t in range(kmax)]
        gs = [sess["F"].run(None, {"h": hs[t], "h1": hs[0]})[0] for t in range(kmax)]
        csum = np.zeros((1, d), np.float32)
        x_all, m_all, k_all = [], [], []
        for k in range(1, kmax + 1):
            csum = csum + gs[k - 1]
            summary = np.concatenate([csum / k, gs[k - 1], gs[k - 1] - gs[0]], axis=-1).astype(np.float32)
            y = sess["H"].run(None, {"summary": summary})[0].item()
            m = torch.zeros(1, 100); m[0, :k] = 1
            x_all.append((T[None] * m[..., None, None])); m_all.append(m); k_all.append((k, summary, mu + s * y))
        with torch.no_grad():
            ref = model(torch.cat(x_all), torch.cat(m_all)).numpy() * s + mu
        for (k, summary, eol_onnx), r in zip(k_all, ref):
            rel.append(abs(eol_onnx - r) / max(abs(r), 1.0)); n += 1
            calib_h.append(summary[0]); val_io.append((cell["filename"], k, float(r), float(eol_onnx), float(cell["eol"])))
        calib_e.extend(T[:kmax].numpy()); calib_f.extend(np.concatenate([np.concatenate(hs), np.repeat(hs[0], kmax, 0)], -1))
    rel = np.array(rel)
    # host validation sets for stedgeai (val inputs and PyTorch reference outputs), small and deterministic
    rng = np.random.default_rng(2021)
    pick = lambda a, k: np.asarray(a)[np.sort(rng.choice(len(a), size=min(k, len(a)), replace=False))]
    np.savez(OUT / f"{eid}_E_val.npz", curve=pick(calib_e, 200)[:, None] if False else pick(calib_e, 200))
    np.savez(OUT / f"{eid}_H_val.npz", summary=pick(calib_h, 200))
    np.savez(OUT / f"{eid}_val_io.npz", io=np.array([(a, b, c, e, f) for a, b, c, e, f in val_io], dtype=object), allow_pickle=True)
    pred = np.array([v[3] for v in val_io]); tgt = np.array([v[4] for v in val_io])
    report = {
        "experiment": eid, "split": prepare.SPLIT, "onnx": {k: str(p.relative_to(ROOT)) for k, p in paths.items()},
        "onnx_bytes": {k: p.stat().st_size for k, p in paths.items()}, "ops": ops, "banned_ops_found": found,
        "val_prefixes": n, "max_rel_eol_diff": float(rel.max()), "mean_rel_eol_diff": float(rel.mean()),
        "pass_1e-4": bool(rel.max() <= 1e-4), "onnx_pipeline_val_mape": float(np.mean(np.abs(pred - tgt) / tgt)),
        "params": int(sum(p.numel() for p in model.parameters())), "label_mu": mu, "label_s": s,
        "seconds": round(time.time() - t0, 1),
    }
    (OUT / f"{eid}_p0_report.json").write_text(json.dumps(report, indent=1))
    json.dump({"label_mu": mu, "label_s": s, "d_model": d, "summary": "[mean, last, last-first] of g over observed cycles"},
              open(OUT / f"{eid}_C_params.json", "w"), indent=1)
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main(sys.argv[1])
