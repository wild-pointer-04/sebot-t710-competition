# 来源与许可说明

本仓库由维护者提供的 SEBOT T710 竞赛工程整理发布，保存源码、配置、地图、模型、运行资源及原有作者标注。原始根 README 将其描述为“商服机器人，赛事产品，T710 板卡 ROS 工程代码”。

## 组件来源

工程包含 SEBOT / 赛曙平台相关文件，以及 ROS navigation、robot_pose_ekf、GMapping、Hector SLAM、RPLIDAR、STDR 等组件。具体来源、版权与许可应以各目录的 `package.xml`、LICENSE 和源文件头为准，例如：

- [餐饮应用包元数据](../sebot_catering/src/sebot_catering/package.xml)
- [ROS navigation 包元数据](../sebot_ros_kits/src/sebot_navigation/navigation/package.xml)
- [RPLIDAR 许可](../sebot_ros_kits/src/sebot_driver/rplidar_ros/LICENSE)
- [STDR 许可](../sebot_ros_stdr/src/sebot_stdr/LICENSE)
- [定位工具许可](../sebot_location/src/sebot_location/LICENSE)

部分平台包使用非标准许可标识或保留原版权声明，因此本次整理未给整个仓库重新赋予 MIT、Apache-2.0 等统一许可证，也未将上游实现表述为维护者从零原创。模型、媒体和字体资源同样需要遵循各自适用的使用条件。

## 整理范围

- 重写中文 README，并增加英文介绍、架构图、技术导航与复现说明。
- 增加 `.gitignore` / `.gitattributes`，排除编译缓存、编辑器缓存与本地录音。
- 保留算法实现、硬件接口、模型及运行资源。
- 将两个语音脚本的硬编码云服务凭据改为环境变量读取。
- 保留原作者、版权信息和组件内许可证；未添加未经确认的奖项、性能提升比例或评测结果。

项目扩展描述与当前代码证据的对应关系见[实现范围](IMPLEMENTATION_STATUS.md)。
