import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import torch

from train import MaskedTemporalConvBlock, build_model


def test_masked_group_ignores_padding_and_uses_observed_length():
    torch.manual_seed(7)
    block = MaskedTemporalConvBlock(8, 2, "masked_group").eval()
    observed = torch.randn(2, 8, 30)
    short_mask = torch.ones(2, 1, 30)
    short_output = block(observed, short_mask)

    zero_padded = torch.zeros(2, 8, 100)
    zero_padded[:, :, :30] = observed
    long_mask = torch.zeros(2, 1, 100)
    long_mask[:, :, :30] = 1
    noisy_padded = zero_padded.clone()
    noisy_padded[:, :, 30:] = torch.randn_like(noisy_padded[:, :, 30:])

    assert torch.allclose(short_output, block(zero_padded, long_mask)[:, :, :30], atol=1e-5, rtol=1e-5)
    assert torch.allclose(short_output, block(noisy_padded, long_mask)[:, :, :30], atol=1e-5, rtol=1e-5)


def test_group_mode_preserves_groupnorm_output():
    torch.manual_seed(19)
    block = MaskedTemporalConvBlock(8, 2, "group").eval()
    values = torch.randn(2, 8, 30)
    mask = torch.ones(2, 1, 30)
    transformed = block.pointwise(block.depthwise(torch.nn.functional.pad(values, (block.left_padding, 0))))
    expected = torch.relu(values + block.norm(transformed)) * mask
    assert torch.equal(block(values, mask), expected)


def test_zn_e003_smoke():
    config = {
        "variant": "residual_conv_temporal_tcn_masked_summary",
        "d_model": 64,
        "residual_layers": 2,
        "temporal_dilations": [1, 2, 4, 8, 16, 32],
        "temporal_norm": "masked_group",
        "d_ff": 64,
        "mlp_layers": 9,
        "summary_mode": "mean_last_delta",
        "dropout": 0.1,
    }
    model = build_model(config).eval()
    inputs = torch.randn(2, 100, 3, 300)
    mask = torch.zeros(2, 100)
    mask[:, :30] = 1
    with torch.no_grad():
        output = model(inputs, mask)
    assert output.shape == (2,)
    assert torch.isfinite(output).all()
    print("params", sum(parameter.numel() for parameter in model.parameters()))


if __name__ == "__main__":
    test_masked_group_ignores_padding_and_uses_observed_length()
    test_group_mode_preserves_groupnorm_output()
    test_zn_e003_smoke()
