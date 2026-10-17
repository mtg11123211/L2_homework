# L3 课时作业

**代码 tag**：`L3`（提交 `0f4ec60 lecture3 homework`）

**内容**：多线程采集-处理流水线，独立工程在 `lecture3_hw/`：

- `include/blocking_queue.hpp` —— 有界阻塞队列
- `include/frame_source.hpp` / `src/frame_source.cpp` —— 帧源线程
- `include/image_processor.hpp` / `src/image_processor.cpp` —— 处理线程
- `include/pipeline.hpp` / `src/pipeline.cpp` —— 流水线装配与启停
- `include/statistics.hpp` / `src/statistics.cpp` —— 帧率与丢帧统计
- `tests/` —— 帧拷贝、流水线、安全退出、统计四个单测
- `scripts/check.sh`、`scripts/generate_assets.py` —— 自检与素材生成

**交付物**：
- `lecture3_hw/README.md` —— 工程说明
- `lecture3_hw/report.md` —— 本课报告

**要点**：帧深拷贝、统计加锁、析构里安全停线程（这三条是这一课最容易踩的坑）。
