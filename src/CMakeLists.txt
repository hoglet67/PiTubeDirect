
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

file( GLOB core_files
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
    tubevc.c
    tubevc.h
)

# 6502 Co Processor using 65tube Implementation
file( GLOB copro_65tube_files
    copro-65tube.h
    copro-65tube.c
    copro-65tubeasm.S
)

# 6502 Co Processor using 65tube Implementation
file( GLOB copro_lib6502_files
    copro-lib6502.c
    copro-lib6502.h
    lib6502.c
    lib6502.h
)

# Shared
file( GLOB shared_6502_files
    tuberom_6502.h
    tuberom_6502.c
)

# 80186 Co Pro
file( GLOB copro_80186_files
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
)

# ARM2
file( GLOB copro_arm2_files
    copro-arm2.c
    copro-arm2.h
    tuberom_arm.c
    tuberom_arm.h
    mame/arm.c
    mame/arm.h
)

# 32016
file( GLOB copro_32016_files
    copro-32016.c
    copro-32016.h
    NS32016/32016.c
    NS32016/32016.h
    NS32016/Decode.c
    NS32016/Decode.h
    NS32016/defs.h
    NS32016/mem32016.c
    NS32016/mem32016.h
    NS32016/NDis.h
    NS32016/NSDis.c
    NS32016/pandora/PandoraV0_61.h
    NS32016/pandora/PandoraV2_00.h
    NS32016/Profile.c
    NS32016/Profile.h
    NS32016/Trap.c
    NS32016/Trap.h
)

# Z80
file( GLOB copro_z80_files
    copro-z80.h
    copro-z80.c
    yaze/simz80.h
    yaze/simz80.c
    yaze/mem_mmu.h
)

# Null co processor
file( GLOB copro_null_files
    copro-null.c
    copro-null.h
)

if( ${MINIMAL_BUILD} )

  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMINIMAL_BUILD=1" )

    add_executable( tube-client
        ${core_files}
        ${copro_65tube_files}
        ${shared_6502_files}
    )

else()

    add_executable( tube-client
        ${core_files}
        ${copro_65tube_files}
        ${copro_lib6502_files}
        ${shared_6502_files}
        ${copro_80186_files}
        ${copro_arm2_files}
        ${copro_32016_files}
        ${copro_z80_files}
        ${copro_null_files}
    )

endif()
    
target_link_libraries (tube-client m)

add_custom_command(
    TARGET tube-client POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ./tube-client -O binary ${KERNEL_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Convert the ELF output file to a binary image" )
