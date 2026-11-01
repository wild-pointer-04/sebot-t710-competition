/**
 * @file paying.cpp
 * @author HC (sasu@saishukeji.com)
 * @brief 智能结算场景服务 (机器人核算金额)
 * @version 0.1
 * @date 2024/03/14 09:12:30
 * @copyright  :Copyright (c) 2024
 * @note 具体功能模块:
 */

#include "../include/tools.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

class Order
{
public:
    int tableId;            // 餐桌编号
    vector<string> food;    // 餐桌订单
};

class Paying
{
    private:

    /**
     * @brief 智能结算食物价格
     *
     */
    std::unordered_map<std::string, int> foodPrice; // 食物价格字典

    /**
     * @brief 智能结算订单情况
     *
     */
    enum PayStep
    {
        PAY_STEP_START = 0, // 任务开始
        PAY_STEP_NONE,      // 没有订单
        PAY_STEP_ONE,       // 一个订单
        PAY_STEP_TWO,       // 二个订单
        PAY_STEP_THREE,     // 三个订单
        PAY_STEP_MORE,      // 更多订单
        PAY_STEP_END        // 结束
    };

    PayStep payStep = PayStep::PAY_STEP_START; // 智能结算任务流程
    
    public:
    string pathPkg;     // 功能包路径
    Mat imgB;           // 背景图片
    Mat imgT;           // 餐桌图片
    Mat food;           // 食物图片
    Mat imgMoney;       // 金钱图片
    Mat imgMoneyD;      // 金钱(red)图片
    cv::Mat stitching;  // 图片缝合
    Paying()
    {
        foodPrice[LABEL_AI_COLA]   = 9;                           // 可乐的价格
        foodPrice[LABEL_AI_COFFEE] = 18;                          // 咖啡的价格
        foodPrice[LABEL_AI_BURGER] = 22;                          // 汉堡包的价格
        foodPrice[LABEL_AI_CAKE]   = 5;                           // 瑞士卷的价格
        foodPrice[LABEL_AI_CHEESE] = 7;                           // 奶酪的价格
        foodPrice[LABEL_AI_SUNDAE] = 13;                          // 圣代的价格
        pathPkg = ros::package::getPath("sebot_catering");        // 功能包文件根路径
        imgB = imread(pathPkg + "/res/image/background.png");     // 读取背景图片
        imgT = imread(pathPkg + "/res/image/table.png");          // 读取餐桌图片
        imgMoney = imread(pathPkg + "/res/image/money.png");      // 读取人民币图片
        imgMoneyD = imread(pathPkg + "/res/image/money(red).png");// 读取人民币(红)图片

        if (imgB.empty() || imgT.empty()) 
        {  
            ROS_INFO_STREAM("Could not open or find the base image!");
        }  
        
        cv::resize(imgB, imgB, cv::Size(1920, 1080), 0, 0, cv::INTER_LINEAR); // 缩放背景图片
    };
    
    /**
     * @brief 获取订单信息
     * @param orders 全部订单信息
     */
    void getAll(vector<Order> orders)
    {
        switch (orders.size())
        {
        case 0:                                // 没有订单
            payStep = PayStep::PAY_STEP_NONE;
            break;
        case 1:                                // 1个订单
            payStep = PayStep::PAY_STEP_ONE;
            break;
        case 2:                                // 2个订单
            payStep = PayStep::PAY_STEP_TWO;
            break;
        case 3:                                // 3个订单
            payStep = PayStep::PAY_STEP_THREE;
            break;
        }
        if (orders.size() > 3 )
            payStep = PayStep::PAY_STEP_MORE;  // 更多订单
    }

