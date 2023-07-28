#!/bin/sh

cmake $* -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi-rpi3.cmake ../
