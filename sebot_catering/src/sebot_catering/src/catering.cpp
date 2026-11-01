/**
 * @file catering.cpp
 * @author Leo (sasu@saishukeji.com)
 * @brief 送餐到桌场景（基于MultiNavi）
 * @version 0.1
 * @date 2024-03-05
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_datatypes.h>
#include "../include/tinyxml.hpp"
#include "../include/tools.hpp"
#include "ordering.cpp"
#include "picking.cpp"
#include "paying.cpp"
#include <signal.h>

using namespace std;

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient; // moveBase导航通信类

/**
 * @brief 智能餐饮数据类
 *
 */
class Cater
{
private:
#pragma region "数据结构声明"

    /**
     * @brief  智能餐饮场景流程（状态机标志）
     *
     */
    enum CaterStep
    {
        CATER_STEP_START = 0, // 任务开始
        CATER_STEP_INIT,      // 机器人重定位
        CATER_STEP_DELIVERY,  // 送餐到桌
        CATER_STEP_PAY,       // 智能结算
        CATER_STEP_END        // 任务结束
    };

    /**
     * @brief 导航目标点类型
     *
     */
    enum StationNavi
    {
        STATION_TABLE = 0, // 餐桌
        STATION_SERVING,   // 取餐台
        STATION_COUNTER    // 收银台
    };

    /**
     * @brief 餐桌场景数据类
     *
     */
    class Table
    {
    public:
        move_base_msgs::MoveBaseGoal position; // 餐桌位置信息
        vector<string> orders;                 // 订单信息
        bool ordered = false;                  // 是否启动过[自主点餐]任务标志
        bool putting = false;                  // 放置食物
        int foodPlace = 1;                     // 餐桌累计放置食物数量
    };

    /**
     * @brief 餐厅场景定位信息类
     *
     */
    class Location
    {
    public:
        vector<Table> tables;                 // 餐桌位置信息（1~6个）
        move_base_msgs::MoveBaseGoal serving; // 取餐台位置信息
        move_base_msgs::MoveBaseGoal origin;  // 起始点位置信息
        move_base_msgs::MoveBaseGoal counter; // 收银台位置信息
    };

    /**
     * @brief 导航相关参数
     *
     */
    class Navigation
    {
    public:
        bool arriving = false;                             // 等待导航到达目标点标志
        bool overtime = false;                             // 导航超时查验标志
        int timeout = 10000;                               // 导航超时时间：ms
        geometry_msgs::PoseWithCovarianceStamped amclPose; // AMCL重定位坐标
        StationNavi station = StationNavi::STATION_TABLE;  // 导航目标点类型
    };

    ros::NodeHandle rosNode;                           // ROS节点类
    ros::Publisher pubAudio;                           // 语音播报话题发布
    ros::Publisher pubInitPose;                        // 发布机器人重定位话题
    ros::Subscriber subAmclPose;                       // 接收AMCL话题
    MoveBaseClient actionMoveBase;                     // Movebase导航目标通信
    bool enable = true;                                // 场景初始化标志(资源加载成功)
    long timeBegin;                                    // 任务开始计时
    long counter;                                      // 任务计数器
    Location location;                                 // 餐厅场景定位信息类
    CaterStep caterStep = CaterStep::CATER_STEP_START; // 智能餐饮场景流程（状态机标志）
    int tableId = 1;                                   // 餐桌ID编号
    vector<Order> ordersPaying;                        // 智能结算订单数据

#pragma endregion

    /**
     * @brief 获取AMCL定位信息（重定位/导航成功后会接收此话题）
     *
     * @param pose
     */
    void getAmclPose(const geometry_msgs::PoseWithCovarianceStamped &pose)
    {
        navi.amclPose = pose;
    }

