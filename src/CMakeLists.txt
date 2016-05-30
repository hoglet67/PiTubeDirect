
#   Part of the Raspberry-Pi Bare Metal Tutorials
#   Copyright (c) 2015, Brian Sidebotham
#   All rights reserved.

#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:

#   1. Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.

#   2. Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.

#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#   POSSIBILITY OF SUCH DAMAGE.

# CMake build environment for the Valvers Raspberry-Pi bare metal tutorials

# CMake 2.8.10 Documentation: http://www.cmake.org/cmake/help/v2.8.10/cmake.html

cmake_minimum_required( VERSION 2.8 )

# Mark the language as C so that CMake doesn't try to test the C++
# cross-compiler's ability to compile a simple program because that will fail
project( tube-client C ASM )

# Add any C compiler flags here. The C flags concerned with processor tuning
# are in the toolchain files toolchain-arm-none-eabi-bcm2835.cmake, etc.
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPROFILING" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffixed-ip" )



# Set the linker flags so that we use our "custom" linker script
set( CMAKE_EXE_LINKER_FLAGS "-Wl,-T,${PROJECT_SOURCE_DIR}/rpi.ld" )

add_executable( tube-client
    armc-cstartup.c
    armc-cstubs.c
    armc-start.S
    tube-client.c
    tube-defs.h
    tube-exception.c
    cache.c
    cache.h
    rpi-gpio.c
    rpi-gpio.h
    rpi-aux.c
    rpi-aux.h
    rpi-interrupts.c
    rpi-interrupts.h
    rpi-mailbox.c
    rpi-mailbox.h
    rpi-mailbox-interface.c
    rpi-mailbox-interface.h
    tube.S
    tube-ula.c
    programs.c
    programs.h
    info.c
    info.h
    performance.c
    performance.h
# 6502 Co Processor using 65tube Implementation
    copro-65tube.h
    copro-65tube.c
    copro-65tubeasm.S
# 6502 Co Processor using 65tube Implementation
    copro-lib6502.c
    copro-lib6502.h
    lib6502.c
    lib6502.h
# Shared
    tuberom_6502.h
    tuberom_6502.c
# 80186 Co Pro
    copro-80186.c
    copro-80186.h
    cpu80186/Client86_v1_01.h
    cpu80186/cpu80186.c
    cpu80186/cpu80186.h
    cpu80186/i386dasm.c
    cpu80186/iop80186.c
    cpu80186/iop80186.h
    cpu80186/mem80186.c
    cpu80186/mem80186.h
# ARM2
    copro-arm2.c
    copro-arm2.h
    tuberom_arm.c
    tuberom_arm.h
    mame/arm.c
    mame/arm.h
# Null co processor
    copro-null.c
    copro-null.h
)

target_link_libraries (tube-client m)

add_custom_command(
    TARGET tube-client POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ./tube-client -O binary ./kernel.img
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Convert the ELF output file to a binary image" )
