#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
"""
@file          :asr.py
@Description   :在线语音识别(ASR)
@Date          :2023/10/07 11:03:33
@Autor         :HC
@Version       :v1.0
"""

import os
import pyaudio
import wave
import keyboard
from urllib.request import urlopen
from urllib.request import Request
from urllib.error import URLError
from urllib.parse import urlencode
import json
import urllib.error
import base64
from utils import *

BAIDU_API_KEY = os.environ.get("BAIDU_API_KEY", "")
BAIDU_SECRET_KEY = os.environ.get("BAIDU_SECRET_KEY", "")


class AudioRecorder:
    """
    语音录制类
    """

    def __init__(self, savePath):
        if savePath == None:
            self.savePath = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/record/temp.wav"  # 录音文件存放地址
        else:
            self.savePath = savePath
        self.sampleRate = 16000  # 采样率,语音信号的频率范围通常更窄（低于8000Hz）
        self.channels = 1  # 音频声道数量
        self.width = 2  # 音频量化位数: 16位
        self.driveIndex = 1  # 麦克驱动号
        self.chunk = 1024  # 缓存中存放帧数
        self.recording = False  # 正在录音标志
        self.frames = []  # 录音数据列表
        self.finish = False  # 录音结束标志
        with Shield():  # 抑制内容显示
            self.audio = pyaudio.PyAudio()  # 创建音频类实例
        self.stream = None  # 创建音频流对象
        keyboard.on_press_key(
            "space", lambda _: self.startRecord())  # 空格键按下开始录音
        keyboard.on_release_key(
            "space", lambda _: self.stopRecord())  # 释放空格键停止录音
        print("[Info] 按键录制音频程序启动!")

    def AudioRecording(self, audio, count, time, status):
        """
        音频录制
        """
        if self.recording:
            self.frames.append(audio)  # 将音频帧添加到列表中
        return (audio, pyaudio.paContinue)

    def startRecord(self):
        """
        开始录音使能
        """
        if not self.recording:
            print("开始录音...")
            self.frames = []  # 清空音频帧列表
            self.finish = False  # 清录音完成标志
            self.stream = self.audio.open(  # 创建音频流对象
                rate=self.sampleRate,
                format=self.audio.get_format_from_width(self.width),
                channels=self.channels,
                input=True,
                input_device_index=self.driveIndex,
                frames_per_buffer=self.chunk,
                stream_callback=self.AudioRecording)
            self.recording = True

    def stopRecord(self):
        """
        停止录音并保存音频文件
        """
        if self.recording:
            self.recording = False
            self.stream.stop_stream()  # 停止音频流录制
            self.stream.close()

            file = wave.open(self.savePath, 'wb')  # 保存音频文件
            file.setnchannels(self.channels)
            file.setsampwidth(self.audio.get_sample_size(
                self.audio.get_format_from_width(self.width)))
            file.setframerate(self.sampleRate)
            file.writeframes(b''.join(self.frames))
            file.close()
            self.finish = True  # 录音完成!


class AsrOnline:
    """
    在线语音识别
    """

    def __init__(self, audioPath):
        self.apiKey = BAIDU_API_KEY  # 百度语音API密钥
        self.secretKey = BAIDU_SECRET_KEY  # 百度语音安全密钥
        if audioPath == None:
            self.audioPath = "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/record/temp.wav"  # 录音文件存放地址
        else:
            self.audioPath = audioPath
        self.cuid = '123456PYTHON'
        self.scope = 'audio_voice_assistant_get'  # 云端API语音功能类型
        print("[Info] 在线语音识别功能已启动!")

    def speechRecognition(self):
        """
        语音识别(调用本地音频录制文件)
        return: str: 识别结果
        """
        try:
            token = self.fetchToken()  # 获取云端token信息
        except BaseException:
            audioPlayer = AudioPlayer(None)  # 启动音乐播放功能
            audioPlayer.playRadio(
                "/root/workspace/sebot-t710-competition/sebot_ros_kits/src/sebot_speech/res/audio/sound/network.mp3")
            print("网络不佳")
            return ""

        if not token:
            return ""

        audio = []  # 音频流数据
        with open(self.audioPath, 'rb') as file:  # 打开本地录音文件
            audio = file.read()

        if len(audio) == 0:
            print('file %s length read 0 bytes' % self.audioPath)  # 音频文件为空
            return ""

        speech = base64.b64encode(audio)  # 格式转换
        speech = str(speech, 'utf-8')  # 中文格式
        params = {'dev_pid': 1537,  # 选择语言及识别模型, DEV_PID=1537: 普通话
                  # "lm_id" : LM_ID,    #测试自训练平台开启此项
                  'format': self.audioPath[-3:],  # 文件格式：pcm/wav/amr，极速版增加m4a格式
                  'rate': 16000,  # 音频采样率(固定频率)
                  'token': token,
                  'cuid': self.cuid,
                  'channel': 1,  # 音频通道数
                  'speech': speech,
                  'len': len(audio)
                  }
        request = json.dumps(params, sort_keys=False)  # 发送请求数据
        response = Request('http://vop.baidu.com/server_api',
                           request.encode('utf-8'))
        response.add_header('Content-Type', 'application/json')
        try:
            result = urlopen(response)
            resAsr = result.read()
        except URLError as err:
            print('asr http response http code : ' + str(err.code))
            resAsr = err.read()

        resAsr = str(resAsr, 'utf-8')
        resJson = json.loads(resAsr)    # 解析JSON数据
        try:
            resAsr = resJson["result"][0]   # 解析语音识别字符
            print("ASR: ", resAsr)
        except KeyError:
            print("[Error] 获取云端识别数据失败, 未检索到result字段")

        return resAsr

    def fetchToken(self):
        """
        获取云端token信息
        return: False: 账户检验失败, str: token信息
        """
        params = {'grant_type': 'client_credentials',
                  'client_id': self.apiKey,
                  'client_secret': self.secretKey}
        request = urlencode(params).encode('utf-8')
        response = Request('http://aip.baidubce.com/oauth/2.0/token',
                           request)  # 获取云端API信息
        try:
            file = urlopen(response)
            result = file.read()
        except urllib.error.HTTPError as err:
            print('token http response http code : ' + str(err.code))
            result = err.read()
        except URLError as err:
            print('token http response http code : ' + str(err.code))
            result = err.read()

        res = json.loads(result.decode())
        if ('access_token' in res.keys() and 'scope' in res.keys()):
            # self.scope = False 忽略检查
            if self.scope and (not self.scope in res['scope'].split(' ')):
                print('[Error] ASR scope is not correct!')
                return False
            return res['access_token']
        else:
            print(
                '[Error] MAYBE API_KEY or SECRET_KEY not correct: access_token or scope not found in token response!')
            return False


"""
UT: 单元测试
"""
if __name__ == "__main__":

    audioRecorder = AudioRecorder(None)  # 启动按键录音功能
    asrOnlie = AsrOnline(None)  # 实例化在线语音识别功能

    while True:
        if audioRecorder.finish:  # 录音完成
            res = asrOnlie.speechRecognition()  # 启动在线语音识别
            if len(res) > 0:  # 语音识别结果有效
                res = ''
            audioRecorder.finish = False
