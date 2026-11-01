/**
 * @file picking.cpp
 * @author Leo (sasu@saishukeji.com)
 * @brief 智能取餐场景服务（机械臂取放食物）
 * @version 0.1
 * @date 2024-03-07
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <geometry_msgs/Twist.h>   // 速控发布类
#include <sensor_msgs/LaserScan.h> // 订阅激光雷达数据
#include <chrono>
#include "../include/detection.hpp"
#include "../include/tools.hpp"
#include "../include/arm.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

/**
 * @brief 智能取餐场景类
 *
 */
class Picking
{
private:
    ros::NodeHandle rosNode;         // ROS节点类
    shared_ptr<Detection> detection; // AI目标检测类实例化指针
    VideoCapture captureDeep;        // 摄像头实例化类
    VideoCapture captureRgb;         // 摄像头实例化类
    ros::Subscriber subLaser;        // 订阅激光雷达话题
    ros::Publisher pubCmd;           // 发布速控话题
    long counter = 0;                // 计数器
    float orienOffset = 0.0;         // 姿态方向校正误差
    Talon talon;                     // 机械臂控制类

    /**
     * @brief 激光雷达类
     *
     */
    class Rplidar
    {
    public:
        float angle = 1.57;           // 激光雷达探测角度（机器人前方/rad）
        float angleIncrement = 0.087; // 间隔取点度数rad(≈5°)
        vector<float> rangesLeft;     // 选取左侧距离数据
        vector<float> rangesRight;    // 选取右侧距离数据
    };
    Rplidar rplidar; // 激光雷达类

    /**
     * @brief 目标点控制中心
     *
     */
    class Center
    {
    public:
        int x = COLSIMAGE; // X轴向
        int y = ROWSIMAGE; // Y轴向
    };

    /**
     * @brief 接收激光雷达数据
     * @note angle_min: -3.12413907051  angle_max: 3.14159274101    angle_increment: 0.0174532923847
     *      time_increment: 1.80087170065e-07   scan_time: 6.46512926323e-05    range_min: 0.193000003695
     *      range_max: 8.0
     * @param msg
     */
    void getLaser(const sensor_msgs::LaserScan::ConstPtr &msg)
    {
        rplidar.rangesLeft.clear();
        rplidar.rangesRight.clear();

        if (msg->ranges.size() > 0 && msg->angle_increment > 0)
        {
            if (rplidar.angleIncrement < msg->angle_increment)
                rplidar.angleIncrement = msg->angle_increment;
            int numLaser = rplidar.angle / rplidar.angleIncrement;    // 截取的激光点数 (90)
            int steps = msg->ranges.size();                           // 激光雷达总步长
            int step = rplidar.angleIncrement / msg->angle_increment; // 间隔取点步长

            if (steps > numLaser * step)
            {
                for (int i = 0; i < numLaser / 2; i++)
                {
                    if (msg->ranges[i * step] > 0.2 && msg->ranges[i * step] < 1.0)
                        rplidar.rangesLeft.push_back(msg->ranges[i * step]);
                    if (msg->ranges[steps - i * step - 1] > 0.2 && msg->ranges[steps - i * step - 1] < 1.0)
                        rplidar.rangesRight.push_back(msg->ranges[steps - i * step - 1]);
                }
            }
        }
    }

    /**
     * @brief 获取容器数据中值
     *
     * @param stream
     * @return float
     */
    float getMedian(vector<float> stream)
    {
        sort(stream.begin(), stream.end()); // 排序
        int size = stream.size();
        if (size & 1)                 // 位运算判断是否是奇数
            return stream[size >> 1]; // 奇数
        else
            return (stream[size >> 1] + stream[(size - 1) >> 1]) / 2; // 偶数个
    }

    /**
     * @brief 获取机械爪夹取食物的间距：cm
     *
     * @param foo
     * @return float
     */
    float getClawPosition(string food)
    {
        if (food == LABEL_AI_COLA) // 可乐
            return 4.4;
        else if (food == LABEL_AI_COFFEE) // 咖啡
            return 5.9;
        else if (food == LABEL_AI_BURGER) // 汉堡
            return 6.6;
        else if (food == LABEL_AI_CAKE) // 瑞士卷
            return 3.2;
        else if (food == LABEL_AI_CHEESE) // 奶酪
            return 2.7;
        else if (food == LABEL_AI_SUNDAE) // 圣代
            return 5.1;
        else
            return 5.0;
    }

