#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
'''
@file 		   :sebotSafety.py
@Description   :智能安控
@Date 		   :2023/10/19 17:49:45
@Autor         :HC
@Version       :v1.0
'''
from faceDetection import *
import numpy as np
import keyboard
import time
import pygame
from utils import *
import sys
sys.path.append(
    "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/scripts/")
sys.path.append(
    "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/scripts")


class SebotSafety:
    """
    安控检测类
    """

    def __init__(self):
        self.score = 0.6    # AI目标检测的置信度: 0.0~1.0
        self.face = Detection(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/models/faceDetectionNPU")  # 初始化人脸检测
        self.audioAsk = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/ask.mp3"        # 无人询问音频
        self.audioOpen = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/open.mp3"      # 开启安控音频
        self.audioClose = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/close.mp3"    # 关闭安控音频
        self.audioFun = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/fun.mp3"        # 功能介绍音频
        self.audioGreet = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/greet.mp3"          # 问候音频
        self.audioIntroduce = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/introduce.mp3"  # 引导词
        self.audioInvade = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotSafety/invade.mp3"          # 入侵提示音频
        self.imgBackground = cv2.imread(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/images/sebotSafety.png")      # 读取背景图片

        self.number = 0                     # 初始化人脸检测的数量
        self.timeInterval = 5               # 设置问候间隔时间 timeSearch >= 2 问候时间需要大于等于 2 s
        self.timeSearch = 10                # 设置寻人问候的等待时间 timeSearch >= 4 寻人问候时间需要大于等于 4 s
        self.timeintroduction = 5           # 设置自我介绍时间 timeintroduction >= 5 自我介绍时间需要大于等于 5 s
        self.timeSecurity = 2              # 设置安控触发时间
        self.faceInterval = 2               # 设置人脸更新间隔
        self.greet = False                  # 问候使能
        self.detection = False              # 人脸检测使能(是否检测到人)
        self.time = time.time()             # 已检测/未检测的时间
        self.timeFace = time.time()         # 人脸更新时间
        self.securityEnable = False         # 安控使能
        self.imgA = self.Result()           # 初始化A图像信息
        self.imgB = self.Result()           # 初始化A图像信息
        self.imgC = self.Result()           # 初始化A图像信息

    class Result:
        """
        人脸数据
        """

        def __init__(self):
            self.img = None                     # 储存人脸检测结果
            self.time = 0                       # 储存人脸检测的时间信息
            self.score = 0                      # 储存人脸检测的分数

    def introduction(self):
        """
        机器人问候和介绍
        """
        timeNow = time.time()  # 获取当前时间
        if self.timeInterval < 2 or self.timeSearch < 4 or self.timeintroduction < 5 or self.timeSecurity < 2:
            print("[Error] 时间不能为负数!!!")
            return False
        if self.securityEnable:  # 进入安控模式
            if timeNow - self.time > self.timeSecurity and self.time != 0 and self.detection:
                self.time = time.time()  # 开启检测计时
                self.audio(self.audioInvade)
        else:
            if self.detection:
                if timeNow - self.time > self.timeintroduction + 4 and self.time != 0:  # 计时到自我介绍时间
                    self.time = time.time()  # 开启检测计时
                    self.audio(self.audioFun)
                if self.greet:              # 开启问候使能
                    self.greet = False
                    self.time = time.time()  # 开启检测计时
                    self.audio(self.audioGreet)
            elif not self.detection:
                if timeNow - self.time > self.timeSearch and self.time != 0:  # 计时到寻人问候时间
                    self.time = time.time()  # 开启检测计时
                    self.audio(self.audioAsk)
                if timeNow - self.time > self.timeInterval and self.time != 0:  # 计时到问候时间
                    self.greet = True           # 开启问候使能

    def decide(self):
        """
        判断是否检测到人脸
        """
        if self.number != 0:
            if self.detection == False:
                self.detection = True
                self.time = time.time()  # 开启检测计时

        else:
            if self.detection == True:
                self.detection = False
                self.time = time.time()  # 开启检测计时

    def reversal(self):
        """
        反转安控使能
        """
        if type(self.securityEnable) == bool:
            self.securityEnable = not self.securityEnable  # 反转安控使能
            self.time = time.time()  # 开启检测计时

            if self.securityEnable:
                self.audio(self.audioOpen)  # 安控检测开启音频
            else:
                self.audio(self.audioClose)  # 安控检测关闭音频

    def showFace(self, img):
        """
        显示人脸检测结果
        """

        img, self.number = self.face.drawBox(img, self.score)

        imgBackground = self.imgBackground.copy()   # 复制一份背景图

        timeNow = time.time()  # 获取当前时间
        if timeNow - self.timeFace > self.faceInterval:
            self.timeFace = timeNow
            i = 0
            for result in self.face.result:
                if i >= 4:
                    break
                if result.score > self.score and result.yMin > 0 and result.xMin > 0:
                    coord = [result.xMin, result.yMin,
                             result.xMax, result.yMax]  # 人脸图像的坐标
                    if i == 0:
                        self.imgA.img = cutImageFromDet(
                            img, coord, [320, 240])  # 储存人脸检测的结果图像
                        self.imgA.time = time.strftime(
                            '%H:%M')                   # 储存人脸检测的时间信息
                        # 储存人脸检测的分数
                        self.imgA.score = str(round(result.score, 3))
                    elif i == 1:
                        self.imgB.img = cutImageFromDet(
                            img, coord, [320, 240])  # 储存人脸检测的结果图像
                        self.imgB.time = time.strftime(
                            '%H:%M')                   # 储存人脸检测的时间信息
                        # 储存人脸检测的分数
                        self.imgB.score = str(round(result.score, 3))
                    elif i == 2:
                        self.imgC.img = cutImageFromDet(
                            img, coord, [320, 240])  # 储存人脸检测的结果图像
                        self.imgC.time = time.strftime(
                            '%H:%M')                   # 储存人脸检测的时间信息
                        # 储存人脸检测的分数
                        self.imgC.score = str(round(result.score, 3))
                    i += 1

        i = 0
        for result in self.face.result:
            if i >= 4:
                break
            if result.score > self.score and result.yMin > 0 and result.xMin > 0:
                cv2.putText(imgBackground, f"(Num:{i+1})", (20+420*i, 900),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (200, 200, 200), 2,)
                cv2.putText(imgBackground, f"Score: {str(round(result.score,3))}", (
                    160+420*i, 891), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2,)
                cv2.putText(imgBackground, f"XMin: {round(result.xMin)}", (
                    160+420*i, 931), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2,)
                cv2.putText(imgBackground, f"YMin: {round(result.yMin)}", (
                    160+420*i, 971), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2,)
                cv2.putText(imgBackground, f"XMax: {round(result.xMax)}", (
                    160+420*i, 1011), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2,)
                cv2.putText(imgBackground, f"YMax: {round(result.yMax)}", (
                    160+420*i, 1051), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2,)
            i += 1

        img = cv2.resize(img, (1280, 750))
        imgBackground[90:90+750, 13:13+1280] = img

        if self.imgA.img is not None:
            imgBackground[92:92+240, 1304:1304 +
                          320] = cv2.resize(self.imgA.img, (320, 240))  # 显示人脸检测的结果
            cv2.putText(imgBackground, f"Time: {self.imgA.time}", (
                1640, 150), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 0, 255), 2,)  # 显示检测时间
            cv2.putText(imgBackground, f"Score: {self.imgA.score}", (
                1640, 250), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 255, 0), 2,)  # 显示检测分数

        if self.imgB.img is not None:
            imgBackground[345:345+240, 1304:1304 +
                          320] = cv2.resize(self.imgB.img, (320, 240))  # 显示人脸检测的结果
            cv2.putText(imgBackground, f"Time: {self.imgB.time}", (
                1640, 400), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 0, 255), 2,)  # 显示检测时间
            cv2.putText(imgBackground, f"Score: {self.imgB.score}", (
                1640, 500), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 255, 0), 2,)  # 显示检测分数

        if self.imgC.img is not None:
            imgBackground[599:599+240, 1304:1304 +
                          320] = cv2.resize(self.imgC.img, (320, 240))  # 显示人脸检测的结果
            cv2.putText(imgBackground, f"Time: {self.imgC.time}", (
                1640, 650), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 0, 255), 2,)  # 显示检测时间
            cv2.putText(imgBackground, f"Score: {self.imgC.score}", (
                1640, 750), cv2.FONT_HERSHEY_SIMPLEX, 1.3, (0, 255, 0), 2,)  # 显示检测分数

        cv2.putText(imgBackground, f"Time: {time.strftime('%H:%M')}", (
            1575, 60), cv2.FONT_HERSHEY_SIMPLEX, 1.8, (255, 255, 255), 2,)

        cv2.putText(imgBackground, f"{self.number}", (1825, 945),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2,)

        cv2.putText(imgBackground, f"{self.securityEnable}", (1825, 1015),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0) if self.securityEnable else (0, 0, 255), 2,)

        return imgBackground

    def audio(self, audioPath):
        """
        播放音频
        """
        if audioPath != None:
            if not pygame.mixer.get_init():
                pygame.mixer.init()  # 初始化声音混合器
            try:
                pygame.mixer.music.load(audioPath)  # 加载音频文件
                pygame.mixer.music.play()  # 开始播放
            except:
                print("[Error] 音频播放失败，请检查音频内容及路径")
        else:
            print("[Error] 请输入正确的音频路径")


