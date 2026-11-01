#!/usr/bin/env python
# -*- encoding: utf-8 -*-
'''
@file          :depth.py
@Description   :AstraPro相机深度测量
@Date          :2023/10/25 16:21:43
@Autor         :Leo
@Version       :v1.0
'''

from openni import openni2
import numpy as np
import cv2


class Depth:
    """
    基于AstraPro-RGBD相机,采集深度数据流，并计算中心距离
    """

    def __init__(self):
        self.stream = None  # 相机深度图像流
        self.device = None  # OpenNI驱动

        openni2.initialize()  # OpenNI驱动初始化
        self.device = openni2.Device.open_any()  # OpenNI启动深度相机：AstraPro
        print(self.device.get_device_info())  # 相机ID参数
        self.stream = self.device.create_depth_stream()  # 相机深度图像流
        self.stream.start()

    def readFrame(self):
        """
        获取单帧深度数据
        """
        frame = self.stream.read_frame()
        depthFrame = np.array(
            frame.get_buffer_as_triplet()).reshape([480, 640, 2])
        dpt1 = np.asarray(depthFrame[:, :, 0], dtype='float32')
        dpt2 = np.asarray(depthFrame[:, :, 1], dtype='float32')

        dpt2 *= 255

        return dpt1 + dpt2

    def getCenterDepth(self, center, width, height, depthFrame):
        """
        计算中心深度信息
        center: 中心坐标(row,col)
        width: 目标图形宽
        height: 目标图像高
        depthFrame: 深度图像
        """
        if center[0] >= depthFrame.shape[0] or center[1] >= depthFrame.shape[1]:  # 数据溢出
            return 0.0

        # 获取像素RGB值
        depthNum = depthFrame[center]

        # 取目标图像(+1/3,+1/3)像素点深度信息
        row = center[0] + int(height/3)
        if row >= depthFrame.shape[0]:
            row = depthFrame.shape[0]-1
        col = center[1] + int(width/3)
        if col >= depthFrame.shape[1]:
            col = depthFrame.shape[1]-1

        depthNum += depthFrame[row, col]

        # 取目标图像(+1/3,-1/3)像素点深度信息
        col = center[1] - int(width/3)
        if col < 0:
            col = 0
        depthNum += depthFrame[row, col]

        # 取目标图像(-1/3,-1/3)像素点深度信息
        row = center[0] - int(height/3)
        if row < 0:
            row = 0
        depthNum += depthFrame[row, col]

        # 取目标图像(-1/3,+1/3)像素点深度信息
        col = center[1] + int(width/3)
        if col >= depthFrame.shape[1]:
            col = depthFrame.shape[1]-1
        depthNum = depthNum + depthFrame[row, col]

        depthNum /= 5000  # 取深度平均值:m

        #print("pixel", str(depthNum))
        return depthNum


if __name__ == "__main__":

    depth = Depth()  # 深度图像类初始化

    while True:
        depthFrame = depth.readFrame()
        cv2.imshow('depth', depthFrame)

        depth.getCenterDepth((240, 320), 20, 20, depthFrame)
        if cv2.waitKey(1) == 27:  # 按ESC退出程序
            break

    depth.stream.stop()
    depth.device.close()