    /**
     * @brief 验证AMCL定位坐标与导航目标是否一致
     *
     * @param goal
     * @return true
     * @return false
     */
    bool checkGoal(move_base_msgs::MoveBaseGoal goal)
    {
        // 误差可合理设置更大一些，因为降低AMCL的重复定位频率（CPU消耗），AMCL位置更新可能更慢
        if (abs(goal.target_pose.pose.position.x - navi.amclPose.pose.pose.position.x) < 0.5 &&
            abs(goal.target_pose.pose.position.y - navi.amclPose.pose.pose.position.y) < 0.5 &&
            abs(abs(goal.target_pose.pose.orientation.w) - abs(navi.amclPose.pose.pose.orientation.w)) < 0.5) // 校验AMCL定位信息与导航目标点一致
        {
            return true; // 重定位成功
        }
        else
            return false;
    }

    /**
     * @brief 字符串解析（double）
     *
     * @param str
     * @return std::vector<double>
     */
    vector<double> stringTodouble(std::string str)
    {
        std::string token;
        std::istringstream iss(str);
        std::vector<double> doubles;

        while (std::getline(iss, token, ' ')) // 分离空格符
        {
            try
            {
                doubles.push_back(std::stod(token)); // str转double
            }
            catch (const std::invalid_argument &e)
            {
                ROS_ERROR_STREAM("Invalid number format: " + token); // 字符格式错误
            }
            catch (const std::out_of_range &e)
            {
                ROS_ERROR_STREAM("Number out of range: " + token); // 数值超出double范围
            }
        }
        return doubles;
    }

    /**
     * @brief 解析MoveBaseGoal数据类型
     *
     * @param position 坐标
     * @param orientation 方向
     * @return move_base_msgs::MoveBaseGoal
     */
    move_base_msgs::MoveBaseGoal getMoveBaseGoal(std::string position, std::string orientation)
    {
        move_base_msgs::MoveBaseGoal goal;

        std::vector<double> positions = stringTodouble(position);       // 字符串解析（double）
        std::vector<double> orientations = stringTodouble(orientation); // 字符串解析（double）

        if (positions.size() >= 3 && orientations.size() >= 4) // 数据准确性校验
        {
            goal.target_pose.pose.position.x = positions[0];
            goal.target_pose.pose.position.y = positions[1];
            goal.target_pose.pose.position.z = positions[2];
            goal.target_pose.pose.orientation.x = orientations[0];
            goal.target_pose.pose.orientation.y = orientations[1];
            goal.target_pose.pose.orientation.z = orientations[2];
            goal.target_pose.pose.orientation.w = orientations[3];
            goal.target_pose.header.frame_id = "map";
        }
        else
            ROS_ERROR_STREAM("MoveBaseGoal conversion failed!!");

        return goal;
    }

    /**
     * @brief 位置信息加载（读取xml文件）
     *
     * @param xml
     */
    Location locationFromXml()
    {
        string pathXml = ros::package::getPath("sebot_catering") + "/res/location.xml";

        Location location; // 餐厅场景定位信息类
        if (pathXml.empty())
        {
            ROS_ERROR_STREAM("Location'xml file is empty!!!");
            enable = false; // 场景初始化标志
            return location;
        }

        TiXmlDocument *xmlFile = new TiXmlDocument();                    // 实例化xml文档类
        if (!xmlFile->LoadFile(pathXml.c_str(), TIXML_ENCODING_UNKNOWN)) // 加载xml文件
        {
            ROS_ERROR_STREAM("Can't load xml file!");
            enable = false; // 场景初始化标志
            return location;
        }

        TiXmlElement *root = xmlFile->RootElement(); // 读取xml根节点
        if (!root)
        {
            ROS_ERROR_STREAM("Failed to get root element!");
            enable = false; // 场景初始化标志
            return location;
        }

        vector<Table> tables;
        for (TiXmlElement *child = root->FirstChildElement(); child; child = child->NextSiblingElement()) // 遍历坐标数据
        {
            const char *nameEle = child->Value();
            move_base_msgs::MoveBaseGoal goal;

            std::string position = child->FirstChildElement("local")->Attribute("position");       // 读取position属性
            std::string orientation = child->FirstChildElement("local")->Attribute("orientation"); // 读取orientation属性
            if (!position.empty() && !orientation.empty())
            {
                if (!std::strcmp(nameEle, "table")) // 餐桌定位数据
                {
                    move_base_msgs::MoveBaseGoal goal = getMoveBaseGoal(position, orientation); // 解析MoveBaseGoal数据类型
                    for (int i = 1; i < 7; i++)                                                 // 解析坐标点序号
                    {
                        if (child->Attribute("name") == ("餐桌-" + to_string(i)))
                        {
                            goal.target_pose.header.seq = i;
                            break;
                        }
                    }

                    Table table;
                    table.position = goal;
                    tables.push_back(table);
                }

                else if (!std::strcmp(nameEle, "serving"))                     // 取餐台定位数据
                    location.serving = getMoveBaseGoal(position, orientation); // 解析MoveBaseGoal数据类型
                else if (!std::strcmp(nameEle, "origin"))                      // 起始点定位数据
                    location.origin = getMoveBaseGoal(position, orientation);  // 解析MoveBaseGoal数据类型
                else if (!std::strcmp(nameEle, "counter"))                     // 收银台定位数据
                    location.counter = getMoveBaseGoal(position, orientation); // 解析MoveBaseGoal数据类型
            }
        }
        for (int i = 0; i < tables.size(); i++)
        {
            for (int j = 0; j < tables.size(); j++)
            {
                if (tables[j].position.target_pose.header.seq == i + 1)
                {
                    location.tables.push_back(tables[i]);
                    break;
                }
            }
        }

        return location;
    }

