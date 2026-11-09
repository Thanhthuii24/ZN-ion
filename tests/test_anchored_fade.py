import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import torch
import torch.nn as nn
import torch.nn.functional as F

from train import AnchoredFadeResConvMaskedSummary, ResidualConvMaskedSummary, build_model


def test_padding_invariance():
    torch.manual_seed(42)
    config = {
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
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_clean = model(curves, mask)

        curves_noisy = curves.clone()
        curves_noisy[:, 30:] += torch.randn_like(curves_noisy[:, 30:]) * 20.0
        out_noisy = model(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"Padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )


def test_cycle1_dependence():
    torch.manual_seed(101)
    config = {
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
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_base = model(curves, mask)

        # 1. Mutate cycle 0 (index 0 / cycle 1): must change output
        curves_mod_c0 = curves.clone()
        curves_mod_c0[:, 0] += torch.randn_like(curves_mod_c0[:, 0]) * 1.0
        out_c0 = model(curves_mod_c0, mask)
        diff_c0 = (out_c0 - out_base).abs().max().item()
        assert diff_c0 > 1e-4, f"Mutating cycle 0 did not change output (max diff = {diff_c0})"

        # 2. Mutate a padded cycle (index 50 >= 30): must not change output
        curves_mod_padded = curves.clone()
        curves_mod_padded[:, 50] += torch.randn_like(curves_mod_padded[:, 50]) * 15.0
        out_padded = model(curves_mod_padded, mask)
        assert torch.allclose(out_base, out_padded, atol=1e-5, rtol=1e-5), (
            f"Mutating padded cycle 50 changed output: max diff = {(out_base - out_padded).abs().max()}"
        )


def test_zne005_backward_compatibility():
    # ZN-E005 baseline config
    config_005 = {
        "variant": "residual_conv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }

    torch.manual_seed(42)
    model_005 = build_model(config_005).eval()

    torch.manual_seed(123)
    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out = model_005(curves, mask)

    expected = torch.tensor([0.5174931287765503, 0.49267321825027466])
    assert torch.allclose(out, expected, atol=1e-6, rtol=1e-6), (
        f"ZN-E005 output changed! Expected {expected.tolist()}, got {out.tolist()}"
    )


def test_finite_forward_and_param_count():
    torch.manual_seed(7)
    config = {
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
    model = build_model(config).eval()

    inputs = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        output = model(inputs, mask)

    assert output.shape == (2,), f"Expected shape (2,), got {output.shape}"
    assert torch.isfinite(output).all(), "Non-finite outputs detected"

    num_params = sum(param.numel() for param in model.parameters())
    print("params (anchor default):", num_params)
    assert num_params == 98113, f"Expected 98113 parameters, got {num_params}"
    expected = torch.tensor([0.048489369451999664, 0.03065742552280426])
    assert torch.allclose(output, expected, atol=1e-6, rtol=1e-6), (
        f"Default anchor output changed! Expected {expected.tolist()}, got {output.tolist()}"
    )

    # Explicit fade_input="anchor" produces identical output and params
    torch.manual_seed(7)
    model_explicit = build_model(dict(config, fade_input="anchor")).eval()
    with torch.no_grad():
        output_explicit = model_explicit(inputs, mask)
    assert torch.allclose(output, output_explicit, atol=1e-7, rtol=1e-7), (
        "Explicit fade_input='anchor' does not match default output"
    )
    assert sum(p.numel() for p in model_explicit.parameters()) == 98113


def test_anchor_rate_finite_forward_and_param_count():
    torch.manual_seed(7)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "anchor_rate",
    }
    model = build_model(config).eval()
    assert model.fade_input == "anchor_rate"
    assert model.fade_encoder[0].in_features == 3 * 64

    inputs = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        output = model(inputs, mask)

    assert output.shape == (2,), f"Expected shape (2,), got {output.shape}"
    assert torch.isfinite(output).all(), "Non-finite outputs detected for anchor_rate"

    num_params = sum(param.numel() for param in model.parameters())
    print("params (anchor_rate):", num_params)
    assert num_params == 102209, f"Expected 102209 parameters, got {num_params}"
    expected = torch.tensor([0.01636943221092224, -0.009812891483306885])
    assert torch.allclose(output, expected, atol=1e-6, rtol=1e-6), (
        f"anchor_rate output changed! Expected {expected.tolist()}, got {output.tolist()}"
    )


def test_anchor_rate_padding_invariance():
    torch.manual_seed(42)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "anchor_rate",
    }
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_clean = model(curves, mask)

        curves_noisy = curves.clone()
        curves_noisy[:, 30:] += torch.randn_like(curves_noisy[:, 30:]) * 20.0
        out_noisy = model(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"anchor_rate padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )


def test_anchor_rate_cycle1_dependence():
    torch.manual_seed(101)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "anchor_rate",
    }
    model = build_model(config).eval()

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_base = model(curves, mask)

        # 1. Mutate cycle 0 (index 0 / cycle 1): must change output
        curves_mod_c0 = curves.clone()
        curves_mod_c0[:, 0] += torch.randn_like(curves_mod_c0[:, 0]) * 1.0
        out_c0 = model(curves_mod_c0, mask)
        diff_c0 = (out_c0 - out_base).abs().max().item()
        assert diff_c0 > 1e-4, f"anchor_rate: Mutating cycle 0 did not change output (max diff = {diff_c0})"

        # 2. Mutate a padded cycle (index 50 >= 30): must not change output
        curves_mod_padded = curves.clone()
        curves_mod_padded[:, 50] += torch.randn_like(curves_mod_padded[:, 50]) * 15.0
        out_padded = model(curves_mod_padded, mask)
        assert torch.allclose(out_base, out_padded, atol=1e-5, rtol=1e-5), (
            f"anchor_rate: Mutating padded cycle 50 changed output: max diff = {(out_base - out_padded).abs().max()}"
        )


def test_relative_rate_init_identity_and_param_count():
    torch.manual_seed(7)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "relative_rate",
    }
    model = build_model(config).eval()
    assert model.fade_input == "relative_rate"
    assert model.fade_encoder[0].in_features == 2 * 64

    # Check zero-initialization of the LAST Linear layer in fade_encoder
    assert torch.all(model.fade_encoder[-1].weight == 0.0)
    assert torch.all(model.fade_encoder[-1].bias == 0.0)

    inputs = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        output = model(inputs, mask)

    assert output.shape == (2,), f"Expected shape (2,), got {output.shape}"
    assert torch.isfinite(output).all(), "Non-finite outputs detected for relative_rate"

    num_params = sum(param.numel() for param in model.parameters())
    print("params (relative_rate):", num_params)
    assert num_params == 98113, f"Expected 98113 parameters, got {num_params}"

    # Verify that at init, g == h exactly (encode_cycle_states == encode_curve_features)
    with torch.no_grad():
        h = model.encode_curve_features(inputs)
        g_unmasked = model.encode_cycle_states(inputs, mask=None)
        assert torch.allclose(g_unmasked, h, atol=1e-7, rtol=1e-7), (
            f"At init, g != h! max diff = {(g_unmasked - h).abs().max()}"
        )
        g_masked = model.encode_cycle_states(inputs, mask=mask)
        expected_masked = h * mask.unsqueeze(-1)
        assert torch.allclose(g_masked, expected_masked, atol=1e-7, rtol=1e-7), (
            f"At init with mask, g != h * mask! max diff = {(g_masked - expected_masked).abs().max()}"
        )


def test_relative_rate_cycle0_passthrough():
    torch.manual_seed(42)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "relative_rate",
    }
    model = build_model(config).eval()

    # Re-randomize fade_encoder weights to non-zero values so fade_encoder is active
    with torch.no_grad():
        model.fade_encoder[-1].weight.normal_(0.0, 1.0)
        model.fade_encoder[-1].bias.normal_(0.0, 1.0)

    curves = torch.randn(2, 100, 3, 300)

    with torch.no_grad():
        h = model.encode_curve_features(curves)
        g = model.encode_cycle_states(curves, mask=None)

        # Cycle 0 (t = 0 / 1st cycle) must be passed through unchanged: g[:, 0] == h[:, 0]
        assert torch.allclose(g[:, 0, :], h[:, 0, :], atol=1e-7, rtol=1e-7), (
            f"Cycle-0 passthrough failed: max diff = {(g[:, 0, :] - h[:, 0, :]).abs().max()}"
        )

        # For t > 0, g must differ from h because fade_encoder is active
        diff_t_gt_0 = (g[:, 1:, :] - h[:, 1:, :]).abs().max().item()
        assert diff_t_gt_0 > 1e-3, (
            f"Fade encoder should affect t > 0, but max diff was only {diff_t_gt_0}"
        )


def test_relative_rate_padding_invariance():
    torch.manual_seed(42)
    config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "activation": "gelu",
        "fade_hidden": 64,
        "fade_input": "relative_rate",
    }
    model = build_model(config).eval()

    # Re-randomize fade_encoder weights so fade_encoder is active
    with torch.no_grad():
        model.fade_encoder[-1].weight.normal_(0.0, 1.0)
        model.fade_encoder[-1].bias.normal_(0.0, 1.0)

    curves = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1.0

    with torch.no_grad():
        out_clean = model(curves, mask)

        curves_noisy = curves.clone()
        curves_noisy[:, 30:] += torch.randn_like(curves_noisy[:, 30:]) * 20.0
        out_noisy = model(curves_noisy, mask)

    assert torch.allclose(out_clean, out_noisy, atol=1e-5, rtol=1e-5), (
        f"relative_rate padding invariance failed: max diff = {(out_clean - out_noisy).abs().max()}"
    )