"""
主函数
"""

if __name__ == "__main__":
    from uart import*
    from joyStick import *

    sebotSafety = SebotSafety()  # 初始化安控检测类
    uart = Uart("/dev/robot")  # 串口初始化
    joy = JoyStick(0, uart)  # 遥控手柄初始化
    # 设置摄像头
    capture = cv2.VideoCapture("/dev/deepCamera")   # 可以设置摄像头
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    # 创建窗口
    cv2.namedWindow("Video", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Video", 1920, 1080)

    # 播放引导词
    sebotSafety.audio(sebotSafety.audioIntroduce)
    keyboard.on_press_key(
        "enter", lambda _: sebotSafety.reversal())  # 回车键按下反转安控使能

    fps = 0  # 初始化帧率
    while True:

        ret, frame = capture.read()      # 读取视频

        # 检查是否成功读取视频帧
        if not ret:
            print("摄像头打开失败！请检查摄像头")
            break
        startTime = time.time()
        sebotSafety.face.render(frame)                     # 开启模型进行目标检测
        sebotSafety.decide()                               # 判断是否检测到人脸
        sebotSafety.introduction()                         # 开启机器人问候和介绍
        frame = sebotSafety.showFace(frame)                # 显示人脸检测结果

        # 使用cv绘制帧率
        cv2.putText(frame, f"FPS: {round(fps, 2)}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2,)

        currentTime = time.time()
        cv2.imshow("Video", frame)

        elapsedTime = currentTime - startTime
        fps = 1/elapsedTime
        # print("FPS: ",str(round(elapsedTime*1000,1)),"ms")

        if cv2.waitKey(1) in [27, 99]:  # 按 ESC 或 c 退出程序
            break
        joy.joystickHandle()  # 遥控手柄获取键值,控制机器人速度
    capture.release()
    cv2.destroyAllWindows()