    /**
     * @brief 机器人重定位（初始化位置校正）
     *
     */
    void initialPose(move_base_msgs::MoveBaseGoal goal)
    {
        if (!enable)
            return;
        geometry_msgs::PoseWithCovarianceStamped msgPose; // 重定位话题msg
        msgPose.header.stamp = ros::Time::now();          // 当前时间戳
        msgPose.header.frame_id = "map";                  // 参考坐标系，通常是地图坐标系
        msgPose.pose.pose.position = goal.target_pose.pose.position;
        msgPose.pose.pose.orientation = goal.target_pose.pose.orientation;
        msgPose.pose.covariance[0] = 0.25; // 协方差矩阵
        msgPose.pose.covariance[6 * 1 + 1] = 0.25;
        msgPose.pose.covariance[6 * 5 + 5] = 0.06853891945200942;

        pubInitPose.publish(msgPose); // 发布机器人重定位话题
    }

    /**
     * @brief 设置智能取餐相关配置
     *
     * @param picking
     */
    void setPicking(Picking &picking)
    {
        picking.pidPose.kP = pidConfig.pidPoseKp;
        picking.pidPose.kI = pidConfig.pidPoseKi;
        picking.pidPose.kD = pidConfig.pidPoseKd;
        picking.pidDis.kP = pidConfig.pidDisKp;
        picking.pidDis.kI = pidConfig.pidDisKi;
        picking.pidDis.kD = pidConfig.pidDisKd;
        picking.disClaw = disClaw;
        picking.disPick = disPick;
        picking.disSearch = disSearch;
        picking.pidLocal.kP = pidConfig.pidLocalKp;
        picking.pidLocal.kI = pidConfig.pidLocalKi;
        picking.pidLocal.kD = pidConfig.pidLocalKd;
        picking.pidClawX.kP = pidConfig.pidClawXKp;
        picking.pidClawX.kI = pidConfig.pidClawXKi;
        picking.pidClawX.kD = pidConfig.pidClawXKd;
        picking.pidClawY.kP = pidConfig.pidClawYKp;
        picking.pidClawY.kI = pidConfig.pidClawYKi;
        picking.pidClawY.kD = pidConfig.pidClawYKd;
        picking.debug = debug;
        picking.score = score;
    }

    /**
     * @brief 设置智能取餐相关配置
     *
     * @param picking
     */
    void sysExit()
    {
        Talon talon; // 机械臂控制类
        sleep(2);
        talon.setJointEnable(talon.ArmJoint::ARM_JOINT_ALL, false); // 机械臂失能
        sleep(1);
        talon.setJointEnable(talon.ArmJoint::ARM_JOINT_ALL, false); // 机械臂失能
    }

public:
    Navigation navi;         // 导航相关参数
    bool simulation = false; // STDR仿真模式