    /**
     * @brief 控制机械爪搜索食物位置
     *
     * @param food
     */
    void clawFindFoods(string food)
    {
        Center center = searchFoodRgb(food);
        if (center.x < COLSIMAGE && center.y < ROWSIMAGE) // 检测到有效的目标食物
        {
            pidClawX.enable = true;
            pidClawY.enable = true;
        }
        else
        {
            pidClawX.enable = false;
            pidClawY.enable = false;
        }
        clawControl(center); // 机械爪运动控制
    }

public:
    Pid pidPose;           // PID姿态控制器：方向（角度）
    Pid pidDis;            // PID姿态控制器：距离（X轴）
    Pid pidLocal;          // PID姿态控制器：位置（Y轴）
    Pid pidClawX;          // PID姿态控制器：机械爪方向（X轴）
    Pid pidClawY;          // PID姿态控制器：机械爪方向（Y轴）
    bool debug = false;    // 调试使能：AI绘制+显示
    bool findFood = false; // AI搜索到目标食物标志
    float score = 0.4;     // AI置信度
    float disSearch = 0.8; // AI搜索食物距离：m
    float disPick = 0.45;   // 机械臂抓取食物距离：m
    float disClaw = 0.35;   // 机械爪夹取食物距离：m
    int placeFood = 1;     // 食物放置位置（餐桌）：1/中间，2/左边，3/右边

    /**
     * @brief 智能取餐任务流程
     *
     */
    enum PickStep
    {
        PICK_STEP_START = 0, // 任务开始
        PICK_STEP_POSE,      // 机器人姿态校正
        PICK_STEP_SEARCH,    // 食物搜索
        PICK_STEP_FORWARD,   // 机器人向前移动
        PICK_STEP_AIM,       // 机械爪瞄准食物
        PICK_STEP_GRAB,      // 食物抓取
        PICK_STEP_LOCAL,     // 机器人重定位
        PICK_STEP_END,       // 智能取餐结束
    };
    PickStep pickStep = PickStep::PICK_STEP_START; // 智能取餐任务流程

    Picking()
    {
        string pathPkg = ros::package::getPath("sebot_catering");   // 功能包文件根路径
        detection = make_shared<Detection>(pathPkg + "/res/model"); // 初始化NPU及AI模型
        detection->score = 0.4;                                     // AI检测置信度

        // 摄像头初始化
        setCamera(true);

        // ROS节点类
        subLaser = rosNode.subscribe("/scan", 1, &Picking::getLaser, this); // 订阅激光雷达话题
        pubCmd = rosNode.advertise<geometry_msgs::Twist>("/cmd_vel", 5);    // 速控话题发布器

        // PID控制器初始化
        pidPose.enable = true;
        pidDis.enable = true;
        pidDis.ref = 0.3;       // 机器人距离控制：m（激光雷达盲区20cm+10cm距离）
        pidDis.deadline = 0.05; // 机器人距离控制死区：m
        pidDis.outMax = 0.15;   // 限制输出速度：m/s
        pidLocal.deadline = 15; // 图像控制中心死区
        pidLocal.outMax = 0.2;  // 限制输出速度：m/s
        pidClawX.deadline = 10; // 图像控制中心死区
        pidClawX.outMax = 0.3;  // 限制输出角度:rad
        pidClawY.deadline = 5;  // 图像控制中心死区
        pidClawY.outMax = 0.2;  // 限制输出角度:rad
        sleep(1);
    };
    ~Picking()
    {
        robotCtrl(0, 0, 0); // 机器人运动控制
        captureDeep.release();
        captureRgb.release();
        talon.close(); // 释放串口线程
    };

    /**
     * @brief 机械臂使能/失能
     */
    void setJointEnable(bool enable)
    {
        talon.setJointEnable(talon.ArmJoint::ARM_JOINT_ALL, enable);
    }

