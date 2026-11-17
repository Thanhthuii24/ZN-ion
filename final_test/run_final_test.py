"""One-time final-test evaluation of the frozen v6 models (user authorization A10-ZN, 2026-11-17).
Models: ZN-E010 config (E010/E013/E014), P0c (E018/E019/E020), S2 compressed (E023/E024/E025); saved checkpoints,
no retraining, no selection on test. Usage: python final_test/run_final_test.py"""
import json, os, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
RUNS = {"ZN-E010 AnchoredFade-ResConv-GELU": {"2021": "ZN-E010", "42": "ZN-E013", "2024": "ZN-E014"},
        "P0c physics-informed": {"2021": "ZN-E018", "42": "ZN-E019", "2024": "ZN-E020"},
        "S2 compressed": {"2021": "ZN-E023", "42": "ZN-E024", "2024": "ZN-E025"}}
if len(sys.argv) == 3:  # worker: one split, one experiment
    split, eid = sys.argv[1], sys.argv[2]
    sys.path.insert(0, str(ROOT)); import prepare, train, torch
    ck = torch.load(ROOT / "experiments" / eid / "best_checkpoint.pt", map_location="cpu", weights_only=False)
    model = train.build_model(ck["model"]); model.load_state_dict(ck["state_dict"]); model.eval()
    loader, scaler = prepare.load_final_test_data(256)
    preds, tgts = [], []
    with torch.no_grad():
        for b in loader:
            y = model(b["cycle_curve_data"], b["curve_attn_mask"]).numpy() * scaler.scale + scaler.mean
            preds.extend(y.tolist()); tgts.extend(b["raw_eol"].numpy().tolist())
    m = prepare.compute_metrics(preds, tgts); m.update(n_samples=len(tgts), n_cells=len(loader.dataset.cells))
    print(json.dumps(m)); sys.exit(0)
out = {}
for name, runs in RUNS.items():
    out[name] = {}
    for split, eid in runs.items():
        r = subprocess.run([sys.executable, __file__, split, eid], capture_output=True, text=True, env=dict(os.environ, BL_SPLIT=split), cwd=ROOT)
        if r.returncode: raise SystemExit(f"{name} {split} {eid}: {r.stderr[-800:]}")
        out[name][split] = dict(json.loads(r.stdout.strip().splitlines()[-1]), experiment=eid)
        print(name, split, eid, out[name][split])
(ROOT / "final_test" / "final_test_results.json").write_text(json.dumps(out, indent=1))