    /**
     * @brief 智能取餐相关配置
     *
     */
    class PidConfig
    {
    public:
        float pidPoseKp = 12.0;    // 方向PID：比例系数
        float pidPoseKi = 3.3;     // 方向PID：积分系数
        float pidPoseKd = 10.0;    // 方向PID：微分系数
        float pidDisKp = 5.0;      // 距离PID：比例系数
        float pidDisKi = 0.3;      // 距离PID：积分系数
        float pidDisKd = 0.0;      // 距离PID：微分系数
        float pidLocalKp = 0.001;  // 位置PID：比例系数
        float pidLocalKi = 0.0002; // 位置PID：积分系数
        float pidLocalKd = 0.0036; // 位置PID：微分系数

        float pidClawXKp = 0.001;  // 机械爪PID：比例系数
        float pidClawXKi = 0.0002; // 机械爪PID：积分系数
        float pidClawXKd = 0.0036; // 机械爪PID：微分系数
        float pidClawYKp = 0.001;  // 机械爪PID：比例系数
        float pidClawYKi = 0.0002; // 机械爪PID：积分系数
        float pidClawYKd = 0.0036; // 机械爪PID：微分系数
    };
    PidConfig pidConfig;   // 自主取餐 + 智能取餐PID参数配置
    float disSearch = 0.6; // AI搜索食物距离：m
    float disPick = 0.4;   // 机械臂抓取食物距离：m
    float disClaw = 0.3;   // 机械爪夹取食物距离：m
    float disOrder = 0.55; // 机器人距离阈值：m（需要+激光雷达盲区20cm）
    bool debug = false;    // 调试使能：AI绘制+显示
    float score = 0.4;     // AI置信度

    Cater() : actionMoveBase("move_base", true)
    {
        pubInitPose = rosNode.advertise<geometry_msgs::PoseWithCovarianceStamped>("/initialpose", 2); // 发布AMCL重定位话题
        subAmclPose = rosNode.subscribe("/amcl_pose", 2, &Cater::getAmclPose, this);                  // 订阅AMCL定位话题
        pubAudio = rosNode.advertise<std_msgs::String>("/audio", 2);                                  // 语音播报话题发布
        location = locationFromXml();                                                                 // 位置信息加载（读取xml文件）
        timeBegin = getSystTime();                                                                    // 任务开始计时
    };