    /**
     * @brief 选择相机通道
     *
     * @param depth 是否选择深度相机
     */
    void setCamera(bool depth)
    {
        if (depth) // 深度相机
        {
            captureDeep = VideoCapture("/dev/deepCamera"); // 打开摄像头
            if (!captureDeep.isOpened())
                ROS_ERROR_STREAM("can not open deepCamera !!!!");
            captureDeep.set(CAP_PROP_FRAME_WIDTH, COLSIMAGE);  // 设置图像的列
            captureDeep.set(CAP_PROP_FRAME_HEIGHT, ROWSIMAGE); // 设置图像的行
        }
        else // RGB相机
        {
            captureDeep.release();
            sleep(1);
            captureRgb = VideoCapture("/dev/video1"); // RGB摄像头初始化
            if (!captureRgb.isOpened())
                ROS_ERROR_STREAM("can not open rgbCamera !!!!");
            captureRgb.set(CAP_PROP_FRAME_WIDTH, COLSIMAGE);  // 设置图像的列
            captureRgb.set(CAP_PROP_FRAME_HEIGHT, ROWSIMAGE); // 设置图像的行
        }
    }

    /**
     * @brief  机器人运动控制
     *
     * @param linearX X轴线速度
     * @param linearY Y轴线速度
     * @param angular Z轴角速度
     */
    void robotCtrl(float linearX, float linearY, float angular)
    {
        geometry_msgs::Twist msgCmd; // 速控话题数据
        msgCmd.linear.x = linearX;
        msgCmd.linear.y = linearY;
        msgCmd.angular.z = angular;
        pubCmd.publish(msgCmd); // 发布速控话题
    }

    /**
     * @brief 机器人姿态控制（距离+方向+位置）
     *
     */
    void poseControl()
    {
        float linearX = 0.0, linearY = 0.0, angular = 0.0;

        if (rplidar.rangesLeft.size() >= 3 && rplidar.rangesRight.size() >= 3)
        {
            //[01] 方向控制
            if (pidPose.enable)
            {
                if (orienOffset > 0.3)
                    orienOffset = 0.3;
                else if (orienOffset < -0.3)
                    orienOffset = -0.3;
                pidPose.feedBack = getMedian(rplidar.rangesLeft) - getMedian(rplidar.rangesRight) - orienOffset; // ±0.3
                pidController(pidPose);                                                                          // 姿态PID控制器
                angular = pidPose.out;
            }

            //[02] 距离控制
            if (pidDis.enable)
            {
                if (pidDis.ref < 0.2) // 激光雷达盲区距离
                    pidDis.ref = 0.2;
                pidDis.feedBack = (getMedian(rplidar.rangesLeft) + getMedian(rplidar.rangesRight)) / 2; // ±0.3
                pidController(pidDis);
                linearX = -pidDis.out;
                // if (linearX < 0) // 限制机器人仅向前运动
                //     linearX = 0;
            }
            if (debug)
                ROS_ERROR_STREAM("[PidDis] error:" + to_string(pidDis.feedBack) + "  out:" + to_string(linearX));
        }

        // 位置控制
        if (pidLocal.enable)
        {
            pidController(pidLocal);
            linearY = pidLocal.out;
            // ROS_ERROR_STREAM("error:" + to_string(pidLocal.feedBack) + "  out:" + to_string(linearY));
        }

        robotCtrl(linearX, linearY, angular); // 机器人运动控制
    }


    void poseControl_180()
    {
        float linearX = 0.0, linearY = 0.0, angular = 0.0;

        if (rplidar.rangesLeft.size() >= 3 && rplidar.rangesRight.size() >= 3)
        {
                pidPose.feedBack = getMedian(rplidar.rangesLeft) - getMedian(rplidar.rangesRight) - 0.3; // ±0.3
                pidController(pidPose);                                                                          // 姿态PID控制器
                angular = pidPose.out;
        }

        robotCtrl(linearX, linearY, angular); // 机器人运动控制
    }

    /**
     * @brief 机械爪运动控制
     *
     */
    void clawControl(Center center)
    {
        if (pidClawX.enable)
        {
            pidClawX.ref = 0;
            pidClawX.feedBack = center.x;
            pdController(pidClawX);
        }

        if (pidClawY.enable)
        {
            pidClawY.ref = 0;
            pidClawY.feedBack = center.y;
            pdController(pidClawY);
        }

        talon.setClawMotion(-pidClawX.out, pidClawY.out); // 机械爪运动控制
    }

