#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
'''
@file 		   :sebotPose.py
@Description   :人体姿态检测
@Date 		   :2023/10/20 16:47:40
@Autor         :HC
@Version       :v1.0
'''
from poseEstimation import *
import numpy as np
import keyboard
import time
import pygame


class SebotPose:
    """
    人体姿态检测类
    """

    def __init__(self):
        # AI目标检测的置信度: 0.0~1.0 [0]: 行人检测模型 [1]: 人体关键点检测模型
        self.score = [0.34, 0.45]
        self.pose = KeyPointDetector("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/models/pedestrianDetection",
                                     "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/models/poseEstimation")  # 初始化人体姿态检测
        self.pose.detector.number = 2                                     # 初始化行人检测上限
        self.imgBackground = cv2.imread(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/images/sebotPose.png")      # 读取背景图片
        self.audioIntroduce = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/introduce.mp3"         # 延时拍照声音
        # 开启单人挥手拍照音频路径
        self.audioOpen = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/open.mp3"
        # 关闭单人挥手拍照音频路径
        self.audioClose = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/close.mp3"
        self.audioRetreat = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/retreat.mp3"            # 距离过近音频路径
        self.audioShutter = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/shutter.mp3"            # 快门声音
        # 延时拍照声音
        self.audioDi = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_visions/res/audio/sebotPose/di.mp3"

        # X轴像素值(挥手的幅度, 用来调节相机拍照灵敏度)    该值越低越灵敏 > 0
        self.valueX = 150
        # Y轴像素值(举起手的高度, 用来调节相机拍照灵敏度) 该值越高越灵敏 > 0
        self.valueY = 400
        self.pictureKeyPoints = 7       # 关键点阈值 (大于该值才能进行挥手拍照)
        self.personNum = 0              # 初始化行人个数

        self.time = time.time()         # 获取程序开启的时间
        self.handLChange = 0            # 初始化左手变化值
        self.handRChange = 0            # 初始化右手变化值
        self.picture = self.Picture()   # 初始化拍照数据
        self.keyPoints = self.KeyPoints()  # 初始化假人关键点坐标
        self.pictureHand = True     # 挥手拍照拍照使能
        self.pictureEnter = False   # 回车拍照使能

    class Picture:
        """
        拍照数据
        """

        def __init__(self):
            self.timeCamera = time.time()  # 初始化拍照时间：s(float)
            self.pictureEnable = False     # 拍照功能使能
            self.pictureA = None    # 拍照存储的第一个照片
            self.pictureB = None    # 拍照存储的第二个照片
            self.pictureC = None    # 拍照存储的第三个照片
            self.indexes = 0        # 拍照图片索引
            self.imgOld = None      # 原始图像

    class KeyPoints:
        """
        假人关键点坐标
        """

        def __init__(self):
            # 假人0~17个关键点的坐标
            self.axis = [
                (1633, 395),                # 鼻子
                (1666, 283),                # 左眼
                (1590, 283),                 # 右眼
                (1695, 347),                 # 左耳
                (1520, 347),                 # 右耳
                (1720, 423),                 # 左肩
                (1559, 423),                 # 右肩
                (1779, 513),                 # 左肘
                (1464, 513),                 # 右肘
                (1712, 674),                 # 左手
                (1515, 674),                 # 右手
                (1702, 634),                 # 左胯
                (1546, 634),                 # 右胯
                (1727, 788),                 # 左膝
                (1504, 788),                 # 右膝
                (1707, 980),                 # 左脚
                (1510, 980)]                 # 右脚
            self.colors = [                  # 设置关键点的颜色
                [255, 0, 0],
                [255, 85, 0],
                [255, 170, 0],
                [255, 255, 0],
                [170, 255, 0],
                [85, 255, 0],
                [0, 255, 0],
                [0, 255, 85],
                [0, 255, 170],
                [0, 255, 255],
                [0, 170, 255],
                [0, 85, 255],
                [0, 0, 255],
                [85, 0, 255],
                [170, 0, 255],
                [255, 0, 255],
                [255, 0, 170],
                [255, 0, 85]]

    def camera(self, img):
        """
        挥手相机拍照
        """
        timeNow = time.time()  # 获取当前时间

        if self.pose.keyPoints.handL is not None or self.pose.keyPoints.handR is not None:
            # 如果左右手挥手幅度大于像素点阈值
            if ((abs(self.pose.keyPoints.handL[0] - self.handLChange) > self.valueX and self.handLChange != 0) or (abs(self.pose.keyPoints.handR[0] - self.handRChange) > self.valueX and self.handRChange != 0)) and self.pose.keyPoints.num > self.pictureKeyPoints and self.personNum == 1:
                self.picture.pictureEnable = True
                self.picture.timeCamera = time.time()     # 获取程序开启的时间

            # 如果手部关键点坐标高于像素点阈值
            if self.pose.keyPoints.handL[1] < self.valueY and not self.picture.pictureEnable and self.pictureHand:
                # 储存左手X轴像素值
                self.handLChange = self.pose.keyPoints.handL[0]
            else:
                self.handLChange = 0                                 # 清空左手储存的像素值

            if self.pose.keyPoints.handR[1] < self.valueY and not self.picture.pictureEnable and self.pictureHand:
                # 储存右手X轴像素值
                self.handRChange = self.pose.keyPoints.handR[0]
            else:
                self.handRChange = 0                                 # 清空右手储存的像素值

            if self.picture.pictureEnable or self.pictureEnter:

                if int(timeNow - self.picture.timeCamera) > 3:
                    self.audio(self.audioShutter)           # 播放快门声
                    self.picture.pictureEnable = False
                    self.pictureEnter = False
                    self.handLChange = 0
                    self.handRChange = 0
                    if self.picture.indexes == 0:               # 拍照并把索引下移
                        self.picture.indexes += 1
                        self.picture.pictureA = cv2.resize(
                            img[248:248+480, 742:742+640], (320, 240))
                    elif self.picture.indexes == 1:
                        self.picture.indexes += 1
                        self.picture.pictureB = cv2.resize(
                            img[248:248+480, 742:742+640], (320, 240))
                    elif self.picture.indexes == 2:
                        self.picture.indexes = 0                # 复位索引
                        self.picture.pictureC = cv2.resize(
                            img[248:248+480, 742:742+640], (320, 240))

                elif int(timeNow - self.picture.timeCamera) == 3:
                    img = np.ones((1080, 1920, 3), dtype=np.uint8)
                    img *= 255
                else:
                    if 0.2 > timeNow - self.picture.timeCamera > 0 or 1 < timeNow - self.picture.timeCamera < 1.2 or 2 < timeNow - self.picture.timeCamera < 2.2:
                        self.audio(self.audioDi)           # 播放延迟滴滴声
                    cv2.putText(img, str(3-(int(timeNow - self.picture.timeCamera))),
                                (680, 900), cv2.FONT_HERSHEY_SIMPLEX, 30, (0, 255, 0) if 3-(int(timeNow - self.picture.timeCamera)) == 3 else (0, 255, 255) if 3-(int(timeNow - self.picture.timeCamera)) == 2 else (0, 0, 255), 20)
        else:
            self.handLChange = 0    # 清空左手储存的像素值
            self.handRChange = 0    # 清空右手储存的像素值

        if timeNow - self.time > 8:  # 每过八秒提示用户状态
            self.time = timeNow                           # 获取当前时间
            if self.personNum == 1:
                if self.pose.keyPoints.num < self.pictureKeyPoints:
                    self.audio(self.audioRetreat)                 # 播放距离提示音
                elif self.pose.keyPoints.num >= self.pictureKeyPoints and self.pictureHand == False:
                    self.audio(self.audioOpen)          # 开启挥手拍照功能
            elif self.personNum > 1:
                self.audio(self.audioClose)         # 关闭挥手拍照功能

        if self.personNum == 1:                 # 根据当前判断用户数量和关键点的状态
            if self.pose.keyPoints.num < self.pictureKeyPoints:
                self.pictureHand = False
            elif self.pose.keyPoints.num >= self.pictureKeyPoints and self.pictureHand == False:
                self.pictureHand = True
        elif self.personNum > 1:
            self.pictureHand = False
            self.picture.pictureEnable = False  # 关闭拍照使能

        return img

    def keyboard(self, key):
        """
        获取键盘键值
        """
        if str(key).__contains__("enter down"):
            self.pictureEnter = True
            self.picture.timeCamera = time.time()     # 获取程序开启的时间

    def render(self, img):
        """
        模型预测并绘制图像
        """
        self.imgOld = img                                        # 备份原始图像
        res = self.pose.detector.render(img)                     # 开启模型进行人体检测
        res = self.pose.detector.drawBox(res, self.score[0])     # 绘制检测到的人体矩形框
        self.personNum = res["boxesNum"]                         # 获取行人数量
        res = self.pose.render(img, res)  # 开启模型进行人体关键点检测

        img = self.pose.drawBox(img, res, self.score[1])  # 绘制人体关键点和肢体曲线

        return img

    def showInterface(self, img):
        """
        显示AI检测结果
        """

        self.imgOld = cv2.resize(
            self.imgOld, (640, 480))                # 调整图像大小
        # 调整图像大小
        img = cv2.resize(img, (640, 480))
        imgBackground = self.imgBackground.copy()                      # 复制一张背景图
        imgBackground[248:248+480, 54:54+640] = self.imgOld           # 显示原始图像
        imgBackground[248:248+480, 742:742 +
                      640] = img                 # 显示AI绘制图像
        cv2.putText(imgBackground, f"{self.score[0]}", (
            620, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (249, 218, 63), 4,)       # 显示行人检测模型阈值
        cv2.putText(imgBackground, f"{self.score[1]}", (
            1305, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (249, 218, 63), 4,)      # 显示人体关键点模型阈值
        cv2.putText(imgBackground, f"{self.pose.keyPoints.num}", (1750, 122),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.3, (104, 186, 211), 4,)      # 显示人体关键点数量

        return imgBackground

    def showKeyPoints(self, img):
        """
        显示人体关键点分数
        """
        for i in range(17):
            if self.pose.keyPoints.score[i] > self.score[1]:  # 如果关键点的分数大于阈值
                cv2.putText(img, f"{self.pose.keyPoints.score[i]}", self.keyPoints.axis[i],cv2.FONT_HERSHEY_SIMPLEX, 1, self.keyPoints.colors[i], 2,)      # 显示人体关键点数量

        return img

    def showPicture(self, img):
        """
        显示拍好的照片
        """
        if self.picture.pictureA is not None:               # 如果图像不为空
            img[778:778+240, 52:52+320] = self.picture.pictureA      # 将拍好的照片放到背景上
        if self.picture.pictureB is not None:               # 如果图像不为空
            img[778:778+240, 555:555+320] = self.picture.pictureB
        if self.picture.pictureC is not None:               # 如果图像不为空
            img[778:778+240, 1059:1059+320] = self.picture.pictureC

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
    sebotPose = SebotPose()  # 初始化人体姿态检测类
    # 设置摄像头
    capture = cv2.VideoCapture("/dev/deepCamera")   # 可以设置摄像头
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    # 创建窗口
    cv2.namedWindow("Video", cv2.WINDOW_NORMAL)
    # 设置窗口大小
    cv2.resizeWindow("Video", 1920, 1080)

    # 播放引导词
    sebotPose.audio(sebotPose.audioIntroduce)
    keyboard.hook(lambda key: sebotPose.keyboard(key))# 开启键盘控制获取键值

    fps = 0
    while True:
        ret, frame = capture.read()      # 读取视频

        # 检查是否成功读取视频帧
        if not ret:
            print("摄像头打开失败！请检查摄像头")
            break
        startTime = time.time()

        frame = sebotPose.render(frame)                           # 开启模型预测并绘制图像
        frame = sebotPose.showInterface(frame)                    # 显示AI检测结果
        frame = sebotPose.showPicture(frame)                      # 显示拍好的照片
        frame = sebotPose.showKeyPoints(frame)                    # 显示人体关键点分数
        frame = sebotPose.camera(frame)                           # 挥手相机拍照

        # 使用cv绘制帧率
        cv2.putText(frame, f"FPS: {round(fps, 2)}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2,)

        currentTime = time.time()
        cv2.imshow("Video", frame)

        elapsedTime = currentTime - startTime
        fps = 1/elapsedTime
        # print("FPS: ",str(round(elapsedTime*1000,1)),"ms")
        if cv2.waitKey(1) in [27, 99]:  # 按 ESC 或 c 退出程序
            break
    capture.release()
    cv2.destroyAllWindows()