    // 定义析构函数
    ~Cater(){};
    /**
     * @brief 餐桌任务
     *
     */
    void taskTables()
    {
        if (location.tables.size() > 0) // 餐桌任务还未执行完毕
        {
            if (!location.tables[0].ordered) // 未点餐
            {
                publishAudio(pubAudio, "ordering"); // 语音播报：启动自主点餐
                if (simulation)                     // 仿真模式
                    sleep(5);
                else
                {
                    Ordering ordering; // 实例化自主点餐类
                    // setOrdering(ordering); // 设置自主点餐相关配置
                    ordering.pidPose.kP = pidConfig.pidPoseKp; // 方向控制PID
                    ordering.pidPose.kI = pidConfig.pidPoseKi;
                    ordering.pidPose.kD = pidConfig.pidPoseKd;
                    ordering.pidDis.kP = pidConfig.pidDisKp; // 距离控制PID
                    ordering.pidDis.kI = pidConfig.pidDisKi;
                    ordering.pidDis.kD = pidConfig.pidDisKd;
                    ordering.pidDis.ref = disOrder;              // 机器人距离阈值：m（需要+激光雷达盲区20cm）
                    ordering.pidLocal.kP = pidConfig.pidLocalKp; // 距离控制PID
                    ordering.pidLocal.kI = pidConfig.pidLocalKi;
                    ordering.pidLocal.kD = pidConfig.pidLocalKd;
                    ordering.debug = debug;
                    ordering.score = score;

                    bool run = false;
                    while (!run)
                    {
                        run = ordering.getOrders();
                        ros::spinOnce(); // 回调话题通信
                    }
                    sleep(1);
                    location.tables[0].orders = ordering.orders; // 获取点餐信息

                    // 创建智能结算订单
                    if (ordering.orders.size() > 0)
                    {
                        Order order;
                        order.tableId = tableId;
                        order.food = ordering.orders;
                        ordersPaying.push_back(order);
                    }
                }

                location.tables[0].ordered = true;
                switch (location.tables[0].orders.size())
                {
                case 0:                                  // 没有订单
                    publishAudio(pubAudio, "orderNone"); // 语音播报：没有订单
                    break;
                case 1:                                 // 1个食物
                    publishAudio(pubAudio, "orderOne"); // 语音播报：1个食物
                    break;
                case 2:                                // 2个食物
                    publishAudio(pubAudio, "ordeTwo"); // 语音播报：2个食物
                    break;
                case 3:                                   // 3个食物
                    publishAudio(pubAudio, "orderThree"); // 语音播报：3个食物
                    break;
                }
            }
            else if (location.tables[0].putting) // 送餐（放置食物）
            {
                Picking picking;
                setPicking(picking);
                picking.placeFood = location.tables[0].foodPlace; // 食物放置在餐桌上的位置设置
                publishAudio(pubAudio, "putdown");                // 语音播报：放置食物
                if (simulation)                                   // 仿真模式
                    sleep(5);
                else
                {
                    while (!picking.putdownSomething()) // 启动[智能取餐]，等待放置食物
                        ros::spinOnce();                // 回调话题通信
                }
                publishAudio(pubAudio, "putOk"); // 语音播报：放置食物
                location.tables[0].putting = false;
                location.tables[0].foodPlace++;
            }
            else if (location.tables[0].orders.size() > 0) // 继续取餐
            {
                sleep(4);                                        // 等待上一个语音播报结束
                publishAudio(pubAudio, "gotoServing");           // 语音播报：前往取餐台
                naviToWorkStation(StationNavi::STATION_SERVING); // 自主导航至取餐台
            }
            else if (location.tables.size() <= 1) // 送餐结束：导航至收银台
            {
                sleep(4);                                        // 等待上一个语音播报结束
                location.tables.clear();                         // 删除餐桌任务
                naviToWorkStation(StationNavi::STATION_COUNTER); // 导航至收银台
                publishAudio(pubAudio, "deliveryOver");          // 语音播报：前往收银台
            }
            else // 下一个餐桌
            {
                location.tables.erase(location.tables.begin()); // 更新餐桌信息（删除已完成任务餐桌）
                naviToWorkStation(StationNavi::STATION_TABLE);  // 自主导航至下一个餐桌
                tableId += 1;                                   // 餐桌编号+1
            }
        }
    }

    /**
     * @brief 取餐台任务
     *
     */
    void taskServing()
    {
        if (location.tables[0].orders.size() <= 0) // 订单异常
            return;

        Picking picking;
        setPicking(picking);
        if (location.tables[0].orders[0] == LABEL_AI_COLA)        // 抓取可乐
            publishAudio(pubAudio, "pickCola");                   // 语音播报：取餐
        else if (location.tables[0].orders[0] == LABEL_AI_COFFEE) // 抓取咖啡
            publishAudio(pubAudio, "pickCoffee");                 // 语音播报：取餐
        else if (location.tables[0].orders[0] == LABEL_AI_BURGER) // 抓取汉堡包
            publishAudio(pubAudio, "pickBurger");                 // 语音播报：取餐
        else if (location.tables[0].orders[0] == LABEL_AI_CAKE)   // 抓取瑞士卷（蛋糕）
            publishAudio(pubAudio, "pickCake");                   // 语音播报：取餐
        else if (location.tables[0].orders[0] == LABEL_AI_CHEESE) // 抓取奶酪
            publishAudio(pubAudio, "pickCheese");                 // 语音播报：取餐
        else if (location.tables[0].orders[0] == LABEL_AI_SUNDAE) // 抓取圣代（蛋糕）
            publishAudio(pubAudio, "pickSundae");                 // 语音播报：取餐

        if (simulation) // 仿真模式
        {
            picking.findFood = true;
            sleep(5);
        }
        else
        {
            while (!picking.pickupSomething(location.tables[0].orders[0])) // 启动[智能取餐]，抓取食物，等待抓取结束
            {
                ros::spinOnce(); // 回调话题通信
            }

            int count = 0;
            while (!checkGoal(location.serving) && count < 5) // 等待机器人重定位
            {
                if ((getSystTime() - counter) > 3000) // 发布重定位话题：3s
                {
                    initialPose(location.serving); // 重定位
                    counter = getSystTime();       // 更新系统时间戳
                    ROS_INFO_STREAM("Relocation is running...");
                    count++;
                }
                ros::spinOnce(); // 回调话题通信
            }
        }
        if (picking.findFood) // 食物挑选成功
        {
            publishAudio(pubAudio, "pickOk");  // 语音播报：取餐成功!
            location.tables[0].putting = true; // 等待放置食物
        }
        else
        {
            publishAudio(pubAudio, "pickFailed"); // 语音播报：取餐失败!
            location.tables[0].putting = false;
        }
        naviToWorkStation(StationNavi::STATION_TABLE);                      // 自主导航：返回餐桌
        location.tables[0].orders.erase(location.tables[0].orders.begin()); // 更新订单信息（删除已完成订单）
    }

