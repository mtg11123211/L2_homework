#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_dir"

echo "=== Homework Check ==="
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j

ctest --test-dir build --output-on-failure

for workers in 2 3 6; do
  output_dir="test-output/workers-${workers}"
  rm -rf -- "$output_dir"
  ./build/mini_vision \
    --workers="$workers" \
    --producer-delay=0 \
    --worker-delay=4 \
    --output="$output_dir" >/dev/null
  count="$(find "$output_dir" -maxdepth 1 -type f -name '*.jpg' | wc -l)"
  if [[ "$count" -ne 20 ]]; then
    echo "FAIL: ${workers} workers saved ${count}/20 frames"
    exit 1
  fi
  echo "${workers} workers ............. PASS"
done

report_file="report.md"
if [[ ! -f "$report_file" ]]; then
  echo "FAIL: report.md is missing"
  exit 1
fi

required_report_sections=(
  "## 1. 图像生命周期与所有权"
  "## 2. 并发处理与恰好一次"
  "## 3. 共享统计数据"
  "## 4. 线程关闭协议"
)
for section in "${required_report_sections[@]}"; do
  if ! grep -Fqx "$section" "$report_file"; then
    echo "FAIL: report.md is missing section: $section"
    exit 1
  fi
done

if grep -q '^TODO(report)' "$report_file"; then
  echo "FAIL: report.md still contains TODO(report) markers"
  exit 1
fi
echo "report.md .................. PASS"

echo "ALL TESTS PASSED"
