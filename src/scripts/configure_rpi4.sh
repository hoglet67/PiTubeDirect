#!/bin/sh

cmake $* -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi-rpi4.cmake ../