    /**
     * @brief 收银台任务
     *
     */
    void taskCounter()
    {
    }

    /**
     * @brief 自主导航至任务点
     *
     */
    void naviToWorkStation(StationNavi stationNext)
    {
        if (!actionMoveBase.waitForServer(ros::Duration(60))) // 等待60s连接movebase服务器
        {
            ROS_ERROR_STREAM("Can't connected to movebase server!!!");
            publishAudio(pubAudio, "errMovebase"); // 语音播报：movebase服务器连接失败!
        }

        switch (stationNext)
        {
        case StationNavi::STATION_TABLE: // 餐桌
            if (location.tables.size() > 0)
            {
                location.tables[0].position.target_pose.header.stamp = ros::Time::now(); // 获取时间戳
                actionMoveBase.sendGoal(location.tables[0].position);                    // 导航至下一个餐桌位置
                navi.station = StationNavi::STATION_TABLE;                               // 导航目标点类型
            }
            break;

        case StationNavi::STATION_SERVING: // 取餐台

            location.serving.target_pose.header.stamp = ros::Time::now(); // 获取时间戳
            actionMoveBase.sendGoal(location.serving);                    // 导航至收银台
            navi.station = StationNavi::STATION_SERVING;                  // 导航目标点类型
            break;

        case StationNavi::STATION_COUNTER: // 收银台

            location.counter.target_pose.header.stamp = ros::Time::now(); // 获取时间戳
            actionMoveBase.sendGoal(location.counter);                    // 导航至收银台
            navi.station = StationNavi::STATION_COUNTER;                  // 导航目标点类型
            break;
        }
        navi.arriving = true;  // 等到导航到达目标点标志
        navi.overtime = false; // 导航超时查验标志
        sleep(1);              // 延时等待Movebase服务器状态刷新
    }