def test_fade_input_options_and_validation():
    base_config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "fade_hidden": 64,
    }

    # "anchor", "anchor_rate", and "relative_rate" allowed
    m_anchor = build_model(dict(base_config, fade_input="anchor"))
    assert m_anchor.fade_input == "anchor"
    m_rate = build_model(dict(base_config, fade_input="anchor_rate"))
    assert m_rate.fade_input == "anchor_rate"
    m_rel = build_model(dict(base_config, fade_input="relative_rate"))
    assert m_rel.fade_input == "relative_rate"

    # Invalid fade_input rejection
    try:
        build_model(dict(base_config, fade_input="invalid_option"))
        assert False, "Should have raised ValueError for invalid fade_input"
    except ValueError:
        pass


def test_diagnostic_modules():
    config = {
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
    model = build_model(config)
    diag = model.diagnostic_modules()
    assert set(diag.keys()) == {
        "cycle_stem",
        "residual_blocks",
        "fade_encoder",
        "inter_summary",
        "head",
    }, f"Unexpected diagnostic keys with mlp_layers=0: {diag.keys()}"

    config_with_mlp = dict(config, mlp_layers=2)
    model_mlp = build_model(config_with_mlp)
    diag_mlp = model_mlp.diagnostic_modules()
    assert set(diag_mlp.keys()) == {
        "cycle_stem",
        "residual_blocks",
        "fade_encoder",
        "inter_summary",
        "inter_mlp",
        "head",
    }, f"Unexpected diagnostic keys with mlp_layers=2: {diag_mlp.keys()}"


def test_activation_options_and_validation():
    base_config = {
        "variant": "anchored_fade_resconv_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "d_ff": 64,
        "mlp_layers": 0,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
        "fade_hidden": 64,
    }

    # GELU model (default)
    model_gelu = build_model(dict(base_config, activation="gelu")).eval()
    assert model_gelu.activation == "gelu"
    for idx in (1, 3, 5):
        assert isinstance(model_gelu.cycle_stem[idx], nn.GELU)
    assert isinstance(model_gelu.fade_encoder[1], nn.GELU)
    sample_feat = torch.randn(2, 64)
    assert torch.allclose(model_gelu.readout_activation(sample_feat), F.gelu(sample_feat))

    # ReLU model
    model_relu = build_model(dict(base_config, activation="relu")).eval()
    assert model_relu.activation == "relu"
    for idx in (1, 3, 5):
        assert isinstance(model_relu.cycle_stem[idx], nn.ReLU)
    assert isinstance(model_relu.fade_encoder[1], nn.ReLU)
    assert torch.allclose(model_relu.readout_activation(sample_feat), F.relu(sample_feat))

    # Both models have identical parameter count
    params_gelu = sum(p.numel() for p in model_gelu.parameters())
    params_relu = sum(p.numel() for p in model_relu.parameters())
    assert params_gelu == params_relu == 98113

    # Invalid activation rejection
    try:
        build_model(dict(base_config, activation="unsupported_act"))
        assert False, "Should have raised ValueError for invalid activation"
    except ValueError:
        pass


if __name__ == "__main__":
    test_padding_invariance()
    test_cycle1_dependence()
    test_zne005_backward_compatibility()
    test_finite_forward_and_param_count()
    test_anchor_rate_finite_forward_and_param_count()
    test_anchor_rate_padding_invariance()
    test_anchor_rate_cycle1_dependence()
    test_relative_rate_init_identity_and_param_count()
    test_relative_rate_cycle0_passthrough()
    test_relative_rate_padding_invariance()
    test_fade_input_options_and_validation()
    test_diagnostic_modules()
    test_activation_options_and_validation()
    print("All tests passed successfully!")