    /**
     * @brief 搜索食物
     *
     * @param food 食物AI标签
     * @return Center 返回控制中心：center
     */
    Center searchFoodDepth(string food)
    {
        Center center;
        Mat img;
        if (!captureDeep.read(img)) // 相机采集数据
            return center;

        // 启动AI推理
        detection->score = score;
        detection->inference(img); // NPU推理

        vector<PredictResult> results;
        for (int i = 0; i < detection->results.size(); i++)
        {
            if (detection->results[i].label == food &&
                detection->results[i].width < 80 &&
                detection->results[i].height < 120)
                results.push_back(detection->results[i]);
        }
        PredictResult result;
        result.score = 0;
        result.height = 0;
        result.width = 0;
        result.x = 0;
        result.y = 0;
        for (int i = 0; i < results.size(); i++)
        {
            // if (results[i].score >= result.score)//选择分数最优的目标物
            int row = results[i].y + results[i].height / 2;
            if (row > (result.y + result.height / 2) && row > ROWSIMAGE / 2) // 选择图像最底端的目标物
                result = results[i];
        }

        // 控制中心求取
        if (result.label == food)
        {
            center.x = result.x + result.width / 2 - COLSIMAGE / 2; // 校正相机中值（-30）
            center.y = result.y + result.height /2 - ROWSIMAGE / 2 ; //原值为0.5---6月26日改
        }

        if (debug) // 调试模式
        {
            // 绘制目标框
            auto score = std::to_string(result.score);
            int pointY = result.y - 20;
            if (pointY < 0)
                pointY = 0;
            cv::Rect rectText(result.x, pointY, result.width, 20);
            cv::rectangle(img, rectText, cv::Scalar(0, 255, 0), -1);
            std::string label_name = result.label + " [" + score.substr(0, score.find(".") + 2) + "]";
            cv::Rect rect(result.x, result.y, result.width, result.height);
            cv::rectangle(img, rect, cv::Scalar(0, 255, 0), 1);
            cv::putText(img, label_name, Point(result.x, result.y), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 254), 1);

            imshow("img", img);
            waitKey(1);
        }

