# v6_auto chạy trên Kaggle (qua Kaggle API — agent tự đẩy, không cần bấm)

**Người dùng (1 lần):**
1. kaggle.com (tài khoản thanhthuyytltt@gmail.com) → xác minh số điện thoại (cần cho GPU).
2. Settings → API → Create New Token → tải `kaggle.json`.
3. Chạy: `mkdir -p ~/.kaggle_v6 && mv ~/Downloads/kaggle.json ~/.kaggle_v6/ && chmod 600 ~/.kaggle_v6/kaggle.json`
4. Agent điền `kaggle_user` trong `remote/config.json` và chạy `python remote/remote.py init-cache`
   (tải 3 cache lên dataset private `v6-auto-cache`, 1 lần).

**Agent:** mỗi job = 1 kernel private GPU `v6-auto-<id>` (code nhúng trong script, cache từ dataset).
`submit <ID> <proposal> --split S` → `status <ID>` (queued/running/complete/error) → `fetch <ID>`.
Giới hạn: ~30 h GPU/tuần, ≤ 12 h mỗi kernel.
