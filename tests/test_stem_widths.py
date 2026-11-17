import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

import torch
import torch.nn as nn
import torch.nn.functional as F

from train import (
    AnchoredFadeResConvMaskedSummary,
    ResidualConvMaskedSummary,
    build_model,
)
import workflow_preflight


class OldAnchoredFadeResConvMaskedSummary(ResidualConvMaskedSummary):
    """Reference implementation of AnchoredFadeResConvMaskedSummary before T009."""

    def __init__(
        self,
        d_model=64,
        residual_layers=2,
        d_ff=64,
        mlp_layers=0,
        summary_mode="mean_last_delta",
        dropout=0.0,
        activation="gelu",
        fade_hidden=64,
        fade_input="anchor",
    ):
        if activation not in {"relu", "gelu"}:
            raise ValueError(
                f"activation must be 'relu' or 'gelu', got {activation!r}"
            )
        if fade_input not in {"anchor", "anchor_rate", "relative_rate"}:
            raise ValueError(
                f"fade_input must be 'anchor', 'anchor_rate', or 'relative_rate', got {fade_input!r}"
            )
        self.activation = activation
        self.fade_hidden = fade_hidden
        self.fade_input = fade_input
        super().__init__(
            d_model=d_model,
            residual_layers=residual_layers,
            d_ff=d_ff,
            mlp_layers=mlp_layers,
            summary_mode=summary_mode,
            dropout=dropout,
        )
        if activation == "gelu":
            self.cycle_stem = nn.Sequential(
                nn.Conv1d(3, 32, kernel_size=7, stride=2, padding=3),
                nn.GELU(),
                nn.Conv1d(32, 64, kernel_size=5, stride=2, padding=2),
                nn.GELU(),
                nn.Conv1d(64, d_model, kernel_size=3, stride=2, padding=1),
                nn.GELU(),
            )
        act_module = nn.GELU() if activation == "gelu" else nn.ReLU()
        fade_in_dim = 3 * d_model if fade_input == "anchor_rate" else 2 * d_model
        self.fade_encoder = nn.Sequential(
            nn.Linear(fade_in_dim, fade_hidden),
            act_module,
            nn.Linear(fade_hidden, d_model),
        )
        if fade_input == "relative_rate":
            nn.init.zeros_(self.fade_encoder[-1].weight)
            if self.fade_encoder[-1].bias is not None:
                nn.init.zeros_(self.fade_encoder[-1].bias)

    def readout_activation(self, values):
        if self.activation == "gelu":
            return F.gelu(values)
        return super().readout_activation(values)

    def encode_cycle_states(self, curves, mask=None):
        h = self.encode_curve_features(curves)
        h_1 = h[:, 0:1, :]
        d = h - h_1
        if self.fade_input == "anchor":
            u = torch.cat([h, d], dim=-1)
            g = self.fade_encoder(u) + h
        elif self.fade_input == "anchor_rate":
            t = torch.arange(h.size(1), device=h.device, dtype=h.dtype).view(1, -1, 1)
            rate = d / torch.clamp(t, min=1.0)
            u = torch.cat([h, d, rate], dim=-1)
            g = self.fade_encoder(u) + h
        elif self.fade_input == "relative_rate":
            t = torch.arange(h.size(1), device=h.device, dtype=h.dtype).view(1, -1, 1)
            rate = d / torch.clamp(t, min=1.0)
            u = torch.cat([d, rate], dim=-1)
            c = (t > 0).to(dtype=h.dtype)
            g = h + c * self.fade_encoder(u)
        else:
            raise ValueError(f"Unknown fade_input: {self.fade_input!r}")
        if mask is not None:
            g = g * mask.to(g.dtype).unsqueeze(-1)
        return g


