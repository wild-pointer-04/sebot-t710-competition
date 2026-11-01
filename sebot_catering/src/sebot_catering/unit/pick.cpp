/**
 * @file pick.cpp
 * @author Leo (sasu@saishukeji.com)
 * @brief 智能取餐场景
 * @version 0.1
 * @date 2024-03-11
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../src/picking.cpp"

int main(int argc, char **argv)
{
    ros::init(argc, argv, "sebot_picking");
    Picking picking;                                                 // 实例化自主点餐类
    int sencePick = 1;                                               // 1：抓取食物场景 | 2：放置食物场景 | 3：机械爪跟随食物控制
    ros::param::get("/sebot_picking/pidPoseKp", picking.pidPose.kP); // 方向控制PID
    ros::param::get("/sebot_picking/pidPoseKi", picking.pidPose.kI);
    ros::param::get("/sebot_picking/pidPoseKd", picking.pidPose.kD);
    ros::param::get("/sebot_picking/pidDisKp", picking.pidDis.kP); // 距离控制PID
    ros::param::get("/sebot_picking/pidDisKi", picking.pidDis.kI);
    ros::param::get("/sebot_picking/pidDisKd", picking.pidDis.kD);
    ros::param::get("/sebot_picking/pidLocalKp", picking.pidLocal.kP); // 位置控制PID
    ros::param::get("/sebot_picking/pidLocalKi", picking.pidLocal.kI);
    ros::param::get("/sebot_picking/pidLocalKd", picking.pidLocal.kD);
    ros::param::get("/sebot_picking/pidClawXKp", picking.pidClawX.kP); // 机械爪PID
    ros::param::get("/sebot_picking/pidClawXKi", picking.pidClawX.kI);
    ros::param::get("/sebot_picking/pidClawXKd", picking.pidClawX.kD);
    ros::param::get("/sebot_picking/pidClawYKp", picking.pidClawY.kP); // 机械爪PID
    ros::param::get("/sebot_picking/pidClawYKi", picking.pidClawY.kI);
    ros::param::get("/sebot_picking/pidClawYKd", picking.pidClawY.kD);
    ros::param::get("/sebot_picking/debug", picking.debug);
    ros::param::get("/sebot_picking/disClaw", picking.disClaw);     // 机械爪夹取食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_picking/disPick", picking.disPick);     // 机械臂抓取食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_picking/disSearch", picking.disSearch); // AI搜索食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_picking/sencePick", sencePick);

    ros::Rate loop_rate(30); // 设置ROS循环周期: Hz
    bool run = false;

    if (sencePick == 3)
        picking.setCamera(false);
    sleep(1);
    while (ros::ok())
    {
        if (sencePick == 1) // 抓取食物场景
        {
            if (!run)
                run = picking.pickupSomething(LABEL_AI_COLA);
        }
        else if (sencePick == 2) // 放置食物场景
        {
            picking.placeFood = 2;
            if (!run)
                run = picking.putdownSomething();
        }
        else if (sencePick == 3) // 机械爪跟随食物控制
        {
            picking.pickupSomething(LABEL_AI_COLA);
            picking.pickStep = picking.PICK_STEP_AIM;
        }

        ros::spinOnce();   // 回调话题通信
        loop_rate.sleep(); // ROS线程周期控制
    }

    picking.setJointEnable(false); // 机械臂失能

    return 0;
}
