#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
'''
@file 		   :aiInteraction.py
@Description   :智能交互
@Date 		   :2023/10/10 09:35:00
@Autor         :HC
@Version       :v1.0
'''

import time
from speech import *
from uart import *
from PIL import Image, ImageDraw, ImageFont
import numpy as np
from datetime import datetime
import os
import cv2
import signal


class AiInteraction:
    """
    智能交互主控类
    """

    def __init__(self):
        self.audioRecorder = AudioRecorder(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/record/temp.wav")    # 启动按键录音功能
        self.audioPlayer = AudioPlayer(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/music")                  # 启动音乐播放功能
        self.asrOnlie = AsrOnline(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/record/temp.wav")             # 实例化在线语音识别功能
        self.ttsOnline = TtsOnline(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/tts/temp.mp3")               # 文字合成语音实例化
        self.wordsMatching = WordsMatching(
            "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/config/interaction.json")  # 字符串检索实例化
        self.soundFolder = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/sound/"   # 广播提示音文件夹
        self.fontBland = 30                                                   # 对话内容显示中文字号
        self.fontCH = ImageFont.truetype("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/utils/QingNingYouYuan.ttf", self.fontBland)  # 中文字体(字体文件,大小)
        self.uart = Uart("/dev/robot")                                        # 选择串口
        self.uart.start()                                                     # 开启串口
        self.control = self.Control(self.uart)                                # 初始化机器人控制类
        self.launch = self.Launch(self.uart)                                  # 初始化功能启动类
        self.running = True                                                   # 程序运行标志
        self.keyboardEnable = False                                           # 键盘开启功能标志
        self.lenChatRow = 36                                                  # 语音对话内容，UI每行显示的字数
        self.chat = ""                                                        # 模拟语音输入
        self.word = ""                                                        # 显示对话内容
        os.system("echo 1 > /proc/sys/vm/drop_caches")                        # 清空缓存
        os.system("gnome-terminal -- killall -r ros rviz master")             # 关闭所有ROS进程

    def decide(self):
        """
        判断交互类型
        """
        if self.audioRecorder.finish or self.keyboardEnable:  # 录音完成或者已开启键盘控制
            self.audioRecorder.finish = False             # 清空录音完成标志
            if self.keyboardEnable:
                # 将键盘控制传入的参数作为语音识别的结果 (会关闭语音识别)
                res = self.chat
                self.chat = ""                            # 清空输入内容
            else:
                res = self.asrOnlie.speechRecognition()   # 启动在线语音识别
            if len(res) > 0:  # 语音识别结果有效
                if res.__contains__("结束"):
                    keyboard.press_and_release("c")             # 模拟键盘输入字母c关闭python程序
                    self.stop(0, 0)                             # 结束智能交互进程
                    return "", ""
                elif res.__contains__("关机"):
                    os.system("gnome-terminal -- poweroff")     # 输入关机指令
                    return "", ""
                elif res.__contains__("停止"):
                    keyboard.press_and_release("c")             # 模拟键盘输入字母c关闭python程序
                    self.audioPlayer.stopMusic()                # 停止播放音乐
                    return "cmd", ["已停止所有程序！", "killall -r ros rviz master", "close.mp3"]
                elif res == "-1":                               # 网络不佳的标志
                    return "", ""
                elif res.__contains__("向") or res.__contains__("自检程序") or res.__contains__("氛围灯") or res.__contains__("复位"):
                    return "ctrl", res
                elif res.__contains__("保存") or res.__contains__("地图"):
                    return "cmd", ["正在为您保存地图，请稍等!", "gnome-terminal -- roslaunch sebot_slam sebot_map_save.launch", "save.mp3"]
                elif res.__contains__("电量"):
                    voltage = self.uart.sensors.voltage     # 获取电压
                    power = self.uart.sensors.power         # 获取电量
                    return "chat", "当前电池的电量为百分之"+str(power)+"电压为"+str(round(voltage, 2)).replace('.0', '')+"伏"
                elif res.__contains__("几点") or res.__contains__("时间"):
                    return "chat", datetime.now().strftime('当前时间是: %H点%M分').replace(': 0', ': ').replace('点0', '点')
                elif res.__contains__("几号") or res.__contains__("日期"):
                    return "chat", datetime.now().strftime('今天是%Y年%m月%d号').replace('年0', '年').replace('月0', '月')
                elif res.__contains__("音乐"):
                    self.audioPlayer.playMusic()        # 开始播放音乐(随机开始播放)
                    return "", ""
                elif res.__contains__("上一首"):
                    self.audioPlayer.previousMusic()  # 播放上一首音乐
                    return "", ""
                elif res.__contains__("下一首"):
                    self.audioPlayer.nextMusic()  # 播放上一首音乐
                    return "", ""
                type, response = self.wordsMatching.wordsMatching(
                    res)              # 字符检索(关键字)
                if type == "chat":
                    return "chat", response     # 返回对话结果
                elif type == "cmd":
                    return "cmd", response      # 返回指令结果
            if not self.keyboardEnable:             # 如果识别结果无效并且未开启键盘控制
                return "0", "unknown.mp3"

        return "", ""

    def execute(self, type, response):
        """
        执行交互
        type: 交互类型
        response: 交互的文本
        """
        if type != "":
            if type == "cmd":
                self.word = response[0]              # 显示文字
                res = self.launch.launch(response)   # 启动功能
                response = res          # 统一输出
            elif type == "ctrl":
                self.word = response                       # 显示文字
                response = self.control.motionControl(response)   # 控制机器人
            elif type == "chat":
                self.word = response
            elif type == "0":
                self.word = "抱歉我没有听清，请您再讲一次"
            print(self.word)                            # 显示语音交互内容
            # 返回需要合成音频的文字或音频文件名称(.mp3)
            return response

    def keyboardControl(self, value):
        """
        键盘控制功能启动
        value: 按下键盘的值
        """

        value = str(value)  # 转化为字符串型
        if value.__contains__("tab down"):  # 如果按键1被按下
            self.chat = ""                                  # 清空按键输入
            self.keyboardEnable = not self.keyboardEnable   # 键盘开启功能标志
            if self.keyboardEnable:
                print("键盘控制已开启")
                aiInteraction.speechControl("turnOn.mp3")   # 播放语音
            else:
                print("键盘控制已关闭")
                aiInteraction.speechControl("turnOff.mp3")  # 播放语音
        if value.__contains__("(1 down"):  # 如果按键1被按下
            self.chat = "启动即时建图程序"
        elif value.__contains__("(2 down"):  # 如果按键2被按下
            self.chat = "启动自主导航程序"
        elif value.__contains__("(3 down"):  # 如果按键3被按下
            self.chat = "启动仿真建图程序"
        elif value.__contains__("(4 down"):  # 如果按键4被按下
            self.chat = "启动仿真导航程序"
        elif value.__contains__("(5 down"):  # 如果按键5被按下
            self.chat = "启动智能安控程序"
        elif value.__contains__("(6 down"):  # 如果按键6被按下
            self.chat = "启动垃圾巡检程序"
        elif value.__contains__("(7 down"):  # 如果按键7被按下
            self.chat = "启动人体姿态程序"
        elif value.__contains__("(8 down"):  # 如果按键8被按下
            self.chat = "启动智能跟随程序"
        elif value.__contains__("(0 down"):  # 如果按键0被按下
            self.chat = "启动自检程序"
        elif value.__contains__("(s down") or value.__contains__("(S down"):  # 如果按键s被按下
            self.chat = "保存地图"
        elif value.__contains__("(k down") or value.__contains__("(K down"):  # 如果按键k被按下
            self.chat = "停止所有程序"
        elif value.__contains__("(a down") or value.__contains__("(A down"):  # 如果按键A被按下
            self.chat = "向左转"
        elif value.__contains__("(d down") or value.__contains__("(D down"):  # 如果按键D被按下
            self.chat = "向右转"
        elif value.__contains__("(w down") or value.__contains__("(W down"):  # 如果按键W被按下
            self.chat = "向前移动"
        elif value.__contains__("(x down") or value.__contains__("(X down"):  # 如果按键X被按下
            self.chat = "向后移动"
        elif value.__contains__("(r down") or value.__contains__("(R down"):  # 如果按键R被按下
            self.chat = "复位下位机"
        elif value.__contains__("(p down") or value.__contains__("(P down"):  # 如果按键P被按下
            self.chat = "关机"
        elif value.__contains__("esc down"):  # 如果按键esc被按下
            self.chat = "结束智能交互"

    def speechControl(self, response):
        """
        控制语音输出
        response: 需要合成的音频文件或音频文件名称
        """
        if response != "" and response is not None:
            if response.__contains__(".mp3"):
                response = self.soundFolder + response  # 获取音频所在路径
                self.audioPlayer.playRadio(response)  # 立即播放语音
            else:
                audioPath = self.ttsOnline.speechTransfer(
                    response)  # 在线语音合成mp3
                if audioPath != False:  # 语音合成成功
                    self.audioPlayer.playRadio(audioPath)  # 立即播放语音
                self.audioPlayer.autoResumeMusic()  # 自动切换音频播放状态

    def showChat(self, img):
        """
        主题UI显示语音对话内容
        img: 显示对话的图片
        """
        if img is not None:
            img = Image.fromarray(img)  # 将数组转换成图像
            draw = ImageDraw.Draw(img)  # 创建绘图对象
            word = self.word
            if len(word) > 35:  # 限制字符长度
                word = word[:35] + "..."
            content = []  # 创建多行显示内容
            while len(word) > self.lenChatRow:
                content.append(word[:self.lenChatRow])
                word = word[self.lenChatRow:]
            content.append(word)

            row = 0  # 显示的行号
            for i in range(len(content)-1):
                pointX = 1920/2 - self.lenChatRow*self.fontBland/2
                draw.text((pointX, 910+row*self.fontBland), content[i], font=self.fontCH,
                        fill=(0, 255, 0))  # 显示日期
                row += 1

            pointX = 1920/2 - len(word)*self.fontBland/2
            draw.text((pointX, 910+row*self.fontBland), word,
                    font=self.fontCH, fill=(0, 255, 0))  # 显示对话内容

            img = np.array(img)  # 将图片转换为数组格式
        return img

    def showErrorCode(self,img):
        """
        显示错误代码
        img: 显示错误代码的图片
        """
        errorCode = self.uart.sensors.errorCode # 接收错误代码
        error = [0]*6
        if img is not None and errorCode > 0:
            for i in range(5,-1,-1):               # 遍历错误代码
                    if  errorCode - 2**i >= 0:
                        errorCode = errorCode - 2**i
                        error[i] = 1
                    else:
                        error[i] = 0
            img = Image.fromarray(img)  # 将数组转换成图像
            draw = ImageDraw.Draw(img)  # 创建绘图对象
            txt = "自检错误代码："
            draw.text((85, 754), txt, font=self.fontCH, fill=(0, 0, 255))  # 显示文本
            for i in range(6):
                if error[i]:  # 打印错误提示
                    if i == 0:
                        txt = "电量计故障"
                    elif i == 1:
                        txt = "陀螺仪故障"
                    elif i == 2:
                        txt = "左前轮故障"
                    elif i == 3:
                        txt = "左后轮故障"
                    elif i == 4:
                        txt = "右前轮故障"
                    elif i == 5:
                        txt = "右后轮故障"

                    if i < 3:
                        draw.text((85, 40*(i+1)+754), txt, font=self.fontCH, fill=(0, 0, 255))  # 显示文本
                    elif i >=3:
                        draw.text((300, 40*(i-2)+754), txt, font=self.fontCH, fill=(0, 0, 255))  # 显示文本
            img = np.array(img)  # 将图片转换为数组格式
        return img

    def stop(self, signum, frame):
        """
        停止语音交互进程
        """
        if self.running:
            self.running = False                                # 关闭语音交互进程
            os.system("echo 1 > /proc/sys/vm/drop_caches")      # 清空缓存
            # 关闭所有ROS进程
            os.system("gnome-terminal -- killall -r ros rviz master")
            self.uart.stop(0, 0)                                # 关闭串口
            self.speechControl("finish.mp3")                    # 播放结束语
            time.sleep(4)                                       # 延时四秒关闭程序
            os.system("gnome-terminal -- killall -9 python3")   # 关闭Python程序

    class Control:
        """
        机器人控制类
        """

        def __init__(self, uart):
            self.uart = uart        # 串口通信

        def motionControl(self, response):
            """
            运动控制
            response: 运动控制指令
            """
            if response.__contains__("向左转"):
                self.uart.turnning(0.4, 0.78539816)  # 以0.4 rad/s的速度向左转45°
                return "di.mp3"
            elif response.__contains__("向右转"):
                self.uart.turnning(0.4, -0.78539816)  # 以0.4 rad/s的速度向右转45°
                return "di.mp3"
            elif response.__contains__("向前移动"):
                self.uart.movement2X(0.4, 0.5)  # 以0.4 m/s的速度向前走0.5 米
                return "di.mp3"
            elif response.__contains__("向后移动"):
                self.uart.movement2X(0.4, -0.5)  # 以0.4 m/s的速度向后走0.5 米
                return "di.mp3"
            elif response.__contains__("启动自检程序"):
                self.uart.robotSelfcheck()  # 启动自检程序
                return "selfcheck.mp3"
            elif response.__contains__("打开"):
                self.uart.rgbEnable(True)       # 打开RGB氛围灯
                return "di.mp3"
            elif response.__contains__("关闭"):
                self.uart.rgbEnable(False)      # 关闭RGB氛围灯
                return "di.mp3"
            elif response.__contains__("复位"): # 复位下位机
                self.uart.systemReset()     # 灯光复位
                self.uart.systemReboot()    # MCU复位
                return "di.mp3"
                

    class Launch:
        """
        功能启动类
        """

        def __init__(self, control):
            self.running = False  # 是否启动ROS功能标志
            self.ctrl = control   # 获取对机器人的控制

        def launch(self, response):
            """
            启动ROS功能
            response: cmd指令
            """

            # 矫正ROS当前状态
            os.system(
                "ps -ef | grep -w ros >> temporary_ros.txt && ps -ef | grep -w python3 >> temporary_python.txt")
            # 通过ROS当前进程数来判断ROS功能状态,os.path.getsize 获取文件的字节数
            # 记录开启程序前相关进程的字符数，大于该字符时视为已启动程序
            if os.path.getsize('temporary_ros.txt') > 350 or os.path.getsize('temporary_python.txt') > 650:
                self.running = True         # 判断应用已开启
            else:
                self.running = False        # 判断应用未开启

            os.system("rm temporary_ros.txt temporary_python.txt")  # 删除临时文件

            if response[0].__contains__("启动"):  # 判断当前指令不是  停止功能 和 结束语音交互
                if self.running:
                    return "当前已开启应用,请您关掉当前应用后重试"
                else:
                    if self.ctrl.running:     # 如果串口正在运行
                        self.ctrl.stop(0, 0)   # 关闭Python串口
                        time.sleep(1)           # 延时一秒
                    os.system(response[1])  # 启动ROS功能
                    self.running = True
                    return response[2]      # 返回音频路径
            else:
                if not self.ctrl.running:   # 如果串口未运行
                    self.ctrl.start(True)       # 重新开启Python串口
                if response[0].__contains__("停止"):
                    self.running = False        # 更新应用状态
                os.system(response[1])  # 关闭当前功能
                return response[2]      # 返回音频路径


if __name__ == "__main__":
    """
    主程序
    """
    aiInteraction = AiInteraction()  # 实例化智能交互主控类
    signal.signal(signal.SIGINT, aiInteraction.stop)  # 定义软件退出信号量
    # 显示智能交互UI
    capture = cv2.VideoCapture("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/image/aiInteraction.mp4")  # 加载智能交互界面
    keyboardImg = cv2.imread("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/image/keyboard.png")        # 加载操作指示图
    aiInteraction.speechControl("aiInteraction.mp3")  # 播放语音
    capture.set(cv2.CAP_PROP_FPS, 60)  # 设置视频的读取速率
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cv2.namedWindow("Video", cv2.WINDOW_NORMAL)  # 创建UI窗口
    cv2.setWindowProperty('Video', cv2.WND_PROP_FULLSCREEN,
                          cv2.WINDOW_FULLSCREEN)  # 将窗口铺满屏幕
    keyboard.on_press(
        lambda key: aiInteraction.keyboardControl(key))  # 创建一个线程监听键盘

    while aiInteraction.running:
        if not aiInteraction.launch.running:  # 如果未开启功能 (开启功能时自动屏蔽智能交互界面)
            if not aiInteraction.keyboardEnable:   # 如果开启了键盘控制使能则打开操作指示图
                ret, img = capture.read()        # 读取视频数据
                if ret:
                    if len(aiInteraction.word) > 0:
                        img = aiInteraction.showChat(img)  # 显示语音回答内容
                    img = aiInteraction.showErrorCode(img) # 显示错误代码
                    cv2.imshow("Video", img)     # 显示语音交互UI界面
                else:
                    capture = cv2.VideoCapture("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/image/aiInteraction.mp4")  # 读取视频
                    cv2.namedWindow("Video", cv2.WINDOW_NORMAL)  # 创建UI窗口
                    cv2.setWindowProperty('Video', cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)  # 将窗口铺满屏幕
            else:
                img = keyboardImg.copy()        # 复制操作指示图
                cv2.imshow("Video", img)     # 显示语音交互UI界面
                cv2.namedWindow("Video", cv2.WINDOW_NORMAL)  # 创建UI窗口
                cv2.setWindowProperty('Video', cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)  # 将窗口铺满屏幕

        else:
            capture.release()       # 释放摄像头
            cv2.destroyAllWindows()  # 释放窗口

        aiInteraction.control.uart.transmitSysHeart()       # 发送心跳信号
        type, response = aiInteraction.decide()             # 判断交互类型
        response = aiInteraction.execute(type, response)    # 执行交互
        aiInteraction.speechControl(response)               # 播放语音

        if cv2.waitKey(1) == 27:  # 按ESC退出程序
            aiInteraction.stop(0, 0)

    aiInteraction.uart.stop(0, 0)  # 关闭串口
    capture.release()       # 释放摄像头
    cv2.destroyAllWindows()  # 释放窗口
