<div align="center">
<img src="docs/assets/hero.svg" alt="SEBOT T710 Autonomous Service Robot" width="100%" />

# SEBOT T710 · Autonomous Service Robot

**An integrated perception, navigation and manipulation stack for robotics competitions and catering scenarios.**

[中文文档](README.md) · [Implementation status](docs/IMPLEMENTATION_STATUS.md) · [Reproduction notes](docs/REPRODUCIBILITY.md)
</div>

## Overview

This repository brings together edge vision, laser mapping, mobile navigation, robotic grasping and task orchestration for the SEBOT T710 platform. A catering state machine connects ordering, object pickup, table delivery and payment through ROS nodes and hardware interfaces.

| Layer | Included implementation |
| :-- | :-- |
| Perception | PaddleDetection/YOLO deployment, PPNC NPU inference, ONNX Runtime post-processing and OpenCV |
| Navigation | GMapping/Hector SLAM, AMCL, EKF, costmaps, GlobalPlanner and DWA |
| Manipulation | Visual feedback, stage-based grasp control, PID correction and Talon serial commands |
| Integration | C++, ROS1/catkin, Qt/RViz, STDR simulation and URDF/MoveIt configuration |

**Version scope:** this snapshot uses ROS1/catkin; historical build artifacts reference ROS Melodic. Navigation selects Dijkstra and DWA; an A* implementation is included. ROS2, improved A*, TEB, Docker, training/pruning and reinforcement-learning policies are described by the maintainer as broader project work but are not included in this snapshot.

## Technical highlights

- **Edge inference pipeline:** model configuration, NPU execution, ONNX post-processing and structured detection results are encapsulated in [`Detection`](sebot_catering/src/sebot_catering/include/detection.hpp).
- **Navigation-to-task interface:** [`Cater`](sebot_catering/src/sebot_catering/src/catering.cpp) sends `move_base` action goals and coordinates task transitions using localization and navigation state.
- **Feedback-driven manipulation:** [`Picking`](sebot_catering/src/sebot_catering/src/picking.cpp) combines target detection, laser distance observations, PID correction and staged arm actions.
- **Modular bring-up:** separate detection, ordering, picking, payment and arm demonstration nodes support targeted hardware debugging.

## Getting started

Use a Linux/ROS1 environment matching the T710 board image. The historical distribution is ROS Melodic. The catering package additionally requires vendor PPNC and ONNX packages, OpenCV, LibSerial and the corresponding hardware. Model binaries are platform-specific.

```bash
git clone https://github.com/wild-pointer-04/sebot-t710-competition.git
cd sebot-t710-competition
export SEBOT_ROOT="$PWD"
source /opt/ros/melodic/setup.bash
cd "$SEBOT_ROOT/sebot_ros_kits"
rosdep install --from-paths src --ignore-src -r -y
catkin_make
source devel/setup.bash
cd "$SEBOT_ROOT/sebot_catering"
rosdep install --from-paths src --ignore-src -r -y
catkin_make
source devel/setup.bash
roslaunch sebot_catering sebot_yolo.launch
```

These commands were derived from the source tree and have not been validated on robot hardware during publication. Vendor SDKs are not supplied by `rosdep`. See the [Chinese README](README.md) for architecture, module links and other launch targets.

## Reported project results

The maintainer reports YOLO training and structured pruning, containerized deployment, ROS2 navigation with improved A* and TEB, and a policy-optimization-based grasping system with **96% success in complex scenes**. Training code, the extended navigation implementation, policy checkpoints and evaluation logs are not present here. The 96% figure is a maintainer-reported result, not a reproducible benchmark of this release.

## Publication notes

Build products and local recordings are excluded. Runtime model assets are retained. Optional Baidu speech credentials are read from `BAIDU_API_KEY` and `BAIDU_SECRET_KEY`; see [`.env.example`](.env.example).

Original authorship and component licenses are preserved. The repository includes upstream robotics components and does not assign a new blanket license to their contents. See [provenance](docs/PROVENANCE.md).
