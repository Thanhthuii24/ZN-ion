"""Phase 3 board, step A2 (v6 Zn-ion): static INT8 (QDQ, per-channel, symmetric weights) of the streaming E/F/H models.
Follows v5_auto/phase3/board/st_int8.py: Constant tensors moved to initializers, calibration on TRAIN cells only.
Evaluates on all val prefixes of the split (never test):
  (1) onnxruntime INT8 chain E -> F -> C glue -> H,
  (2) ST C-model INT8 chain on host (`stedgeai validate --input-data-type float32 --output-data-type float32`),
  and compares the val MAPE with the fp32 chain (pass: delta <= +0.001).
Usage: BL_SPLIT=2021 python phase3/board/st_int8.py          (env EXCL_E/EXCL_F/EXCL_H = nodes kept in float)
"""
import json, os, subprocess, sys
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
from onnxruntime.quantization import CalibrationDataReader, CalibrationMethod, QuantFormat, QuantType, quantize_static

ROOT = Path(__file__).resolve().parents[2]; sys.path.insert(0, str(ROOT))
import prepare  # noqa: E402

ST = "/Applications/ST/STEdgeAI/4.1/Utilities/macarm/stedgeai"
X = ROOT / "phase3" / "export"; B = ROOT / "phase3" / "board"
RUNS = {"2021": "ZN-E010", "42": "ZN-E013", "2024": "ZN-E014"}


def const_to_init(src, dst):
    m = onnx.load(src); keep = []
    for n in m.graph.node:
        if n.op_type == "Constant" and n.attribute[0].name == "value":
            t = onnx.helper.get_attribute_value(n.attribute[0]); t.name = n.output[0]; m.graph.initializer.append(t)
        else:
            keep.append(n)
    del m.graph.node[:]; m.graph.node.extend(keep); onnx.save(m, dst); return dst


class Reader(CalibrationDataReader):
    def __init__(s, items): s.it = iter(items)
    def get_next(s): return next(s.it, None)


def sess(p): return ort.InferenceSession(str(p), providers=["CPUExecutionProvider"])


def summaries(g):
    """C glue: summary for every prefix n = 1..k from per-cycle g [k,64]."""
    cs = np.cumsum(g, 0); n = np.arange(1, len(g) + 1)[:, None]
    return np.concatenate([cs / n, g, g - g[:1]], axis=1).astype(np.float32)


def chain(sE, sF, sH, cells):
    """Run E -> F -> C glue -> H for all prefixes; returns EOL predictions (standardized) and per-cell lists."""
    ys = []
    for T, _, k in cells:
        h = np.concatenate([sE.run(None, {"curve": T[t:t + 1]})[0] for t in range(k)])
        g = np.concatenate([sF.run(None, {"h": h[t:t + 1], "h1": h[:1]})[0] for t in range(k)])
        S = summaries(g)
        ys.append(np.array([sH.run(None, {"summary": S[n:n + 1]})[0].item() for n in range(k)]))
    return ys


def st_validate(model, inputs, tag):
    npz = B / f"{tag}_in.npz"
    np.savez(npz, **{f"m_inputs_{i + 1}": a.astype(np.float32) for i, a in enumerate(inputs)})
    out = B / f"val_{tag}"
    r = subprocess.run([ST, "validate", "-m", str(model), "--target", "stm32h7", "--input-data-type", "float32",
                        "--output-data-type", "float32", "-vi", str(npz), "--workspace", str(out) + "_ws", "--output", str(out)],
                       capture_output=True, text=True)
    txt = r.stdout + r.stderr
    w = [l.strip() for l in txt.splitlines() if "weights (ro)" in l or "activations (rw)" in l or "ERROR" in l or "NOT IMPL" in l]
    if r.returncode:
        raise RuntimeError(f"{tag}: {txt[-1500:]}")
    z = np.load(out / "network_val_io.npz")
    return z[[k for k in z.files if k.startswith("c_outputs")][0]], w