def test_default_stem_widths_identical():
    """Verify default stem_widths gives byte-for-byte identical state_dict and outputs for both relu and gelu."""
    for act in ["gelu", "relu"]:
        seed = 42
        torch.manual_seed(seed)
        old_model = OldAnchoredFadeResConvMaskedSummary(activation=act).eval()

        torch.manual_seed(seed)
        new_model = AnchoredFadeResConvMaskedSummary(activation=act).eval()

        old_sd = old_model.state_dict()
        new_sd = new_model.state_dict()
        assert old_sd.keys() == new_sd.keys(), f"Keys mismatch for activation={act}"
        for k in old_sd:
            assert torch.equal(old_sd[k], new_sd[k]), (
                f"Weights mismatch for {k} under activation={act}"
            )

        torch.manual_seed(123)
        curves = torch.randn(2, 50, 3, 300)
        mask = torch.zeros(2, 50)
        mask[:, :30] = 1.0

        with torch.no_grad():
            out_old = old_model(curves, mask)
            out_new = new_model(curves, mask)

        assert torch.equal(out_old, out_new), (
            f"Outputs differ for activation={act}: max diff = {(out_old - out_new).abs().max()}"
        )


def test_zne010_config_identical():
    """Verify default stem_widths gives identical outputs to current code for the ZN-E010 config."""
    config_zne010 = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
    }

    seed = 2021
    torch.manual_seed(seed)
    old_model = OldAnchoredFadeResConvMaskedSummary(
        d_model=64,
        residual_layers=2,
        d_ff=64,
        mlp_layers=0,
        summary_mode="mean_last_delta",
        dropout=0.1,
        activation="gelu",
        fade_hidden=64,
    ).eval()

    torch.manual_seed(seed)
    new_model = build_model(config_zne010).eval()

    torch.manual_seed(777)
    curves = torch.randn(4, 80, 3, 300)
    mask = torch.zeros(4, 80)
    mask[:, :45] = 1.0

    with torch.no_grad():
        out_old = old_model(curves, mask)
        out_new = new_model(curves, mask)

    assert torch.equal(out_old, out_new), (
        f"ZN-E010 output mismatch: max diff = {(out_old - out_new).abs().max()}"
    )


