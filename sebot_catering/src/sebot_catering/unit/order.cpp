/**
 * @file order.cpp
 * @author Leo (sasu@saishukeji.com)
 * @brief 自主点餐-UT单元测试
 * @version 0.1
 * @date 2024-03-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../src/ordering.cpp"

int main(int argc, char **argv)
{
    ros::init(argc, argv, "sebot_ordering");
    Ordering ordering;                                                 // 实例化自主点餐类
    ros::param::get("/sebot_ordering/pidPoseKp", ordering.pidPose.kP); // 方向控制PID
    ros::param::get("/sebot_ordering/pidPoseKi", ordering.pidPose.kI);
    ros::param::get("/sebot_ordering/pidPoseKd", ordering.pidPose.kD);
    ros::param::get("/sebot_ordering/pidDisKp", ordering.pidDis.kP); // 距离控制PID
    ros::param::get("/sebot_ordering/pidDisKi", ordering.pidDis.kI);
    ros::param::get("/sebot_ordering/pidDisKd", ordering.pidDis.kD);
    ros::param::get("/sebot_ordering/distance", ordering.pidDis.ref);   // 机器人距离阈值：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_picking/pidLocalKp", ordering.pidLocal.kP); // 位置控制PID
    ros::param::get("/sebot_picking/pidLocalKi", ordering.pidLocal.kI);
    ros::param::get("/sebot_picking/pidLocalKd", ordering.pidLocal.kD);
    ros::param::get("/sebot_ordering/debug", ordering.debug);

    ros::Rate loop_rate(20); // 设置ROS循环周期: Hz
    bool run = false;
    while (ros::ok())
    {
        if (!run)
            run = ordering.getOrders();
        else
        {
            std::cout << "orders: ";
            for (int i = 0; i < ordering.orders.size(); i++)
            {
                std::cout << ordering.orders[i] << " ";
            }
            std::cout << "." << std::endl;
        }
        ros::spinOnce();   // 回调话题通信
        loop_rate.sleep(); // ROS线程周期控制
    }
    return 0;
}