def main():
    split = prepare.SPLIT; eid = RUNS[split]; rng = np.random.default_rng(0)
    payload = prepare._load_locked_cache(); mu, s = float(payload["label_scaler"]["mean"]), float(payload["label_scaler"]["scale"])
    fp = {p: sess(X / f"{eid}_{p}.onnx") for p in "EFH"}
    load = lambda key: [(np.asarray(c["tensor"], np.float32), float(c["eol"]),
                         min(100, int(c["observed_cycles"]), int(np.ceil(c["eol"])) - 1)) for c in payload["cells"][key]]
    train_cells, val_cells = load("train"), load("val")
    # ---- calibration on TRAIN cells only (fp32 chain provides F/H inputs) ----
    calE, calF, calH = [], [], []
    for T, _, k in train_cells:
        h = np.concatenate([fp["E"].run(None, {"curve": T[t:t + 1]})[0] for t in range(k)])
        g = np.concatenate([fp["F"].run(None, {"h": h[t:t + 1], "h1": h[:1]})[0] for t in range(k)])
        S = summaries(g)
        for t in rng.choice(k, min(12, k), replace=False):
            calE.append({"curve": T[t:t + 1]}); calF.append({"h": h[t:t + 1], "h1": h[:1]})
        for n in rng.choice(k, min(12, k), replace=False):
            calH.append({"summary": S[n:n + 1]})
    for p, cal in (("E", calE), ("F", calF), ("H", calH)):
        ci = const_to_init(str(X / f"{eid}_{p}.onnx"), str(B / f"{eid}_{p}_ci.onnx"))
        quantize_static(ci, str(B / f"{eid}_{p}_int8.onnx"), Reader(cal), quant_format=QuantFormat.QDQ, per_channel=True,
                        weight_type=QuantType.QInt8, activation_type=QuantType.QInt8, op_types_to_quantize=["Conv", "Gemm", "MatMul"],
                        extra_options={"WeightSymmetric": True, "ActivationSymmetric": False},
                        calibrate_method=getattr(CalibrationMethod, os.environ.get("CAL", "MinMax")),
                        nodes_to_exclude=[x for x in os.environ.get(f"EXCL_{p}", "").split(",") if x])
    tgt = np.concatenate([np.full(k, eol) for _, eol, k in val_cells])
    mape = lambda ys: float(np.mean(np.abs(mu + s * np.concatenate(ys) - tgt) / tgt))
    m_fp = mape(chain(fp["E"], fp["F"], fp["H"], val_cells))
    FP = set(x for x in os.environ.get("FP_PARTS", "").split(",") if x)            # parts kept in fp32 (mixed precision)
    mdl = {p: (X / f"{eid}_{p}.onnx") if p in FP else (B / f"{eid}_{p}_int8.onnx") for p in "EFH"}
    q = {p: sess(mdl[p]) for p in "EFH"}
    m_ort = mape(chain(q["E"], q["F"], q["H"], val_cells))
    # ---- ST C-model INT8 chain on host: E C-outputs -> F -> C glue -> H ----
    curves = np.concatenate([T[:k] for T, _, k in val_cells])
    hC, wE = st_validate(mdl["E"], [curves], f"{eid}_E_{'fp' if 'E' in FP else 'int8'}"); hC = hC.reshape(-1, 64)
    off = np.cumsum([0] + [k for *_, k in val_cells])
    h1 = np.concatenate([np.repeat(hC[off[i]:off[i] + 1], k, 0) for i, (*_, k) in enumerate(val_cells)])
    gC, wF = st_validate(mdl["F"], [hC, h1], f"{eid}_F_{'fp' if 'F' in FP else 'int8'}"); gC = gC.reshape(-1, 64)
    S = np.concatenate([summaries(gC[off[i]:off[i] + k]) for i, (*_, k) in enumerate(val_cells)])
    yC, wH = st_validate(mdl["H"], [S], f"{eid}_H_{'fp' if 'H' in FP else 'int8'}")
    m_st = float(np.mean(np.abs(mu + s * yC.reshape(-1) - tgt) / tgt))
    rep = {"experiment": eid, "split": split, "val_prefixes": int(len(tgt)), "calibration": f"train cells only, {len(calE)} E / {len(calH)} H samples",
           "val_mape_fp32_chain": m_fp, "val_mape_ort_int8_chain": m_ort, "val_mape_st_c_int8_chain": m_st,
           "delta_st_int8_vs_fp32": m_st - m_fp, "pass_delta_0.001": bool(m_st - m_fp <= 0.001),
           "st_report": {"E": wE, "F": wF, "H": wH}, "excluded": {p: os.environ.get(f"EXCL_{p}", "") for p in "EFH"}, "fp32_parts": sorted(FP)}
    (B / f"{eid}_int8_report{('_fp' + ''.join(sorted(FP))) if FP else ''}.json").write_text(json.dumps(rep, indent=1))
    print(json.dumps(rep, indent=1))


if __name__ == "__main__":
    main()
