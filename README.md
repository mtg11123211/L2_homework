# sp-vision-homework

RoboMaster 上位机视觉 —— **课时作业 L1-L5**。

大作业（自动瞄准 / 能量机关 / 前哨站）在另一个仓库：`sp-vision-final-project`。

## 结构约定

代码本体是一个 CMake 工程，留在仓库根目录；**每一课的代码状态用 tag 固化**，交付物（笔记、报告、截图、数据）按课次放进 `L1/`…`L5/`。
这样既能按课次分开看，又不用把同一份工程复制五遍。

| 课次 | 代码 tag | 内容 | 交付物 |
|---|---|---|---|
| L1 | — | 环境与工具链：CMake 工程、相机 SDK、OpenCV | [L1/](L1/) |
| L2 | `L2` | 相机取流（Camera 类）、YOLO 装甲板检测、AprilTag | [L2/](L2/) · `L2_README/` |
| L3 | `L3` | 多线程采集处理流水线（阻塞队列 / 帧源 / 处理 / 统计 / 安全退出） | [L3/](L3/) · `lecture3_hw/` |
| L4 | — | 待补 | [L4/](L4/) |
| L5 | — | 待补 | [L5/](L5/) |

## 回到某一课的代码

```bash
git switch --detach L2     # L2 交作业时的代码
git switch main            # 回到最新
git tag -l                 # 看所有课次 tag
```

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

相机依赖海康 MVS SDK，库文件在 `io/hikrobot/lib/<平台>/`；`logs/` 与 `build/` 不入库。
