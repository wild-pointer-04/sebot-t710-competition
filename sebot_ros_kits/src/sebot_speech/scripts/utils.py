#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
"""
@file          :utils.py
@Description   :公共类
@Date          :2023/10/07 11:03:33
@Autor         :HC
@Version       :v1.0
"""
import os


class Shield:
    """
    抑制python库的多余输出
    """

    def __init__(self):
        self.nullFds = [os.open(os.devnull, os.O_RDWR) for x in range(2)]
        self.saveFds = (os.dup(1), os.dup(2))

    def __enter__(self):
        os.dup2(self.nullFds[0], 1)
        os.dup2(self.nullFds[1], 2)

    def __exit__(self, *_):
        os.dup2(self.saveFds[0], 1)
        os.dup2(self.saveFds[1], 2)
        os.close(self.nullFds[0])
        os.close(self.nullFds[1])