    /**
     * @brief 一个订单任务
     * @param orders 全部订单信息
     */
    void orderOne(vector<Order> orders,Mat imgTable,int num)
    {
        Point text1(200, 440);      // 书写金额
        Point text2(606, 405);      // 书写金额(红)
        Point text3(586, 340);      // 书写编号
        
        stitching = imgTable(cv::Rect(415, 291, imgMoneyD.cols, imgMoneyD.rows));     // 绘制金钱(红)符号
        imgMoneyD.copyTo(stitching);  
        putText(imgTable, to_string(orders[num-1].tableId), text3, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5);          // 书写编号

        if (orders[num-1].food.size() != 1)
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }

        if (orders[num-1].food[0] == "")
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }

        food = imread(pathPkg + "/res/image/" + orders[num-1].food[0] +".png");     // 读取食物图片 
        stitching = imgTable(cv::Rect(61, 63, food.cols, food.rows));
        food.copyTo(stitching);  
        
        stitching = imgTable(cv::Rect(145, 390, imgMoney.cols, imgMoney.rows));     // 绘制金钱符号
        imgMoney.copyTo(stitching);  

        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]]), text1, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);  // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]]), text2, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5);      // 书写金额(红)
    }

    /**
     * @brief 两个订单任务
     * @param orders 全部订单信息
     */
    void orderTwo(vector<Order> orders,Mat imgTable,int num)
    {
        Point text1(247, 450);         // 左侧食物的价格
        Point text2(595, 450);         // 右侧食物的价格
        Point text3(290, 175);         // 总价格
        Point text4(258, 110);         // 书写编号
        
        stitching = imgTable(cv::Rect(92, 61, imgMoneyD.cols, imgMoneyD.rows));     // 绘制金钱(红)符号
        imgMoneyD.copyTo(stitching);   
        putText(imgTable, to_string(orders[num-1].tableId), text4, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5);      // 书写编号

        if (orders[num-1].food.size() != 2)
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }

        if (orders[num-1].food[0] == "" || orders[num-1].food[1] == "")
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }
        
        food = imread(pathPkg + "/res/image/" + orders[num-1].food[0] +".png");   // 读取食物图片 
        cv::resize(food, food, cv::Size(150, 150), 0, 0, cv::INTER_LINEAR);       // 缩放餐桌图片
        stitching = imgTable(cv::Rect(75, 260, food.cols, food.rows));            // 绘制食物
        food.copyTo(stitching);  
        food = imread(pathPkg + "/res/image/" + orders[num-1].food[1] +".png");   // 读取食物图片 
        cv::resize(food, food, cv::Size(150, 150), 0, 0, cv::INTER_LINEAR);       // 缩放餐桌图片
        stitching = imgTable(cv::Rect(380, 260, food.cols, food.rows));           // 绘制食物
        food.copyTo(stitching); 
        stitching = imgTable(cv::Rect(188, 400, imgMoney.cols, imgMoney.rows));   // 绘制金钱符号
        imgMoney.copyTo(stitching);  
        stitching = imgTable(cv::Rect(536, 400, imgMoney.cols, imgMoney.rows));   // 绘制金钱符号
        imgMoney.copyTo(stitching);  
        
        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]]), text1, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);  // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[1]]), text2, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);      // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]] + foodPrice[orders[num-1].food[1]]) , text3, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5); // 书写金额(红)
    }

    /**
     * @brief 三个订单任务
     * @param orders 全部订单信息
     */
    void orderThree(vector<Order> orders,Mat imgTable,int num)
    {
        Point text1(134, 450);         // 左侧食物的价格
        Point text2(366, 450);         // 中间食物的价格
        Point text3(598, 450);         // 右侧食物的价格
        Point text4(290, 175);         // 总价格
        Point text5(258, 110);         // 书写编号

        stitching = imgTable(cv::Rect(92, 61, imgMoneyD.cols, imgMoneyD.rows));   // 绘制金钱(红)符号
        imgMoneyD.copyTo(stitching);  
        putText(imgTable, to_string(orders[num-1].tableId), text5, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5);      // 书写编号 

        if (orders[num-1].food.size() != 3)
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }

        if (orders[num-1].food[0] == "" || orders[num-1].food[1] == "" || orders[num-1].food[2] == "")
        {
            ROS_ERROR_STREAM("Error : "+ to_string(num) + " num");
            return;
        }
        
        food = imread(pathPkg + "/res/image/" + orders[num-1].food[0] +".png");   // 读取食物图片 
        cv::resize(food, food, cv::Size(120, 120), 0, 0, cv::INTER_LINEAR);       // 缩放餐桌图片
        stitching = imgTable(cv::Rect(76, 274, food.cols, food.rows));            // 绘制食物
        food.copyTo(stitching);  


        food = imread(pathPkg + "/res/image/" + orders[num-1].food[1] +".png");   // 读取食物图片 
        cv::resize(food, food, cv::Size(120, 120), 0, 0, cv::INTER_LINEAR);       // 缩放餐桌图片
        stitching = imgTable(cv::Rect(298, 274, food.cols, food.rows));           // 绘制食物
        food.copyTo(stitching); 

        food = imread(pathPkg + "/res/image/" + orders[num-1].food[2] +".png");   // 读取食物图片 
        cv::resize(food, food, cv::Size(120, 120), 0, 0, cv::INTER_LINEAR);       // 缩放餐桌图片
        stitching = imgTable(cv::Rect(520, 274, food.cols, food.rows));           // 绘制食物
        food.copyTo(stitching); 

        stitching = imgTable(cv::Rect(76, 400, imgMoney.cols, imgMoney.rows));    // 绘制金钱符号
        imgMoney.copyTo(stitching);  
        stitching = imgTable(cv::Rect(300, 400, imgMoney.cols, imgMoney.rows));   // 绘制金钱符号
        imgMoney.copyTo(stitching);  
        stitching = imgTable(cv::Rect(524, 400, imgMoney.cols, imgMoney.rows));   // 绘制金钱符号
        imgMoney.copyTo(stitching);  
        
        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]]), text1, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);  // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[1]]), text2, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);    // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[2]]), text3, FONT_HERSHEY_SIMPLEX, 2, Scalar(255, 255, 255), 5);  // 书写金额
        putText(imgTable, to_string(foodPrice[orders[num-1].food[0]] + foodPrice[orders[num-1].food[1]] + foodPrice[orders[num-1].food[2]]) , text4, FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 5);          // 书写金额(红)
        
    }


    /**
     * @brief 一个餐桌
     * @param orders 全部订单信息
     */
    void tableOne(vector<Order> orders)
    {
        cv::resize(imgT, imgT, cv::Size(728, 488), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片
        switch(orders[0].food.size())
        {
        case 0:
            payStep = PayStep::PAY_STEP_NONE;  // 结束
            return;
        case 1:  
            orderOne(orders,imgT,1);
            break;
        case 2:
            orderTwo(orders,imgT,1);
            break;
        case 3:
            orderThree(orders,imgT,1);
            break;
        }
        stitching = imgB(cv::Rect(596, 530, imgT.cols, imgT.rows)); // 将餐桌图片放到背景中
        imgT.copyTo(stitching); 
    }

    /**
     * @brief 两个餐桌
     * @param orders 全部订单信息
     */
    void tableTwo(vector<Order> orders)
    {
        cv::resize(imgT, imgT, cv::Size(728, 488), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片
        for(int n = 0; n < orders.size(); n++)
        {
            cv::Mat imgCopy = imgT.clone();
            switch(orders[n].food.size())
            {
            case 0:
                ROS_ERROR_STREAM("Error : "+ to_string(n+1) + " num");
                break;
            case 1:  
                orderOne(orders,imgCopy,n+1);
                break;
            case 2:
                orderTwo(orders,imgCopy,n+1);
                break;
            case 3:
                orderThree(orders,imgCopy,n+1);
                break;
            }
            stitching = imgB(cv::Rect(167+n*865, 530, imgCopy.cols, imgCopy.rows)); // 将餐桌图片放到背景中
            imgCopy.copyTo(stitching); 
        }
    }

    /**
     * @brief 三个餐桌
     * @param orders 全部订单信息
     */
    void tableThree(vector<Order> orders)
    {
        cv::resize(imgT, imgT, cv::Size(728, 488), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片
        for(int n = 0; n < orders.size(); n++)
        {
            cv::Mat imgCopy = imgT.clone();
            switch(orders[n].food.size())
            {
            case 0:
                ROS_ERROR_STREAM("Error : "+ to_string(n+1) + " num");
                break;
            case 1:  
                orderOne(orders,imgCopy,n+1);
                break;
            case 2:
                orderTwo(orders,imgCopy,n+1);
                break;
            case 3:
                orderThree(orders,imgCopy,n+1);
                break;
            }

            cv::resize(imgCopy, imgCopy, cv::Size(581, 392), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片

            stitching = imgB(cv::Rect(30+n*611, 600, imgCopy.cols, imgCopy.rows)); // 将餐桌图片放到背景中
            imgCopy.copyTo(stitching); 
        }
    }

    /**
     * @brief 更多餐桌
     * @param orders 全部订单信息
     */
    void tableMore(vector<Order> orders)
    {
        cv::resize(imgT, imgT, cv::Size(728, 488), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片

        static int number = 0;
        for(int n = number; n < 3 + number; n++)
        {
            cv::Mat imgCopy = imgT.clone();
            switch(orders[n].food.size())
            {
            case 0:
                ROS_ERROR_STREAM("Error : "+ to_string(n+1) + " num");
                break;
            case 1:  
                orderOne(orders,imgCopy,n+1);
                break;
            case 2:
                orderTwo(orders,imgCopy,n+1);
                break;
            case 3:
                orderThree(orders,imgCopy,n+1);
                break;
            }

            cv::resize(imgCopy, imgCopy, cv::Size(581, 392), 0, 0, cv::INTER_LINEAR); // 缩放餐桌图片
            
            stitching = imgB(cv::Rect(30+(n-number)*611, 600, imgCopy.cols, imgCopy.rows)); // 将餐桌图片放到背景中
            imgCopy.copyTo(stitching); 
        }
        if ((orders.size() - 3) == number)
            number = 0;     // 循环一轮
        else
            number++;       // 向后移动
    }

    /**
     * @brief 显示订单信息
     * @param orders 全部订单信息
     */
    bool showOrder(vector<Order> orders)
    {
        switch(payStep)
        {
        case PayStep::PAY_STEP_START: 
            getAll(orders);         // 获取订单信息
            return false;  

        case PayStep::PAY_STEP_NONE: 
            imgB = imread(pathPkg + "/res/image/None.png"); // 覆盖背景图片
            cv::resize(imgB, imgB, cv::Size(1920, 1080), 0, 0, cv::INTER_LINEAR); // 缩放背景图片
            payStep = PayStep::PAY_STEP_END;  // 结束
            return false;  
        case PayStep::PAY_STEP_ONE:
            payStep = PayStep::PAY_STEP_END;  // 结束
            tableOne(orders);                 // 处理单个餐桌
            return false;
        case PayStep::PAY_STEP_TWO:
            payStep = PayStep::PAY_STEP_END;  // 结束
            tableTwo(orders);                 // 处理两个餐桌
            return false;
        case PayStep::PAY_STEP_THREE:
            payStep = PayStep::PAY_STEP_END;  // 结束
            tableThree(orders);               // 处理三个餐桌
            return false;
        case PayStep::PAY_STEP_MORE:
            tableMore(orders);                // 处理三个餐桌
            imshow("Result", imgB);           // 显示结果 
            waitKey(2000);  
            return false;
        case PayStep::PAY_STEP_END:
            imshow("Result", imgB);           // 显示结果  
            waitKey(0);  
            return true;  
        }
    }
};