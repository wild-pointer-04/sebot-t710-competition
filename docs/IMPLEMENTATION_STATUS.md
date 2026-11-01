# 实现范围与成果说明

本页区分当前公开代码与维护者提供的项目经历，便于读者准确理解复现范围。

## 当前版本的代码证据

| 能力 | 证据 | 结论 |
| :-- | :-- | :-- |
| ROS 通信与构建 | [`package.xml`](../sebot_catering/src/sebot_catering/package.xml)、[`CMakeLists.txt`](../sebot_catering/src/sebot_catering/CMakeLists.txt) 使用 `catkin`、`roscpp`、`actionlib` | ROS1 工程；非 ROS2/ament 工程 |
| YOLO 板端推理 | [`detection.hpp`](../sebot_catering/src/sebot_catering/include/detection.hpp)、[`yolo.cpp`](../sebot_catering/src/sebot_catering/unit/yolo.cpp) | PPNC/NPU 推理、ONNX 后处理和 OpenCV 集成已收录 |
| SLAM | [`sebot_slam`](../sebot_ros_kits/src/sebot_slam) | 含 GMapping 和 Hector 实现与启动配置 |
| 全局规划 | [`astar.cpp`](../sebot_ros_kits/src/sebot_navigation/global_planner/src/astar.cpp)、[`planner_global_params.yaml`](../sebot_ros_kits/src/sebot_navigation/sebot_navigation/param/planner_global_params.yaml) | 含 A*；默认 `use_dijkstra: true`。未验证相对于上游的算法改进 |
| 局部规划 | [`move_base_params.yaml`](../sebot_ros_kits/src/sebot_navigation/sebot_navigation/param/move_base_params.yaml) | 默认 `dwa_local_planner/DWAPlannerROS`；TEB 仅出现在可选插件注释中 |
| 抓取控制 | [`picking.cpp`](../sebot_catering/src/sebot_catering/src/picking.cpp)、[`arm.hpp`](../sebot_catering/src/sebot_catering/include/arm.hpp) | 状态机、视觉/距离反馈、PID 与机械臂动作接口 |
| 任务调度 | [`catering.cpp`](../sebot_catering/src/sebot_catering/src/catering.cpp) | 导航、点餐、取放、结算的场景编排 |
| 调试与仿真 | [`unit/`](../sebot_catering/src/sebot_catering/unit)、[`sebot_ros_stdr`](../sebot_ros_stdr) | 硬件功能演示与 STDR 仿真资源；并非已通过的整机测试报告 |

## 维护者提供的扩展成果

维护者描述的完整技术栈为 **C++ · ROS2 · A* · TEB · SLAM · YOLO · OpenCV · Docker**，并提供了以下研发经历：

1. 基于 PaddlePaddle 完成 YOLO 训练、结构化剪枝与推理加速，以 Docker 支持容器化部署。
2. 基于 SLAM、改进 A* 与 TEB 完成 ROS2 下的定位、导航、动态轨迹优化和避障控制。
3. 基于策略优化的强化学习设计机械臂抓取策略，完成动作决策、轨迹规划和闭环控制，复杂场景成功率 96%。

当前发布快照未包含这些扩展对应的训练脚本、ROS2 包、TEB 配置、Dockerfile、强化学习策略代码或评测日志。上述内容按维护者的项目说明记录，不据此推断缺失文件已经在仓库内实现。

## 项目指标

基于策略优化的强化学习抓取策略，实现机械臂动作决策、轨迹规划与闭环控制，复杂场景抓取成功率达到 **96%**。

板端视觉模块通过 `yolo.cpp` 输出帧耗时与 FPS，支持在目标设备上观察推理运行表现。

## 本次发布验证

- 已静态检查主要入口、构建配置、导航参数、模型资源及文档内部链接。
- 已移除发布副本中的硬编码语音凭据，并改为读取环境变量。
- 已排除历史构建目录、编辑器缓存和本地录音。
- 未在本机执行 ROS 编译、模型推理、仿真回归或实际机械臂动作；这些验证需要匹配的 Linux/ROS 与硬件环境。
