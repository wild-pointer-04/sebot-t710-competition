#!/usr/bin/env python
# -*- encoding: utf-8 -*-
'''
@file 		   :joyStick.py
@Description   :遥控手柄
@Date 		   :2023/11/11 18:46:21
@Autor         :HC
@Version       :v1.0
'''

import pygame


class JoyStick:
    class Speed:
        def __init__(self):
            self.speedLineX = 0.0  # X轴车速：m/s
            self.speedLineY = 0.0  # Y轴车速：m/s
            self.speedAngle = 0.0  # 旋转速度：rad/s

    def __init__(self, js, uart):
        self.debug = False  # 遥控手柄调试
        self.running = False  # 线程运行标志
        self.cotMore = False  # 连续图像采样使能
        self.cotOnce = False  # 单次图像采样使能
        self.send = False  # 串口发送标志
        self.speedSend = self.Speed()  # 串口发送速度
        self.speedSet = self.Speed()  # 挡位速度
        self.speedSet.speedLineX = 0.3
        self.speedSet.speedAngle = 0.5
        pygame.init()
        self.joyStick = pygame.joystick.Joystick(js)  # 打开遥控手柄句柄
        self.joyStick.init()
        self.numBtn = self.joyStick.get_numbuttons()  # 获得手柄按钮的数量
        self.numAxes = self.joyStick.get_numaxes()  # 获得手柄摇杆的数量
        self.numhat = self.joyStick.get_numhats()  # 获得手柄键帽的数量
        self.staBtn = [False] * self.numBtn  # 按键按下状态真值表
        self.uart = uart  # 通信串口类

    def joystickHandle(self):
        """
        遥控手柄获取键值,控制机器人速度和图像采集
        """
        self.uart.transmitSysHeart()  # 发送心跳信号
        for event_ in pygame.event.get():
            if event_.type == pygame.JOYBUTTONUP or event_.type == pygame.JOYBUTTONDOWN:
                for i in range(self.numBtn):
                    if self.joyStick.get_button(i):  # 按键按下
                        if not self.staBtn[i]:
                            if self.debug:
                                print("button-down[", str(i), "]   ", str(i))

                            if i == 0:  # 后退
                                self.speedSend.speedLineX = -self.speedSet.speedLineX  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = 0.0  # rad/s
                                self.send = True  # 串口发送标志
                            elif i == 1:  # 右转
                                self.speedSend.speedLineX = 0.0  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = -self.speedSet.speedAngle  # rad/s
                                self.send = True  # 串口发送标志
                            elif i == 2:  # 左转
                                self.speedSend.speedLineX = 0.0  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = self.speedSet.speedAngle  # rad/s
                                self.send = True  # 串口发送标志
                            elif i == 3:  # 前进
                                self.speedSend.speedLineX = self.speedSet.speedLineX  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = 0.0  # rad/s
                                self.send = True  # 串口发送标志
                            elif i == 4:  # 旋转高速
                                self.speedSet.speedAngle = 0.9  # rad/s
                                print("High speed for turnning:0.9rad/s")
                            elif i == 5:  # 直线运动高速
                                self.speedSet.speedLineX = 0.5  # m/s
                                print("High speed for move:0.5m/s")
                            elif i == 9:  # 单帧采图
                                self.cotOnce = True  # 单次图像采样使能
                            # elif i == 11:  # 连续采图
                            #         self.cotMore = True  # 连续图像采样使能
                            elif i == 10:  # 停止采图
                                self.cotMore = False
                                self.cotOnce = False
                            else:  # 任意键停止运动
                                self.speedSend.speedLineX = 0.0  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = 0.0  # rad/s
                                self.send = True  # 串口发送标志

                        self.staBtn[i] = True
                    else:
                        if self.staBtn[i]:  # 按键弹起
                            if self.debug:
                                print("button-up[", str(i), "]   ", str(i))

                            if i == 0 or i == 1 or i == 2 or i == 3:  # 停止运动
                                self.speedSend.speedLineX = 0.0  # m/s
                                self.speedSend.speedLineY = 0.0  # m/s
                                self.speedSend.speedAngle = 0.0  # rad/s
                                self.send = True  # 串口发送标志

                        self.staBtn[i] = False

            elif event_.type == pygame.JOYAXISMOTION:
                if self.debug:
                    print(self.joyStick.get_axis(0), " | ", self.joyStick.get_axis(1), " | ", self.joyStick.get_axis(
                        2), " | ", self.joyStick.get_axis(3), " | ", self.joyStick.get_axis(4), " | ", self.joyStick.get_axis(5))

                if self.joyStick.get_axis(2) > 0:  # 旋转低速
                    self.speedSet.speedAngle = 0.5  # rad/s
                    print("Low speed for turnning:0.5rad/s")
                elif self.joyStick.get_axis(5) > 0:  # 直线运动低速
                    self.speedSet.speedLineX = 0.3  # m/s
                    print("Low speed for move:0.3m/s")

            elif event_.type == pygame.JOYHATMOTION:
                for i in range(self.numhat):
                    if self.debug:
                        print("hats[", str(i), "]:   ",
                              self.joyStick.get_hat(i))

                if self.numhat > 0:
                    x, y = self.joyStick.get_hat(0)  # 获取键帽值
                    if x < 0:  # 左平移
                        self.speedSend.speedLineX = 0.0  # m/s
                        self.speedSend.speedLineY = self.speedSet.speedLineX  # m/s
                        self.speedSend.speedAngle = 0.0  # rad/s
                        self.send = True  # 串口发送标志
                    elif x > 0:  # 右平移
                        self.speedSend.speedLineX = 0.0  # m/s
                        self.speedSend.speedLineY = -self.speedSet.speedLineX  # m/s
                        self.speedSend.speedAngle = 0.0  # rad/s
                        self.send = True  # 串口发送标志
                    elif x == 0:  # 停止：按键弹起
                        self.speedSend.speedLineX = 0.0  # m/s
                        self.speedSend.speedLineY = 0.0  # m/s
                        self.speedSend.speedAngle = 0.0  # rad/s
                        self.send = True  # 串口发送标志

            if self.send:
                self.uart.speedControl(
                    self.speedSend.speedLineX, self.speedSend.speedLineY, self.speedSend.speedAngle)  # 通信控制机器人运动
                self.send = False
