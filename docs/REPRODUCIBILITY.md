# 复现与部署说明

## 环境与工作区

工程由多个 catkin 工作区及独立 Qt 启动器组成。原始构建缓存指向 ROS Melodic；缓存路径仅用于识别历史环境，不随发布提交。建议在匹配的机器人板卡镜像中重新构建。

| 工作区 | 主要依赖 | 说明 |
| :-- | :-- | :-- |
| `sebot_ros_kits` | ROS1、OpenCV、导航/建图依赖、设备 SDK | 基础节点与算法包 |
| `sebot_catering` | 基础工作区、LibSerial、`ppnc`、`onnx`、OpenCV | 板端推理及餐饮任务 |
| `sebot_location` | 基础工作区、Qt5 Widgets、RViz、cv_bridge | 定位与场景界面 |
| `sebot_ros_stdr` | ROS1、STDR 相关依赖、Qt | 二维导航仿真 |
| `sebot_startup/sebot` | Qt / qmake | 独立启动器；脚本含旧绝对路径，需要按本机目录调整 |

仓库未锁定全部第三方依赖版本，也未提供独立 Docker 构建环境。`rosdep` 不能替代厂商 PPNC/ONNX 包及硬件驱动配置。若缺少 `ppnc.pc`、`onnx.pc`，应先恢复板卡 SDK 的 pkg-config 环境。

## 设备与资源核对

- 检测演示使用 `/dev/rgbCamera`；抓取代码还引用 `/dev/deepCamera` 和 `/dev/video1`，需与目标板卡设备映射一致。
- Talon 接口使用 `/dev/talon`；雷达和底盘串口以各自配置为准。
- 导航使用 `sebot_navigation/map/map.yaml`；餐饮点位来自 `sebot_catering/res/location.xml`，需与现场地图保持一致。
- 模型目录中的 `.so`、`.o`、`.tar`、`.ro`、`.params` 和 ONNX 文件属于既有板端资源，保留在发布版本中；它们不是可跨架构运行的保证。
- 整机 launch 可能重复启动硬件节点，联调时只选择一条匹配场景的入口。

## STDR 仿真入口

下面命令假定已按首页构建基础工作区，并在同一终端中执行。此路径尚未在本次发布环境中运行验证。

```bash
source /opt/ros/melodic/setup.bash
source "$SEBOT_ROOT/sebot_ros_kits/devel/setup.bash"
cd "$SEBOT_ROOT/sebot_ros_stdr"
rosdep install --from-paths src --ignore-src -r -y
catkin_make
source devel/setup.bash
roslaunch stdr_navigation home_map_nav.launch
```

该入口加载 STDR、地图、AMCL 和 move_base，适合检查导航链路。它不会自动验证 T710 NPU 模型或真实机械臂抓取。

## 可选语音功能

云端语音使用环境变量；`.env.example` 仅说明变量名，程序不会自动加载 `.env`。

```bash
export BAIDU_API_KEY="your-own-api-key"
export BAIDU_SECRET_KEY="your-own-secret-key"
```

语音脚本还依赖 PyAudio、pygame、keyboard、func_timeout 等 Python 包，且包含历史资源路径。请在启用前检查设备、依赖与资源位置。不要把真实密钥写回源码或提交到仓库。

## 建议的硬件验证顺序

1. 验证相机、雷达、里程计、IMU、底盘和机械臂各自的连接与数据。
2. 运行 YOLO 演示，确认标签、输入输出形状及板端模型可用。
3. 验证 TF、地图、AMCL 初始定位，再测试单点导航。
4. 在受控动作范围内调试抓取阶段与 PID 参数。
5. 最后验证点餐、取餐、送餐与结算的任务流，并记录失败情况。

上述步骤为复现建议，不代表这些检查已在发布时通过。