        return center;
    }

    /**
     * @brief 搜索食物(RGB相机)
     *
     * @param food 食物AI标签
     * @return Center 返回控制中心：center
     */
    Center searchFoodRgb(string food)
    {
        Center center;
        Mat img;
        if (!captureRgb.read(img)) // 相机采集数据RGB
            return center;

        // 启动AI推理
        detection->score = score;
        detection->inference(img); // NPU推理

        vector<PredictResult> results;
        for (int i = 0; i < detection->results.size(); i++)
        {
            if (detection->results[i].label == food)
                results.push_back(detection->results[i]);
        }
        PredictResult result;
        result.score = 0;
        result.height = 0;
        result.width = 0;
        result.x = 0;
        result.y = 0;
        for (int i = 0; i < results.size(); i++)
        {
            if (results[i].score >= result.score)
                result = results[i];
        }

        // 控制中心求取
        if (result.label == food)
        {
	    if (food == LABEL_AI_CHEESE)
	    {
	        center.x = result.x + result.width / 2 - COLSIMAGE / 2;       // 校正相机中值（-30）
                center.y = (result.y + result.height * 0.55) - ROWSIMAGE / 2 + 52; // 矫正机械爪，微调向下抓取食物
	    }
	    else if (food == LABEL_AI_COFFEE)
	    {
	        center.x = result.x + result.width / 2 - COLSIMAGE / 2;       // 校正相机中值（-30）
                center.y = (result.y + result.height * 0.55) - ROWSIMAGE / 2 + 2; // 矫正机械爪，微调向下抓取食物
	    }
        else if (food == LABEL_AI_SUNDAE)
        {
                center.x = result.x + result.width / 2 - COLSIMAGE / 2;       // 校正相机中值（-30）
                center.y = (result.y + result.height * 0.55) - ROWSIMAGE / 2 + 9; // 矫正机械爪，微调向下抓取食物
	    }
	    else {
                center.x = result.x + result.width / 2 - COLSIMAGE / 2;       // 校正相机中值（-30）
                center.y = (result.y + result.height * 0.55) - ROWSIMAGE / 2; // 矫正机械爪，微调向下抓取食物
	    }
        }

        if (debug) // 调试模式
        {
            // 绘制目标框
            auto score = std::to_string(result.score);
            int pointY = result.y - 20;
            if (pointY < 0)
                pointY = 0;
            cv::Rect rectText(result.x, pointY, result.width, 20);
            cv::rectangle(img, rectText, cv::Scalar(0, 255, 0), -1);
            std::string label_name = result.label + " [" + score.substr(0, score.find(".") + 2) + "]";
            cv::Rect rect(result.x, result.y, result.width, result.height);
            cv::rectangle(img, rect, cv::Scalar(0, 255, 0), 1);
            cv::putText(img, label_name, Point(result.x, result.y), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 254), 1);

            imshow("img", img);
            waitKey(1);
        }

        return center;
    }

    /**
     * @brief 智能取餐(食物抓取场景)
     *
     * @param food 目标食物AI标签
     * @return true 返回任务完成状态
     * @return false
     */
    bool pickupSomething(string food)
    {
        static int count = 0; // AI检测计数器
        static int scene = 0; // 图像场计数器
        static long thisTime = getSystTime();
        static long countLeft = 0;
        static long countRight = 0;    // 左右移动计数器
        static bool searchLeft = true; // 向左移动搜索目标

        switch (pickStep)
        {
        case PickStep::PICK_STEP_START: // 任务开始
        {
            pickStep = PickStep::PICK_STEP_POSE; // 切换智能取餐任务流程
            count = 0;                           // AI检测计数器
            scene = 0;                           // 图像场计数器
            counter = getSystTime();
            break;
        }

        case PickStep::PICK_STEP_POSE: // 机器人姿态校正
        {
            Center center = searchFoodDepth(food);
            if (center.x < COLSIMAGE && center.y < ROWSIMAGE) // 检测到有效的目标食物
            {
                pidLocal.enable = true; // 使能位置PID控制
                pidLocal.feedBack = center.x;
                count++;
            }
            else
                pidLocal.enable = false;
            if (count > 0)
            {
                scene++;
                if (scene > 10)
                {
                    if (count > 5)
                        findFood = true;
                    scene = 0;
                    count = 0;
                }
            }
            pidDis.ref = disSearch; // AI搜索食物距离：m
            poseControl();          // 机器人姿态控制（距离+方向+位置）

            // 姿态校正状态检测
            if ((abs(pidDis.feedBack - pidDis.ref) < 0.06 &&
                 abs(pidPose.feedBack) < 0.06) ||
                (getSystTime() - counter) > 4000) // 姿态调制至误差范围or超时
            {
                for (int i = 0; i < 5; i++) // 机器人停止运动
                {
                    robotCtrl(0, 0, 0);
                    talon.setActions(talon.ACTION_EXT);             // 机械臂动作：伸展
                    usleep(30 * 1000);                              // 等待：us
                    talon.setJointPosition(talon.ARM_JOINT_6, 9.3); // 机械爪张开：9cm
                }
                if (abs(pidLocal.feedBack - pidLocal.ref) <= pidLocal.deadline && findFood) // 面对目标食物姿态已校正Ok
                {
                    counter = getSystTime(); // 更新计时器
                    // 机器人运动控制
                    setCamera(false);                                                                // 切换RGB相机
                    while (!talon.actionOver[talon.ACTION_EXT] && (getSystTime() - counter) < 12000) // 等待机械臂伸展完成
                    {
                        sleep(1);
                        robotCtrl(0, 0, 0); // 机器人运动控制
                    }
                    sleep(2);
                    talon.setClawInitial();                 // 初始化（标定）机械爪
                    pickStep = PickStep::PICK_STEP_FORWARD; // 切换智能取餐任务流程
                    pidPose.outMax = 0.2;                   // 限制角速度：rad/s
                    pidDis.outMax = 0.1;                    // 限制输出速度：m/s
                    pidLocal.enable = false;                // 位置PID控制
                    count = 0;
                    pidDis.ref = disPick; // 机械臂抓取食物距离：m
                    orienOffset = 0;      // 姿态校正误差=0
                    ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                    robotCtrl(0, 0, 0);      // 机器人运动控制
                    counter = getSystTime(); // 更新计时器
                }
                else
                {
                    pickStep = PickStep::PICK_STEP_SEARCH; // 切换智能取餐任务流程
                    ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                    counter = getSystTime();
                    thisTime = getSystTime();
                    countLeft = 0;
                    countRight = 0;    // 左右移动计数器
                    searchLeft = true; // 向左移动搜索目标
                }

                count = 0;
                scene = 0;
                findFood = false;
            }
        }
        break;

        case PickStep::PICK_STEP_SEARCH: // 食物搜索
        {
            Center center = searchFoodDepth(food);
            if (center.x < COLSIMAGE && center.y < ROWSIMAGE) // 检测到有效的目标食物
            {
                pidLocal.enable = true; // 使能位置PID控制
                pidLocal.feedBack = center.x;
                count++;
                if (count > 3 && center.x < -pidLocal.deadline) // 目标物在左侧
                    searchLeft = true;
                else if (count > 3 && center.x > pidLocal.deadline) // 目标物在右侧
                    searchLeft = false;
            }
            else if (!findFood && (getSystTime() - counter) > 3000) // 如果AI未检测到食物
            {
                pidLocal.enable = true;
                if (searchLeft) // 向右转1s
                {
                    if (countLeft < 2000)
                    {
                        countLeft += getSystTime() - thisTime;
                        orienOffset = -0.1; // 机器人向右转
                    }
                    else
                        searchLeft = false;
                }
                else if (countRight < 4000) // 向左转3s
                {
                    countRight += getSystTime() - thisTime;
                    orienOffset = 0.1; // 机器人向左转
                }
            }
            thisTime = getSystTime(); // 更新控制周期计时
            pidDis.ref = disSearch;   // AI搜索食物距离：m
            poseControl();            // 机器人姿态控制（距离+方向+位置）

            if (!findFood) // 未检测到目标食物
            {
                if (count > 0)
                {
                    scene++;
                    if (scene > 20)
                    {
                        if (count > 10)
                            findFood = true;
                        scene = 0;
                        count = 0;
                    }
                }
            }
            else // 已检测到目标食物
            {
                orienOffset = 0;                                                                               // 姿态校正误差=0
                if (abs(pidLocal.feedBack - pidLocal.ref) <= pidLocal.deadline && abs(pidPose.feedBack) < 0.1) // 角度+位置调整完成
                {
                    counter = getSystTime(); // 更新计时器
                    robotCtrl(0, 0, 0);      // 机器人运动控制
                    setCamera(false);        // 切换RGB相机

                    while (!talon.actionOver[talon.ACTION_EXT] && (getSystTime() - counter) < 8000) // 等待机械臂伸展完成
                    {
                        sleep(1);
                        robotCtrl(0, 0, 0); // 机器人运动控制
                    }
                    sleep(2);
                    talon.setClawInitial();                 // 初始化（标定）机械爪
                    pickStep = PickStep::PICK_STEP_FORWARD; // 切换智能取餐任务流程
                    pidPose.outMax = 0.2;                   // 限制角速度：rad/s
                    pidDis.outMax = 0.1;                    // 限制输出速度：m/s
                    pidLocal.enable = false;                // 位置PID控制
                    count = 0;
                    pidDis.ref = disPick;    // 机械臂抓取食物距离：m
                    counter = getSystTime(); // 更新计时器
                    ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                }
            }

            if (getSystTime() - counter > 15000) // 搜索超时：15s
            {
                pickStep = PickStep::PICK_STEP_LOCAL; // 任务结束
                ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                orienOffset = 0;    // 姿态校正误差=0
                robotCtrl(0, 0, 0); // 机器人运动控制
                findFood = false;   // 搜索食物失败
            }
        }
        break;

        case PickStep::PICK_STEP_FORWARD: // 机器人向前移动
        {
            if ((abs(pidDis.feedBack - pidDis.ref) < 0.1 && abs(pidPose.feedBack) < 0.06))
                count++;
            if ((getSystTime() - counter) > 8000 || count > 10) // 搜索超时or姿态调整完毕
            {
                count = 0;
                pickStep = PickStep::PICK_STEP_AIM; // 切换智能取餐任务流程
                robotCtrl(0, 0, 0);                 // 机器人运动控制
                ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                counter = getSystTime(); // 更新计时器
            }
            else
            {
                pidDis.ref = disPick; // 机械臂抓取食物距离：m
                poseControl();        // 机器人姿态控制（距离+方向）
            }

            // 机械爪跟随食物控制
            Center center = searchFoodRgb(food);
            if (center.x < COLSIMAGE && center.y < ROWSIMAGE) // 检测到有效的目标食物
                pidClawX.enable = true;
            else
                pidClawX.enable = false;

            pidClawY.enable = false;
            clawControl(center); // 机械爪运动控制
        }
        break;

        case PickStep::PICK_STEP_AIM: // 机械爪瞄准食物
        {
            robotCtrl(0, 0, 0);  // 机器人运动控制
            clawFindFoods(food); // 机械爪跟随食物控制

            if (abs(pidClawX.feedBack) < pidClawX.deadline) // 机械爪初次定位准确
                count++;

            if (count > 10 || (getSystTime() - counter) > 5000) // 爪子对准 or 超时
            {
                pickStep = PickStep::PICK_STEP_GRAB; // 切换智能取餐任务流程
                count = 0;
                counter = getSystTime(); // 更新计时器
		//if (food == LABEL_AI_SUNDAE)
		//    {pidDis.ref = disClaw + 0.01;}
		//else if (food == LABEL_AI_BURGER)
		//{
                //    pidDis.ref = disClaw - 0.01;    // 机械爪夹取食物距离：m
		//}
		//else 
		//{
                    pidDis.ref = disClaw - 0.01;    // 机械爪夹取食物距离：m
		//}
                pidDis.outMax = 0.06;    // 限制输出速度：m/s , 机器人缓慢向前进
            }
            break;
        }

        case PickStep::PICK_STEP_GRAB: // 食物抓取
        {
            //[01] 机械爪瞄准食物控制
            clawFindFoods(food); // 机械爪跟随食物控制

            //[02] 机器人距离控制
            poseControl();                                                                    // 机器人姿态控制（距离+方向）
            if (abs(pidDis.feedBack - pidDis.ref) < 0.05 || (getSystTime() - counter) > 5000) // 姿态调制至误差范围or超时
            {
                for (int i = 0; i < 5; i++) // 机器人停止运动
                {
                    robotCtrl(0, 0, 0);
                    usleep(30 * 1000);                                                // 等待：us
                    talon.setJointPosition(talon.ARM_JOINT_6, getClawPosition(food)); // 机械爪夹取
                }
                sleep(13 - getClawPosition(food)); // 等待机械爪夹取结束

                for (int i = 0; i < 5; i++) // 机械爪运动控制: 往上抬0.3rad
                {
                    usleep(400 * 1000); // 等待：us
                    talon.setClawMotion(-pidClawX.out, 0.3);
                }

                pickStep = PickStep::PICK_STEP_LOCAL; // 切换智能取餐任务流程
                ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
                counter = getSystTime();
                findFood = true;     // 搜索食物成功
                pidDis.outMax = 0.2; // 限制输出速度：m/s
            }
            break;
        }

        case PickStep::PICK_STEP_LOCAL: // 机器人重定位
        {
            talon.setClawMotion(-pidClawX.out, 0.3); // 机械爪运动控制: 往上抬0.3rad
            pidDis.ref = disSearch;                  // 机器人后退，导航适合距离
            poseControl();                           // 机器人姿态控制（距离+方向）

            if (abs(pidDis.feedBack - pidDis.ref) < 0.05 || (getSystTime() - counter) > 5000) // 姿态调制至误差范围or超时
            {
                for (int i = 0; i < 3; i++) // 机器人停止运动
                {
                    robotCtrl(0, 0, 0);
                    usleep(30 * 1000);                  // 等待：us
                    talon.setActions(talon.ACTION_CUR); // 机械臂动作：收缩
                }
                pickStep = PickStep::PICK_STEP_END; // 结束
                ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
            }
            break;
        }
        }

        if (pickStep == PickStep::PICK_STEP_END)
            {
                // orienOffset = 0.1;
                for (int i = 0; i < 110; i++) // 机器人停止运动
                {
                    usleep(90 * 1000);                  // 等待：us
                    poseControl_180();
                }
                
                return true;
            }
            
        else
            return false;
    }

    /**
     * @brief 放置食物
     *
     */
    bool putdownSomething()
    {
        switch (pickStep)
        {
        case PickStep::PICK_STEP_START: // 任务开始
            counter = getSystTime();
            pickStep = PickStep::PICK_STEP_POSE; // 切换智能取餐任务流程
            break;

        case PickStep::PICK_STEP_POSE: // 机器人姿态校正
        {
            pidLocal.enable = false;    // 关闭位置PID控制
            pidDis.ref = disPick + 0.1; // 机械臂抓取食物距离：m

            // 姿态校正状态检测
            if ((abs(pidDis.feedBack - pidDis.ref) < 0.1 &&
                 abs(pidPose.feedBack) < 0.1) ||
                (getSystTime() - counter) > 3000) // 姿态调制至误差范围or超时
            {
                pickStep = PickStep::PICK_STEP_GRAB; // 切换智能取餐任务流程

                for (int i = 0; i < 5; i++)          // 机器人停止运动
                {
                    robotCtrl(0, 0, 0);
                    usleep(30 * 1000);                  // 等待：us
                    talon.setActions(talon.ACTION_PUT); // 机械臂动作：放置食物
                }
                counter = getSystTime();
                while (!talon.actionOver[talon.ACTION_PUT] && (getSystTime() - counter) < 10000) // 等待机械臂伸展完成
                {
                    sleep(1);
                    robotCtrl(0, 0, 0); // 机器人运动控制
                }
                sleep(2);
                talon.setClawInitial(); // 初始化（标定）机械爪
                counter = getSystTime();
            }
            else
                poseControl(); // 机器人姿态控制（距离+方向+位置）
        }
        break;

        case PickStep::PICK_STEP_GRAB: // 食物放置
        {
            pidDis.ref = disClaw; // 机械臂放置食物距离：m
            poseControl();        // 机器人姿态控制（距离+方向+位置）

            if (placeFood == 2)
                talon.setClawMotion(-0.35, 0); // 机械爪运动控制: 左侧放置食物
            else if (placeFood == 3)
                talon.setClawMotion(0.35, 0); // 机械爪运动控制: 右侧放置食物

	        talon.setJointPosition_for_j3(talon.ARM_JOINT_3, -1.04); 

            // 姿态校正状态检测
            if ((abs(pidDis.feedBack - pidDis.ref) < 0.1 &&
                 abs(pidPose.feedBack) < 0.1) ||
                (getSystTime() - counter) > 3000) // 姿态调制至误差范围or超时
            {
                pickStep = PickStep::PICK_STEP_LOCAL; // 切换智能取餐任务流程
                for (int i = 0; i < 5; i++)
                {
                    robotCtrl(0, 0, 0); // 机器人运动控制
                    usleep(100 * 1000);
                    talon.setJointPosition(talon.ARM_JOINT_6, 9.2); // 机械爪放置食物
                }
                sleep(6);                   // 等待食物放置完成
                pidDis.ref = disPick + 0.1; // 机器人后退，导航适合距离+机械臂收回
                counter = getSystTime();
            }
            break;
        }

        case PickStep::PICK_STEP_LOCAL: // 机器人重定位
        {
            if (abs(pidDis.feedBack - pidDis.ref) < 0.05 || (getSystTime() - counter) > 5000) // 姿态调制至误差范围or超时
            {
                talon.setJointPosition_for_j3(talon.ARM_JOINT_3, -1.16); 

                for (int i = 0; i < 10; i++) // 机器人停止运动
                {
                    robotCtrl(0, 0, 0);
                    usleep(200 * 1000);                 // 等待：us
                    talon.setActions(talon.ACTION_CUR); // 机械臂动作：收缩
                }
                pickStep = PickStep::PICK_STEP_END; // 结束
                ROS_ERROR_STREAM("----------- STEP: " + to_string(pickStep) + " -----------");
            }
            else
            {
                poseControl(); // 机器人姿态控制（距离+方向）
            }
            break;
        }
        }

        if (pickStep == PickStep::PICK_STEP_END) {
            if (!talon.actionOver[talon.ACTION_CUR]) {
                talon.setActions(talon.ACTION_CUR);
            }
            for (int i = 0; i < 110; i++) // 机器人停止运动
                {
                    usleep(90 * 1000);                  // 等待：us
                    poseControl_180();
                }
            return true;
        }
        else
            return false;
    }
};
