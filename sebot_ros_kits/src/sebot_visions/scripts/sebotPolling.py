#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
'''
@file 		   :sebotPolling.py
@Description   :垃圾巡检
@Date 		   :2023/10/18 10:02:51
@Autor         :HC
@Version       :v1.0
'''
from garbageDetection import *
import numpy as np
import keyboard
import time
import pygame
import sys
sys.path.append(
    "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/scripts/")
sys.path.append(
    "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/scripts")


class SebotPolling:
    """
    垃圾巡检类
    """

    def __init__(self):
        self.score = 0.25                    # AI目标检测的置信度: 0.0~1.0
        self.garbage = GarbageDetection(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/models/garbageDetection")    # 加载模型文件, 需要根据情况修改路径
        self.audioRemind = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPolling/remind.mp3"            # 提醒音频
        self.audioClean = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPolling/clean.mp3"              # 垃圾清理音频
        self.audioIntroduce = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPolling/introduce.mp3"      # 引导词
        self.number = 0                     # 初始化垃圾数量
        self.level = 0                      # 初始化污染等级
        self.time = 0                       # 初始化垃圾存在时间计数器
        self.timeAlert = 6                  # 报警容忍时间：timeAlert >= 6 报警容忍时间需要大于等于 6 s
        self.timeSleep = 3                  # 休眠时间：timeSleep > 3 休眠时间需要大于等于 3 s
        self.timeInterval = 4               # 报警间隔时间：timeInterval > 4 间隔需要大于等于 4 s
        self.timeShowCount = 0              # 初始化垃圾时间计数器
        self.timeShowSleep = 0              # 初始化报警休眠时间计数器
        self.timeGarbage = 0                # 垃圾消失时间
        self.audioEnable = False            # 语音使能
        self.imgBackground = cv2.imread(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/images/sebotPolling.png")      # 读取背景图片
        self.garbageImg = self.GarbageImg()  # 初始化垃圾图像数据

    class GarbageImg:
        """
        垃圾图像数据
        """

        def __init__(self):
            self.firstImg = None                # 垃圾数量第一的照片
            self.secondImg = None               # 垃圾数量第二的照片
            self.thirdlyImg = None              # 垃圾数量第三的照片
            self.fourthlyImg = None             # 垃圾数量第四的照片
            self.calculagraph = time.time()     # 程序计时器
            self.counter = [0, 0, 0, 0]            # 存储垃圾数量

    def calculate(self):
        """
        核算垃圾污染等级和垃圾提醒
        """
        timeNow = time.time()  # 获取当前时间

        if self.timeInterval < 4 or self.timeAlert < 6 or self.timeSleep < 3:
            print("[Error] 时间参数设置不正确!!!")
            return False

        if self.number >= 10:
            self.level = 4          # 重度污染
        elif self.number >= 6:
            self.level = 3          # 中度污染
        elif self.number >= 3:
            self.level = 2          # 轻度污染
        elif self.number >= 1:
            self.level = 1          # 轻微污染

        if self.number != 0:
            self.timeGarbage = 0                # 初始化垃圾消失时间
            if self.time == 0 and self.audioEnable != None:
                self.time = timeNow                 # 获取当前时间
                self.timeShowCount = 0              # 初始化垃圾计数器

        elif self.number == 0 and self.timeGarbage == 0:  # 如果已经垃圾消失，开启垃圾消失时间计数器
            self.timeGarbage = time.time()  # 获取当前时间

        elif timeNow - self.timeGarbage >= 4 and self.audioEnable != None and self.timeGarbage != 0:  # 如果垃圾已经消失5秒则认为已经被清理
            self.level = 0            # 清空垃圾污染等级
            self.time = 0             # 清空垃圾存在时间计数器
            self.timeShowCount = 0    # 清空垃圾计数器
            self.timeGarbage = 0      # 清空垃圾消失时间
            self.audioEnable = False

        elif timeNow - self.time >= self.timeAlert and self.audioEnable == False and self.time != 0:  # 如果当前垃圾存在时间超过报警容忍时间则赋予报警使能
            self.audioEnable = True
            self.time = self.time + self.timeInterval                           # 等待下一个报警间隔时长
        elif timeNow - self.time >= self.timeSleep and self.audioEnable == None:  # 休眠时间结束
            self.audioEnable = False                                               # 重新开启报警功能
            self.timeShowSleep = 0                                                 # 清空休眠计时
            # 重新开启垃圾计时
            self.time = 0

        if timeNow - self.time > self.timeShowCount and self.time != 0 and self.audioEnable == False:
            self.timeShowCount += 1              # 垃圾存在时长计数器

        if timeNow - self.time > self.timeSleep - self.timeShowSleep + 1 and self.time != 0 and self.audioEnable == None:
            self.timeShowSleep = self.timeShowSleep - 1   # 休眠时长计数器

        return True

    def keyControl(self, key):
        """
        键盘控制阈值
        """
        if str(key).__contains__("enter down"):
            self.number = 0                 # 清空当前垃圾计数
            self.time = time.time()         # 清空垃圾数量
            self.level = 0                  # 清空污染等级
            self.timeShowCount = 0          # 清空计时显示
            self.audioEnable = None         # 关闭语音使能
            self.timeShowSleep = self.timeSleep
            self.audio(self.audioClean)     # 播放音频
        elif str(key).__contains__("up down"):
            if self.timeAlert < 15:
                self.timeAlert += 1     # 延长报警所需时间
        elif str(key).__contains__("down down"):
            if self.timeAlert > 6:
                self.timeAlert -= 1     # 缩短报警所需时间

    def showResult(self, img):
        """
        显示模型预测结果
        """
        img, self.number = self.garbage.drawBox(
            img, self.score)            # 将预测的结果绘制到图像上并获取垃圾数量

        # 调整图像大小
        img = cv2.resize(img, (880, 660))
        imgBackground = self.imgBackground.copy()                           # 复制背景图
        imgBackground[150:150+660, 496:496 +
                      880] = img                        # 拼接图像到背景图

        cv2.putText(imgBackground, f"Time: {time.strftime('%H:%M')}", (
            795, 90), cv2.FONT_HERSHEY_SIMPLEX, 1.5, (255, 255, 255), 3,)      # 显示当前时间

        cv2.putText(imgBackground, f"{self.number}", (300, 215), cv2.FONT_HERSHEY_SIMPLEX, 1.8, (
            0, 255, 255), 3,)                        # 显示垃圾数量

        cv2.putText(imgBackground, f"{self.level}", (300, 343), cv2.FONT_HERSHEY_SIMPLEX, 1.8, (
            0, 255, 255), 3,)                         # 显示污染等级

        cv2.putText(imgBackground, f"{str(round(self.timeShowCount))[-2:]}", (
            300, 468), cv2.FONT_HERSHEY_SIMPLEX, 1.8, (0, 255, 255), 3,)  # 显示垃圾存在时间

        cv2.putText(imgBackground, f"{self.timeAlert}", (300, 615),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.8, (0, 255, 255), 3,)                     # 显示报警容忍时间

        cv2.putText(imgBackground, f"{self.timeShowSleep}", (300, 762),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.8, (0, 255, 255), 3,)                 # 显示垃圾休眠时间

        i = 0
        for result in self.garbage.result:   # 如果垃圾分数达到阈值标准则显示垃圾信息
            if i >= 4:
                break
            # 垃圾编号
            cv2.putText(imgBackground, f"(Num:{i+1})", (18+345*i, 880),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (200, 200, 200), 2,)
            cv2.putText(imgBackground, f"Score: {str(round(result.score,3))}", (
                160+345*i, 871), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2,)  # AI识别垃圾分数
            cv2.putText(imgBackground, f"XMin: {round(result.xMin)}", (
                160+345*i, 911), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (222, 222, 0), 2,)         # 方框左上角点X轴坐标
            cv2.putText(imgBackground, f"YMin: {round(result.yMin)}", (
                160+345*i, 951), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (222, 222, 0), 2,)         # 方框左上角点Y轴坐标
            cv2.putText(imgBackground, f"XMax: {round(result.xMax)}", (
                160+345*i, 991), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (222, 222, 0), 2,)         # 方框右下角点X轴坐标
            cv2.putText(imgBackground, f"YMax: {round(result.yMax)}", (
                160+345*i, 1031), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (222, 222, 0), 2,)        # 方框右下角点Y轴坐标
            i += 1

        return imgBackground

    def showGarbageImg(self, img):
        """
        显示污染最严重的四个地方
        """
        for i in range(len(self.garbageImg.counter)):
            if self.number == self.garbageImg.counter[i]:
                break
            if self.number > self.garbageImg.counter[i]:
                if i == 0:  # 将排名向下移动
                    # 将图像和垃圾数量向下移动
                    self.garbageImg.fourthlyImg, self.garbageImg.counter[
                        3] = self.garbageImg.thirdlyImg, self.garbageImg.counter[2]
                    self.garbageImg.thirdlyImg, self.garbageImg.counter[
                        2] = self.garbageImg.secondImg, self.garbageImg.counter[1]
                    self.garbageImg.secondImg, self.garbageImg.counter[
                        1] = self.garbageImg.firstImg, self.garbageImg.counter[0]
                    self.garbageImg.firstImg = cv2.resize(
                        img[150:150+660, 496:496+880], (320, 240))   # 获取新的图像
                    for j in range(3, 0):
                        # 将垃圾数量更新
                        self.garbageImg.counter[j +
                                                1] = self.garbageImg.counter[j]
                elif i == 1:
                    # 将图像和垃圾数量向下移动
                    self.garbageImg.fourthlyImg, self.garbageImg.counter[
                        3] = self.garbageImg.thirdlyImg, self.garbageImg.counter[2]
                    self.garbageImg.thirdlyImg, self.garbageImg.counter[
                        2] = self.garbageImg.secondImg, self.garbageImg.counter[1]
                    self.garbageImg.secondImg = cv2.resize(
                        img[150:150+660, 496:496+880], (320, 240))  # 获取新的图像
                elif i == 2:
                    # 将图像和垃圾数量向下移动
                    self.garbageImg.fourthlyImg, self.garbageImg.counter[
                        3] = self.garbageImg.thirdlyImg, self.garbageImg.counter[2]
                    self.garbageImg.thirdlyImg = cv2.resize(
                        img[150:150+660, 496:496+880], (320, 240))  # 获取新的图像
                elif i == 3:
                    self.garbageImg.fourthlyImg = cv2.resize(
                        img[150:150+660, 496:496+880], (320, 240))  # 获取新的图像
                else:
                    print("[Error] 照片索引出错")
                self.garbageImg.counter[i] = self.number
                break

        if self.garbageImg.firstImg is not None:                        # 如果第一名照片不为空则显示捕获的图像
            img[44:44+240, 1411:1411+320] = self.garbageImg.firstImg
        if self.garbageImg.secondImg is not None:                       # 如果第二名照片不为空则显示捕获的图像
            img[315:315+240, 1411:1411+320] = self.garbageImg.secondImg
        if self.garbageImg.thirdlyImg is not None:                      # 如果第三名照片不为空则显示捕获的图像
            img[570:570+240, 1411:1411+320] = self.garbageImg.thirdlyImg
        if self.garbageImg.fourthlyImg is not None:                     # 如果第四名照片不为空则显示捕获的图像
            img[825:825+240, 1411:1411+320] = self.garbageImg.fourthlyImg

        for i in range(len(self.garbageImg.counter)):               # 显示排行榜每个名次的垃圾总数
            cv2.putText(img, f"Num: {self.garbageImg.counter[i]}", (
                1750, 200+i*263), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (222, 222, 0), 2,)

        return img

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

    sebotPolling = SebotPolling()  # 初始化垃圾巡检类
    uart = Uart("/dev/robot")  # 串口初始化
    joy = JoyStick(0, uart)  # 遥控手柄初始化

    # 设置摄像头
    capture = cv2.VideoCapture("/dev/deepCamera")   # 可以设置摄像头
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    capture.set(cv2.CAP_PROP_FPS, 12)
    # 创建窗口
    cv2.namedWindow("Video", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("Video", 1920, 1080)

    # 播放引导词
    sebotPolling.audio(sebotPolling.audioIntroduce)
    keyboard.hook(lambda key: sebotPolling.keyControl(key)
                  )                  # 开启键盘控制阈值

    fps = 0                 # 初始化FPS
    while True:

        ret, frame = capture.read()      # 读取视频

        # 检查是否成功读取视频帧
        if not ret:
            print("摄像头打开失败！请检查摄像头")
            break
        startTime = time.time()                                             # 程序开启记录时间
        # 开启模型进行目标检测
        sebotPolling.garbage.render(frame)
        frame = sebotPolling.showResult(
            frame)                              # 显示垃圾检测结果
        type = sebotPolling.calculate()                                     # 核算垃圾污染等级和垃圾提醒
        frame = sebotPolling.showGarbageImg(
            frame)                          # 显示污染最严重的四个地方
        if sebotPolling.audioEnable:                                       # 如果提醒语音使能已开启
            sebotPolling.audioEnable = False
            # 播放音频 提醒用户清理垃圾
            sebotPolling.audio(sebotPolling.audioRemind)

        if not type:
            break

        # 使用cv绘制帧率
        cv2.putText(frame, f"FPS: {round(fps, 2)}", (12, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2,)

        currentTime = time.time()           # 程序关闭记录时间
        cv2.imshow("Video", frame)          # 显示图像

        elapsedTime = currentTime - startTime
        fps = 1/elapsedTime
        # print("FPS: ",str(round(elapsedTime*1000,1)),"ms")

        if cv2.waitKey(1) in [27, 99]:  # 按 ESC 或 c 退出程序
            break
        joy.joystickHandle()  # 遥控手柄获取键值,控制机器人速度

    capture.release()
    cv2.destroyAllWindows()
