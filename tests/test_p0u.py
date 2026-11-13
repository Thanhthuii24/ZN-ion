"""T008 tests: P0u capacity-improvement what-if (loss.md §4.6)."""
import json, subprocess, sys, importlib.util, torch
sys.path.insert(0, ".")
import train

torch.manual_seed(0)
x = torch.rand(3, 100, 3, 300); m = torch.zeros(3, 100); m[:, :60] = 1; x = x * m[..., None, None]
a = torch.tensor([0.1, 0.05, 0.15])
xt = train.apply_whatif(x, m, a, physics_type="whatif_improve_p0u")
r = ((torch.arange(1, 101, dtype=x.dtype) - 10) / 90).clamp(0, 1)
exp = x.clone(); exp[:, :, 2, 150:] = x[:, :, 2, 150:] * (1 + a[:, None] * r[None] * m)[..., None]
assert torch.allclose(xt, exp), "what-if formula"
assert torch.equal(xt[:, :10], x[:, :10]), "cycles 1-10 unchanged"
assert torch.equal(xt[:, 60:], x[:, 60:]), "masked cycles unchanged"
assert torch.equal(xt[:, :, :2], x[:, :, :2]) and torch.equal(xt[:, :, 2, :150], x[:, :, 2, :150]), "only discharge Q"

class Mono(torch.nn.Module):          # prediction = sign * mean discharge capacity
    def __init__(s, sign): super().__init__(); s.sign = sign; s.w = torch.nn.Parameter(torch.ones(1))
    def forward(s, c, mk): return s.sign * s.w * (c[:, :, 2, 150:].mean(-1) * mk).sum(1) / mk.sum(1)
bounds = {"__global__": 0.15, "ZN-coin": 0.15}
batch_dom = ["ZN-coin"] * 3
for sign, positive in ((1.0, False), (-1.0, True)):
    mdl = Mono(sign); f = mdl(x, m)
    out = train.whatif_loss(mdl, x, m, batch_dom, bounds_by_domain=bounds, rng=torch.Generator().manual_seed(1), physics_type="whatif_improve_p0u", predictions=f)
    val = out[0] if isinstance(out, tuple) else out
    assert (float(val) > 0) == positive, f"sign {sign}: loss {float(val)}"
print("T008 P0u tests passed")