    /**
     * @brief 智能餐饮场景-状态机处理
     *
     */
    void caterHandle()
    {
        switch (caterStep)
        {
        case CaterStep::CATER_STEP_START:   // 任务开始
            publishAudio(pubAudio, "none"); // 语音播报：
            sleep(1);
            publishAudio(pubAudio, "caterStart"); // 语音播报：智能餐饮任务开始
            caterStep = CaterStep::CATER_STEP_INIT;
            counter = getSystTime(); // 更新系统时间戳
            ROS_INFO_STREAM("Catering is starting!!!");
            break;

        case CaterStep::CATER_STEP_INIT:                    // 机器人重定位
            if (!checkGoal(location.origin) && !simulation) // 等待机器人重定位
            {
                if ((getSystTime() - counter) > 5000) // 发布重定位话题：2s
                {
                    initialPose(location.origin);
                    counter = getSystTime(); // 更新系统时间戳
                    ROS_INFO_STREAM("Relocation is running...");
                }
                sleep(5); // 等待AMCL等节点启动
            }
            else // AMCL重定位成功!!
            {
                publishAudio(pubAudio, "initPose"); // 语音播报：机器人重定位成功!
                caterStep = CaterStep::CATER_STEP_DELIVERY;
                ROS_INFO_STREAM("Relocation success!!!");

                if (location.tables.size() > 0) // 餐桌任务
                {
                    naviToWorkStation(StationNavi::STATION_TABLE); // 自主导航至下一个餐桌
                }
            }
            break;

        case CaterStep::CATER_STEP_DELIVERY: // 送餐到桌
            if (navi.arriving)
            {
                actionMoveBase.waitForResult();                                               // 等待Action通信回馈，堵塞中...
                if (actionMoveBase.getState() == actionlib::SimpleClientGoalState::SUCCEEDED) // 成功到达目标点!
                    ROS_INFO_STREAM("Reached the goal!");
                else // 导航失败!!!(可能已发生碰撞)
                {
                    ROS_ERROR_STREAM("Failed to navigation!!!");
                    // publishAudio(pubAudio, "naviTimeout"); // 语音播报：导航超时
                    // if (navi.station == StationNavi::STATION_TABLE)
                    //     naviToWorkStation(StationNavi::STATION_TABLE); // 重新发布导航目标点
                    // else if (navi.station == StationNavi::STATION_SERVING)
                    //     naviToWorkStation(StationNavi::STATION_SERVING); // 重新发布导航目标点
                    // else if (navi.station == StationNavi::STATION_COUNTER)
                    //     naviToWorkStation(StationNavi::STATION_COUNTER); // 重新发布导航目标点
                }

                if (!navi.overtime) // 导航超时查验标志
                {
                    navi.overtime = true;
                    counter = getSystTime();
                }

                switch (navi.station)
                {
                case StationNavi::STATION_TABLE:                       // 餐桌
                    if (checkGoal(location.tables[0].position))        // 等待到达导航点
                        navi.arriving = false;                         // 等待到达目标点标志
                    else if ((getSystTime() - counter) > navi.timeout) // 导航超时:10s
                    {
                        naviToWorkStation(StationNavi::STATION_TABLE); // 重新发布导航目标点
                        publishAudio(pubAudio, "naviTimeout");         // 语音播报：导航超时
                    }
                    break;

                case StationNavi::STATION_SERVING:                     // 取餐台
                    if (checkGoal(location.serving))                   // 等待到达导航点
                        navi.arriving = false;                         // 等待到达目标点标志
                    else if ((getSystTime() - counter) > navi.timeout) // 导航超时:10s
                    {
                        naviToWorkStation(StationNavi::STATION_SERVING); // 重新发布导航目标点
                        publishAudio(pubAudio, "naviTimeout");           // 语音播报：导航超时
                    }
                    break;

                case StationNavi::STATION_COUNTER:   // 收银台
                    if (checkGoal(location.counter)) // 等待到达导航点
                    {
                        navi.arriving = false;                 // 等待到达目标点标志
                        publishAudio(pubAudio, "paying");      // 语音播报：启动智能结算
                        caterStep = CaterStep::CATER_STEP_PAY; // 启动智能结算

                        long time = getSystTime() - timeBegin;
                        ROS_INFO_STREAM("Total time for catering: " + to_string(long(time / 1000)) + "s");
                    }
                    else if ((getSystTime() - counter) > navi.timeout) // 导航超时:10s
                    {
                        naviToWorkStation(StationNavi::STATION_COUNTER); // 重新发布导航目标点
                        publishAudio(pubAudio, "naviTimeout");           // 语音播报：导航超时
                    }
                    break;
                }
            }
            else // Navi到达以下工作站，执行下列任务↓
            {
                switch (navi.station)
                {
                case StationNavi::STATION_TABLE: // 餐桌
                    taskTables();                // 执行餐桌任务
                    break;

                case StationNavi::STATION_SERVING: // 取餐台
                    taskServing();                 // 执行取餐任务
                    break;

                case StationNavi::STATION_COUNTER: // 收银台
                    taskCounter();                 // 执行收银任务
                    break;
                }
            }
            break;
        case CaterStep::CATER_STEP_PAY: // 智能结算
        {
            sysExit();     // 机械臂关闭
            Paying paying; // 实例化自主点餐类
            bool run = false;
            while (1)
            {
                if (!run)
                    run = paying.showOrder(ordersPaying);
                ros::spinOnce(); // 回调话题通信
            }
            break;
        }
        }
    }
};