def test_narrow_stem_widths():
    """stem_widths (16, 32) with d_model 32, residual_layers 1, fade_hidden 32 builds, is finite and padding-invariant; print its parameter count."""
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "stem_widths": (16, 32),
        "d_model": 32,
        "residual_layers": 1,
        "fade_hidden": 32,
        "activation": "gelu",
    }
    model = build_model(config).eval()

    assert model.stem_widths == (16, 32)
    assert model.cycle_stem[0].in_channels == 3 and model.cycle_stem[0].out_channels == 16
    assert model.cycle_stem[2].in_channels == 16 and model.cycle_stem[2].out_channels == 32
    assert model.cycle_stem[4].in_channels == 32 and model.cycle_stem[4].out_channels == 32

    # Check finite
    torch.manual_seed(42)
    curves = torch.randn(2, 60, 3, 300)
    mask = torch.zeros(2, 60)
    mask[:, :25] = 1.0

    with torch.no_grad():
        out_clean = model(curves, mask)
    assert torch.isfinite(out_clean).all(), "Outputs must be finite"

    # Check padding-invariance
    curves_noisy = curves.clone()
    curves_noisy[:, 25:] += torch.randn_like(curves_noisy[:, 25:]) * 30.0
    with torch.no_grad():
        out_noisy = model(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"Padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )

    n_params = sum(p.numel() for p in model.parameters())
    print(f"Narrow model (stem=(16, 32), d_model=32, residual=1, fade_hidden=32) parameter count: {n_params}")
    assert n_params > 0


def test_phase3_preflight_rules():
    """Verify workflow_preflight.py enforces phase3_tinyml constraints."""
    assert "compression" in workflow_preflight.EXPERIMENT_TYPES

    real_state = workflow_preflight.load_json("state.json")
    real_lock = workflow_preflight.load_json("protocol_lock.json")

    mock_state = dict(real_state)
    mock_state["phase"] = "phase3_tinyml"
    mock_state["research_mode"] = "tinyml_compression"
    mock_state["next_experiment_id"] = "ZN-E099"

    base_p3_proposal = {
        "id": "ZN-E099",
        "phase": "phase3_tinyml",
        "training_stage": "full_candidate",
        "type": "compression",
        "observation": "test",
        "primary_hypothesis": "test",
        "question_inherited": "test",
        "unresolved_from_reference": "test",
        "why_this_experiment": "test",
        "modification": "test",
        "changed_components": "test",
        "held_constant_components": "test",
        "controlled_variables": "test",
        "expected_evidence": "test",
        "rejection_condition": "test",
        "minimum_evidence_requirements": "test",
        "required_diagnostics": "test",
        "estimated_budget": {"experiments": 1, "training_seconds": 600},
        "model": {"display_name": "TestModel"},
        "reference_experiment": "ZN-E010",
        "training": {
            "minimum_epochs": 5,
            "max_epochs": 100,
            "main_loss": "relative_huber",
            "relative_huber_delta": 0.15,
            "learning_rate": 0.0005,
            "batch_size": 128,
            "physics_loss_weight": 0.0,
        },
    }

    original_load = workflow_preflight.load_json

    def check_proposal(prop_data, expected_err_substring=None):
        with tempfile.NamedTemporaryFile("w+", suffix=".json") as f:
            json.dump(prop_data, f)
            f.flush()
            f_path = Path(f.name)

            def mock_load(name):
                if name == "state.json":
                    return mock_state
                return original_load(name)

            workflow_preflight.load_json = mock_load
            try:
                try:
                    workflow_preflight.validate("ZN-E099", f_path)
                    if expected_err_substring is not None:
                        raise AssertionError(
                            f"Expected error containing {expected_err_substring!r}, but passed"
                        )
                except RuntimeError as exc:
                    if expected_err_substring is None:
                        raise AssertionError(f"Unexpected preflight failure: {exc}")
                    assert expected_err_substring in str(exc), (
                        f"Expected {expected_err_substring!r} in error, got: {exc}"
                    )
            finally:
                workflow_preflight.load_json = original_load

    # Valid proposal: no Phase 3 error
    check_proposal(base_p3_proposal, expected_err_substring=None)

    # Invalid type
    inv_type = dict(base_p3_proposal, type="architecture")
    check_proposal(inv_type, expected_err_substring="Phase 3 permits only compression runs")

    # Invalid main_loss
    inv_loss = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], main_loss="mse"))
    check_proposal(inv_loss, expected_err_substring="Phase 3 requires main_loss='relative_huber'")

    # Invalid delta
    inv_delta = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], relative_huber_delta=0.20))
    check_proposal(inv_delta, expected_err_substring="Phase 3 requires relative_huber_delta=0.15")

    # Invalid learning rate
    inv_lr = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], learning_rate=0.001))
    check_proposal(inv_lr, expected_err_substring="Phase 3 requires the locked learning_rate")

    # Invalid batch size
    inv_bs = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], batch_size=64))
    check_proposal(inv_bs, expected_err_substring="Phase 3 requires the locked batch_size")

    # Invalid physics_loss_weight
    inv_plw = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], physics_loss_weight=0.05))
    check_proposal(inv_plw, expected_err_substring="Phase 3 requires physics_loss_weight=0")

    # Invalid physics2_type
    inv_p2 = dict(base_p3_proposal, training=dict(base_p3_proposal["training"], physics2_type="timewarp_aug"))
    check_proposal(inv_p2, expected_err_substring="Phase 3 forbids physics2_type")

    print("Phase 3 preflight checks verified successfully.")


if __name__ == "__main__":
    print("Running test_default_stem_widths_identical...")
    test_default_stem_widths_identical()
    print("Running test_zne010_config_identical...")
    test_zne010_config_identical()
    print("Running test_narrow_stem_widths...")
    test_narrow_stem_widths()
    print("Running test_phase3_preflight_rules...")
    test_phase3_preflight_rules()
    print("ALL TESTS PASSED.")
