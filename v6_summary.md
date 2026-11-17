# v6_auto (Zn-ion, BatteryLife ZN-coin) — tổng kết (2026-11-17)

Dữ liệu: BatteryLife ZN-coin, 3 split của paper (2021 / 42 / 2024), 20 cell validation mỗi split; test split chưa bao giờ được đọc.
Mọi số là trung bình ± độ lệch chuẩn trên 3 split (val). Loss chính: Relative Huber δ = 0.15; Adam lr 5e-4, batch 128, seed 2021.

## 1. Kết quả chính

| Model | Tham số | MAPE | Acc@15 | RMSE |
|---|---|---|---|---|
| Paper CPMLP (Zn-ion) | — | 0.558 ± 0.034 | 0.297 ± 0.084 | — |
| Paper CPTransformer (Zn-ion, MAPE tốt nhất) | — | 0.515 ± 0.067 | 0.202 ± 0.084 | — |
| **ZN-E010 AnchoredFade-ResConv-GELU** (Phase 1, đóng băng) | 98,113 | **0.332 ± 0.016** | **0.324 ± 0.067** | 392 ± 38 |
| ZN-E018/19/20 = ZN-E010 + P0c physics (Phase 2) | 98,113 | 0.339 ± 0.012 | 0.296 ± 0.067 | 395 ± 45 |
| **S2 nén** ZN-E023/24/25 (Phase 3) | **28,921** | **0.3323 ± 0.0055** | 0.311 ± 0.088 | 354 ± 93 |

## 2. Phase 1 — kiến trúc (ZN-E000…E014)
- CPMLP cấu hình paper sụp về một giá trị (MAPE 0.435, Acc@15 0.10); không dùng CPMLP làm ứng viên (yêu cầu của người dùng).
- 4 họ model: ResConv + masked summary, + TCN, phase-split encoder, anchored fade encoder. GELU thay ReLU sửa lỗi "dead ReLU" ở kiểm tra gradient.
- Mô hình chọn: **ZN-E010** — ResConv curve encoder (GELU) → fade encoder phi tuyến trên [h_t, h_t − h_1] → masked mean/last/last−first → head tuyến tính.
- Đóng sớm theo A5: 9 kiến trúc có sai số theo cell tương quan 0.905; cell > 1000 chu kỳ luôn bị đoán thấp ~1000 chu kỳ (giới hạn của loss Relative Huber, gradient ∝ 1/y, và của 100 chu kỳ quan sát).

## 3. Phase 2 — loss (ZN-E015…E021)
| Loss | Kết quả trên split 2021 | Kết luận |
|---|---|---|
| stat_tail_balance (thống kê) | MAPE 0.3279, Acc@15 0.300 | vẫn mang hệ số 1/y, chỉ kéo thêm ~1.19× |
| P0 uniform fade (physics) | MAPE 0.3433; vi phạm 0.150 → 0.044 (−70%) | nhất quán vật lý, độ chính xác ~không đổi |
| P4b falling-trend (physics) | MAPE 0.4215, R = 2.07, cos ≈ −0.85 | xung đột cấu trúc (break-in) |
| P0c late knee (physics) | MAPE 0.3254; vi phạm −63% | 3 split: 0.339 ± 0.012; chưa CONFIRMED (2024 chỉ −44%) |
| P0u capacity improvement (physics mới) | MAPE 0.3454; vi phạm −15%, cos < 0 | không kéo được cell sống lâu |
- Kết luận: physics loss làm model nhất quán vật lý gần như không mất độ chính xác, nhưng không tăng độ chính xác; lý do: trong 100 chu kỳ đầu, cell break-in sống ngắn trông như cell sống lâu (vd. 441-1: EOL 219, SOH_100 1.22; 437-2: EOL 1281, SOH_100 1.045).
- Kiểm tra vật lý trên ZN-coin: Spearman(độ suy giảm dung lượng, log EOL) = −0.957; Spearman(SOH cuối, log EOL) = 0.921; sụt áp không tương quan (+0.016).

## 4. Phase 3 — TinyML trên NUCLEO-H7S3L8 (STM32H7S3, Cortex-M7 600 MHz, 64 KB flash trong, 620 KB SRAM, 32 MB XSPI2)
Streaming: E (mã hoá 1 chu kỳ) → F (fade cell) → C (cộng dồn summary) → H (head). ONNX khớp PyTorch ≤ 8e-6; ST Edge AI 4.1.

| Model trên board | MAPE board (3 split) | Latency / chu kỳ mới | RAM |
|---|---|---|---|
| ZN-E010 (E fp32 + F INT8 + H fp32) | 0.3318 ± 0.0143 | 52.5 ms | 399 KB |
| P0c (fp32) | 0.3390 ± 0.0118 | 57.0 ms | 397 KB |
| **S2 nén (fp32)** | **0.3323 ± 0.0054** | **20.7 ms** | **132 KB** |

- MAPE trên board = máy tính (chênh EOL tối đa 0.0003–4 chu kỳ).
- INT8 toàn phần của ZN-E010: Δ MAPE +0.0086 / +0.0148 / +0.0011 → loại; encoder E nhạy nhất. S2 INT8: +0.0039 / +0.0073 / −0.0029 → loại theo ngưỡng 0.001.
- Trọng số fp32 > 32 KB D-cache: chạy thẳng từ XSPI2 thì E mất 1379 ms; chép trọng số vào SRAM lúc khởi động → 49 ms (ZN-E010) / 17 ms (S2).
- Không chạy được chỉ từ 64 KB flash trong: runtime ST + kernel INT8 đã 46 KB code; S2 INT8 tổng 85 KB.
- Chưa đo năng lượng (không có ampe kế / STLINK-V3PWR).

## 5. File chính
results.tsv · research_log.md · state.json · phase3/results_tinyml.tsv · diagnostics/phase1_breakthrough_diagnosis.md ·
phase3/p0_export.py · phase3/board/st_int8.py · phase3/board/fw/ (zn_app.c, zn_cpart.c, run_split.sh) · loss.md §4.6 (P0u).
