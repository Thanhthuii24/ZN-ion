# Phase 3 plan — v6_auto (Zn-ion), 2026-11-13

Board: **NUCLEO-H7S3L8** (MB1737, UM3276): STM32H7S3L8, Cortex-M7 600 MHz, 64 KB internal flash, 620 KB SRAM,
32 MB Octo-SPI NOR (XSPI2, 0x70000000), ST-LINK V3 VCP /dev/cu.usbmodem11302. Guide: skill `stm32-deploy`.
Rule: the lead lists each board step and waits for the user's go; only val data on the board (never test).

Models (frozen): accuracy model **ZN-E010** (AnchoredFade-ResConv-GELU, 98,113 params; 3-split MAPE 0.332 ± 0.016,
Acc@15 0.324 ± 0.067); physics-informed companion **ZN-E018** config (same architecture + P0c).

Footprint: weights 383 KB fp32 / 96 KB int8 (> 64 KB internal flash -> XSPI2 external flash or compression);
full-window activations 480,000 values (~1.9 MB) in the cycle stem (> 620 KB SRAM -> per-cycle streaming).

| Step | Where | What | Pass check |
|---|---|---|---|
| P0 | host | deploy wrapper (no `If`), static batch-1 ONNX; streaming split: per-cycle encoder (ResConv) in ONNX, fade encoder + masked summary + head in a second ONNX or C | onnxruntime == PyTorch, max rel. EOL diff <= 1e-4 on all val prefixes |
| P1 | host | `stedgeai analyze` / `validate` (fp32, `-c low`) on both ONNX parts | ROM/RAM/MACC; no unsupported ops; val MAPE unchanged |
| P2 | host | static INT8 PTQ (train-calibrated QDQ) if needed for size | val Δ MAPE <= +0.001 on every split |
| P3 | board | validation firmware (STM32Cube AI Studio, external-flash layout), `stedgeai validate --mode target` | latency, cycles/MACC, RAM, output error vs host |
| P4 | board | application loop: per new cycle encode -> state cache -> summary -> head; DWT timing over VCP | latency per cycle and per prediction, flash/RAM from the map file |
| P5 | board (optional) | energy on the IDD jumper (number from UM3276) | µJ per inference |

Results: `phase3/results_tinyml.tsv` (one row per step) and research_log.md.