/**
 * @brief 系统信号回调函数：系统退出
 *
 * @param signum 信号量
 */
void exitSignal(int signum)
{
    Talon talon; // 机械臂控制类
    sleep(2);
    talon.setJointEnable(talon.ArmJoint::ARM_JOINT_ALL, false); // 机械臂失能
    sleep(1);
    talon.setJointEnable(talon.ArmJoint::ARM_JOINT_ALL, false); // 机械臂失能
    ROS_INFO_STREAM("------ sebot_catering closeed!!! ------");
    exit(signum);
}

int main(int argc, char **argv)
{
    int delay = 5;                           // 程序延时启动时间：s
    ros::init(argc, argv, "sebot_catering"); // ROS节点初始化
    Cater cater;                             // 智能餐饮数据类
    signal(SIGINT, exitSignal);              // 程序退出信号

    ros::param::get("/sebot_catering/delayStart", delay);               // 程序延时启动时间：s
    ros::param::get("/sebot_catering/timeoutNavi", cater.navi.timeout); // 导航超时时间：ms
    ros::param::get("/sebot_catering/simulation", cater.simulation);    // STDR仿真模式使能

    // 自主点餐+智能取餐相关配置参数
    ros::param::get("/sebot_catering/pidPoseKp", cater.pidConfig.pidPoseKp); // 方向控制PID
    ros::param::get("/sebot_catering/pidPoseKi", cater.pidConfig.pidPoseKi);
    ros::param::get("/sebot_catering/pidPoseKd", cater.pidConfig.pidPoseKd);
    ros::param::get("/sebot_catering/pidDisKp", cater.pidConfig.pidDisKp); // 距离控制PID
    ros::param::get("/sebot_catering/pidDisKi", cater.pidConfig.pidDisKi);
    ros::param::get("/sebot_catering/pidDisKd", cater.pidConfig.pidDisKd);
    ros::param::get("/sebot_catering/disClaw", cater.disClaw);                 // 机械爪夹取食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_catering/disPick", cater.disPick);                 // 机械臂抓取食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_catering/disSearch", cater.disSearch);             // AI搜索食物距离：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_catering/pidLocalKp", cater.pidConfig.pidLocalKp); // 位置控制PID
    ros::param::get("/sebot_catering/pidLocalKi", cater.pidConfig.pidLocalKi);
    ros::param::get("/sebot_catering/pidLocalKd", cater.pidConfig.pidLocalKd);
    ros::param::get("/sebot_catering/pidClawXKp", cater.pidConfig.pidClawXKp); // 机械爪PID
    ros::param::get("/sebot_catering/pidClawXKi", cater.pidConfig.pidClawXKi);
    ros::param::get("/sebot_catering/pidClawXKd", cater.pidConfig.pidClawXKd);
    ros::param::get("/sebot_catering/pidClawYKp", cater.pidConfig.pidClawYKp); // 机械爪PID
    ros::param::get("/sebot_catering/pidClawYKi", cater.pidConfig.pidClawYKi);
    ros::param::get("/sebot_catering/pidClawYKd", cater.pidConfig.pidClawYKd);

    ros::param::get("/sebot_catering/disOrder", cater.disOrder); // [自主点餐] 机器人距离阈值：m（需要+激光雷达盲区20cm）
    ros::param::get("/sebot_catering/debug", cater.debug);       // 调试使能：AI绘制+显示
    ros::param::get("/sebot_catering/score", cater.score);       // AI检测置信度

    sleep(delay);            // 等待所有任务节点均正常启动
    ros::Rate loop_rate(10); // 设置ROS循环周期: Hz

    while (ros::ok())
    {
        cater.caterHandle();
        ros::spinOnce();   // 回调话题通信
        loop_rate.sleep(); // ROS线程周期控制
    }

    return 0;
}
