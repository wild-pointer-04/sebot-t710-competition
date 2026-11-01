#!/usr/bin/env python
# -*- encoding: utf-8 -*-
'''
@file          :sebot_follow.py
@Description   :
@Date          :2023/10/25 15:27:31
@Autor         :Leo
@Version       :v1.0
'''

from kcf import *
from depth import *
import signal
from geometry_msgs.msg import Twist
import sys
sys.path.append("/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/scripts/")
from uart import *

class PoseCtrl:
    """
    基于PID算法的机器人姿态控制
    """

    def __init__(self):
        self.pidPose = self.PID()  # 机器人姿态PID
        self.pidDis = self.PID()  # 机器人距离PID
        self.reset()

    def reset(self):
        """
        控制器复位
        """
        # 机器人姿态PID参数初始化
        self.pidPose.kp = 0.01       # 比例系数
        self.pidPose.ki = 0.004       # 积分系数
        self.pidPose.kd = 0.036       # 微分系数
        self.pidPose.outMax = 2.0   # 姿态控制上限：角速度rad/s
        self.pidPose.deadline = 50  # 姿态控制死区: pixel
        self.pidPose.preError = 0
        self.pidPose.preDerror = 0
        self.pidPose.out = 0
        self.pidPose.counter = 0

        # 机器人距离PID参数初始化
        self.pidDis.ref = 1.0       # 距离控制期望: m
        self.pidDis.kp = 1.3      # 比例系数
        self.pidDis.ki = 0.7      # 积分系数
        self.pidDis.kd = 0.2       # 微分系数
        self.pidDis.outMax = 0.5    # 距离控制上限：速度m/s
        self.pidDis.deadline = 0.25  # 距离控制死区: m
        self.pidDis.preError = 0
        self.pidDis.preDerror = 0
        self.pidDis.out = 0
        self.pidDis.counter = 0

    def poseControl(self, pid):
        """
        PID控制器核心算法
        """
        error = pid.ref - pid.feedBack  # 误差
        dError = error - pid.preError  # 误差变化速度
        dDerror = dError - pid.preDerror  # 误差变化加速度

        pid.preError = error  # 记录当前误差数据
        pid.preDerror = dError

        if abs(error) <= pid.deadline:  # 死区保护
            pid.counter += 1
        else:
            pid.counter = 0

        if pid.counter < 20:
            pid.out = pid.kp * dError + pid.ki * error + pid.kd * dDerror  # PID环节
        else:
            pid.out = 0

        if pid.out > pid.outMax:
            pid.out = pid.outMax
        elif pid.out < -pid.outMax:
            pid.out = -pid.outMax

    class PID:
        def __init__(self):
            self.ref = 0.0  # 期望
            self.kp = 0.0  # 比例系数
            self.ki = 0.0  # 积分系数
            self.kd = 0.0  # 微分系数
            self.feedBack = 0.0  # 反馈值
            self.preError = 0.0  # 历史误差
            self.preDerror = 0.0  # 历史误差变化速度
            self.deadline = 0.0  # 控制死区
            self.counter = 0  # 计数器
            self.out = 0.0  # 控制器输出量
            self.outMax = 0.0  # 输出上限


if __name__ == '__main__':
    # [1] 初始化串口
    uart = Uart("/dev/robot")
    signal.signal(signal.SIGINT, uart.stop)  # 定义软件退出信号量

    # [1] 深度相机初始化
    capture = cv2.VideoCapture("/dev/deepCamera")   # 设置摄像头
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)  # 设置视频流的分辨率为640x480
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    depth = Depth()  # 深度图像类初始化

    # [2] KCF目标跟踪算法初始化
    track = Tracking()  # 初始化目标跟踪
    cv2.namedWindow('Tracking', cv2.WINDOW_NORMAL)  # 指定窗口名
    cv2.resizeWindow('Tracking', 1280, 960)  # 可调整显示大小
    cv2.setMouseCallback('Tracking', track.mouseCallback)  # 设置鼠标回调 (名称, 回调函数)
    # 如果您使用hog功能, 在您绘制第一个边界框后会有一个短暂的停顿, 这是由于使用Numba加速python程序.
    kcf = KCF(True, True, True)  # 初始化追踪器 (贪婪 , 固定窗口 , 多尺度)

    # [3] 初始化PID姿态控制器
    poseCtrl = PoseCtrl()

    # 循环取流(是否开启)
    preTime = time.time()
    while (capture.isOpened() and uart.serial.is_open):
        uart.transmitSysHeart()  # 串口发送心跳信号

        # 图像处理
        ret, frame = capture.read()  # 相机采集(是否成功,图像)
        if not ret:
            print("摄像头打开失败！请检查摄像头")
            break

        frame = cv2.flip(frame, 1)  # 画面水平翻转
        frame = track.trackHandle(kcf, frame)  # 启动KCF算法并将状态显示

        # 姿态控制
        if track.tracking:  # 目标跟踪中...
            # [1] 姿态控制
            center = int(frame.shape[1]/2) - \
                track.result[0] - int(track.result[2]/2)
            if center > frame.shape[1]/2:
                center = frame.shape[1]/2
            elif center < -frame.shape[1]/2:
                center = -frame.shape[1]/2
            poseCtrl.pidPose.feedBack = float(center)
            poseCtrl.poseControl(poseCtrl.pidPose)  # PID计算

            # [2] 距离控制
            depthFrame = depth.readFrame()
            centerX = track.result[0] + int(track.result[2]/2)
            centerY = track.result[1] + int(track.result[3]/2)
            distance = depth.getCenterDepth(
                (centerY, centerX), 20, 20, depthFrame)  # 计算中心深度信息
            poseCtrl.pidDis.feedBack = distance  # 距离单位:m
            poseCtrl.poseControl(poseCtrl.pidDis)  # PID计算
            poseCtrl.pidDis.out = -poseCtrl.pidDis.out  # 运动方向校正
            if poseCtrl.pidDis.out < 0:  # 限制机器人只能向前运动
                poseCtrl.pidDis.out = 0

            # print("---Center: ", str(center),
            #       " | Angle: {:.2f}".format(poseCtrl.pidPose.out))
            # print("---Distance: {:.2f}".format(distance),
            #       " | Line: {:.2f}".format(poseCtrl.pidDis.out))
            uart.speedControl(poseCtrl.pidDis.out, 0.0,
                              poseCtrl.pidPose.out)  # 机器人速度控制

            # 显示距离信息
            cv2.putText(frame, "Distance: {:.2f}m".format(distance), (frame.shape[1] - 160, 193),
                        cv2.FONT_HERSHEY_TRIPLEX, 0.5, (0, 0, 255), 1, cv2.LINE_AA)
            # cv2.imshow('depth', depthFrame)  # 显示深度图像

        else:  # 未选取目标
            poseCtrl.reset()
            uart.speedControl(0.0, 0.0, 0.0)  # 机器人速度控制

        # 计算帧率
        fps = "FPS: {:.2f}".format(1 / (time.time()-preTime))
        preTime = time.time()
        cv2.putText(frame, fps, (frame.shape[1] - 640, 20),
                    cv2.FONT_HERSHEY_TRIPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)
        cv2.imshow('Tracking', frame)  # 显示RGB图像
        if cv2.waitKey(1) in [27,99]:  # 按 ESC 或 c 退出程序
            break

    uart.stop(0,0)
    capture.release()  # 释放相机
    depth.stream.stop()
    depth.device.close()
    cv2.destroyAllWindows()  # 销毁UI
