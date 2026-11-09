import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import torch
import torch.nn as nn
import torch.nn.functional as F

from train import PhaseSplitResConvMaskedSummary, build_model


def test_padding_invariance():
    torch.manual_seed(42)
    config = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_clean = model(curves, mask)

        curves_noisy = curves.clone()
        curves_noisy[:, 30:] += torch.randn_like(curves_noisy[:, 30:]) * 10.0
        out_noisy = model(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"Padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )


def test_phase_isolation():
    torch.manual_seed(101)
    config = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)

    with torch.no_grad():
        features_clean = model.encode_curve_features(curves)
        charge_clean = features_clean[..., : model.d_model // 2]
        discharge_clean = features_clean[..., model.d_model // 2 :]

        # Direct phase encoder helper matches sliced features
        assert torch.allclose(charge_clean, model.encode_charge(curves), atol=1e-6)
        assert torch.allclose(discharge_clean, model.encode_discharge(curves), atol=1e-6)

        # Mutate discharge of cycle 5
        curves_mod_cycle5 = curves.clone()
        curves_mod_cycle5[:, 5, :, 150:] = torch.randn(2, 3, 150)
        charge_mod_cycle5 = model.encode_charge(curves_mod_cycle5)
        assert torch.allclose(charge_clean, charge_mod_cycle5, atol=1e-6), (
            "Mutating discharge of cycle 5 changed charge-branch features"
        )

        # Mutate all discharge points
        curves_mod_discharge = curves.clone()
        curves_mod_discharge[:, :, :, 150:] = torch.randn_like(curves_mod_discharge[:, :, :, 150:])
        features_mod_discharge = model.encode_curve_features(curves_mod_discharge)
        charge_mod_discharge = features_mod_discharge[..., : model.d_model // 2]
        assert torch.allclose(charge_clean, charge_mod_discharge, atol=1e-6), (
            "Mutating all discharge points changed charge-branch features"
        )

        # Mutate all charge points -> discharge-branch features must be unaffected
        curves_mod_charge = curves.clone()
        curves_mod_charge[:, :, :, :150] = torch.randn_like(curves_mod_charge[:, :, :, :150])
        features_mod_charge = model.encode_curve_features(curves_mod_charge)
        discharge_mod_charge = features_mod_charge[..., model.d_model // 2 :]
        assert torch.allclose(discharge_clean, discharge_mod_charge, atol=1e-6), (
            "Mutating charge points changed discharge-branch features"
        )


def test_finite_forward_and_param_count():
    torch.manual_seed(7)
    config = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }
    model = build_model(config).eval()

    inputs = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        output = model(inputs, mask)

    assert output.shape == (2,), f"Expected shape (2,), got {output.shape}"
    assert torch.isfinite(output).all(), "Non-finite outputs detected"

    num_params = sum(param.numel() for param in model.parameters())
    print("params:", num_params)
    assert num_params == 49857, f"Expected 49857 parameters, got {num_params}"


def test_diagnostic_modules():
    config = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }
    model = build_model(config)
    diag = model.diagnostic_modules()
    assert set(diag.keys()) == {
        "charge_stem",
        "discharge_stem",
        "charge_residual",
        "discharge_residual",
        "inter_summary",
        "head",
    }, f"Unexpected diagnostic keys with mlp_layers=0: {diag.keys()}"

    config_with_mlp = dict(config, mlp_layers=2)
    model_mlp = build_model(config_with_mlp)
    diag_mlp = model_mlp.diagnostic_modules()
    assert set(diag_mlp.keys()) == {
        "charge_stem",
        "discharge_stem",
        "charge_residual",
        "discharge_residual",
        "inter_summary",
        "inter_mlp",
        "head",
    }, f"Unexpected diagnostic keys with mlp_layers=2: {diag_mlp.keys()}"


def test_activation_relu_backward_compatibility():
    base_config = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }

    torch.manual_seed(42)
    model_default = build_model(base_config).eval()

    torch.manual_seed(42)
    model_explicit_relu = build_model(dict(base_config, activation="relu")).eval()

    assert model_default.activation == "relu"
    assert model_explicit_relu.activation == "relu"

    # Stem activations must be nn.ReLU
    for stem in (model_explicit_relu.charge_stem, model_explicit_relu.discharge_stem):
        assert isinstance(stem[1], nn.ReLU)
        assert isinstance(stem[3], nn.ReLU)
        assert isinstance(stem[5], nn.ReLU)

    # State dicts must match exactly
    sd_default = model_default.state_dict()
    sd_relu = model_explicit_relu.state_dict()
    assert sd_default.keys() == sd_relu.keys()
    for key in sd_default:
        assert torch.equal(sd_default[key], sd_relu[key]), f"Mismatch in state_dict key: {key}"

    # Outputs must match exactly
    torch.manual_seed(999)
    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :45] = 1.0

    with torch.no_grad():
        out_default = model_default(curves, mask)
        out_relu = model_explicit_relu(curves, mask)

    assert torch.equal(out_default, out_relu), (
        f"Output mismatch: max diff = {(out_default - out_relu).abs().max()}"
    )


def test_activation_gelu_finite_and_padding_invariant():
    config_gelu = {
        "variant": "phase_split_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
    }

    model_gelu = build_model(config_gelu).eval()
    assert model_gelu.activation == "gelu"

    # Stem activations must be nn.GELU
    for stem in (model_gelu.charge_stem, model_gelu.discharge_stem):
        assert isinstance(stem[1], nn.GELU)
        assert isinstance(stem[3], nn.GELU)
        assert isinstance(stem[5], nn.GELU)

    # Readout activation must be GELU
    sample_feat = torch.randn(2, 64)
    assert torch.allclose(model_gelu.readout_activation(sample_feat), F.gelu(sample_feat))

    # Parameter count must match ReLU model exactly (49857)
    num_params = sum(param.numel() for param in model_gelu.parameters())
    assert num_params == 49857, f"Expected 49857 parameters, got {num_params}"

    # Finite forward
    torch.manual_seed(2026)
    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :35] = 1.0

    with torch.no_grad():
        out_clean = model_gelu(curves, mask)

    assert out_clean.shape == (2,), f"Expected shape (2,), got {out_clean.shape}"
    assert torch.isfinite(out_clean).all(), "Non-finite outputs detected with GELU"

    # Padding invariance
    curves_noisy = curves.clone()
    curves_noisy[:, 35:] += torch.randn_like(curves_noisy[:, 35:]) * 15.0

    with torch.no_grad():
        out_noisy = model_gelu(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"GELU padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )

    # Invalid activation rejection
    try:
        build_model(dict(config_gelu, activation="unsupported_act"))
        assert False, "Should have raised ValueError for invalid activation"
    except ValueError:
        pass


if __name__ == "__main__":
    test_padding_invariance()
    test_phase_isolation()
    test_diagnostic_modules()
    test_finite_forward_and_param_count()
    test_activation_relu_backward_compatibility()
    test_activation_gelu_finite_and_padding_invariant()
