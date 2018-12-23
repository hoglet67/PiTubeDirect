
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

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )

# Add any C compiler flags here. The C flags concerned with processor tuning
# are in the toolchain files toolchain-arm-none-eabi-bcm2835.cmake, etc.
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostartfiles" )
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer " )
#set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffixed-ip" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-delete-null-pointer-checks -fdata-sections -ffunction-sections ")

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --specs=nano.specs --specs=nosys.specs -u _printf_float" )

# Set the linker flags so that we use our "custom" linker script

#set( CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,--print-gc-sections -Wl,-unresolved-symbols=ignore-in-object-files -Wl,-T,${PROJECT_SOURCE_DIR}/rpi.ld" )

set( CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections -Wl,-T,${PROJECT_SOURCE_DIR}/rpi.ld" )


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
    logging.c
    logging.h
    utils.c
    utils.h
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

# Shared 6502 files
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

# 6809 (version based on Neal Crook emulator)
file( GLOB copro_6809nc_files
    copro-mc6809nc.h
    copro-mc6809nc.c
    mc6809nc/mc6809.c
    mc6809nc/mc6809.h
)

# Shared 6809 files
file( GLOB shared_6809_files
  tuberom_6809.h
  tuberom_6809.c
)

# OPC5LS
file( GLOB copro_opc5ls_files
    copro-opc5ls.h
    copro-opc5ls.c
    opc5ls/opc5ls.h
    opc5ls/opc5ls.c
    opc5ls/tuberom.h
    opc5ls/tuberom.c
)

# OPC6
file( GLOB copro_opc6_files
    copro-opc6.h
    copro-opc6.c
    opc6/opc6.h
    opc6/opc6.c
    opc6/tuberom.h
    opc6/tuberom.c
)

# OPC7
file( GLOB copro_opc7_files
    copro-opc7.h
    copro-opc7.c
    opc7/opc7.h
    opc7/opc7.c
    opc7/tuberom.h
    opc7/tuberom.c
)

# PDP 11
file( GLOB copro_pdp11_files
    copro-pdp11.h
    copro-pdp11.c
    pdp11/pdp11.h
    pdp11/pdp11.c
    pdp11/tuberom.h
    pdp11/tuberom.c
)

# Null co processor
file( GLOB copro_null_files
    copro-null.c
    copro-null.h
)

# ARM
file( GLOB copro_arm_files
    copro-armnative.c
    copro-armnativeasm.S
    copro-armnative.h
    rpi-armtimer.c
    rpi-armtimer.h
    rpi-systimer.c
    rpi-systimer.h
    swi.c
    swi.h
    tube-env.h
    tube-swi.c
    tube-swi.h
    tube-isr.c
    tube-isr.h
    tube-lib.c
    tube-lib.h
    tube-commands.c
    tube-commands.h
    darm/armv7.c
    darm/armv7-tbl.c
    darm/armv7-tbl.h
    darm/darm.c
    darm/darm.h
    darm/darm-internal.h
    darm/darm-tbl.c
    darm/darm-tbl.h
    darm/thumb2.c
    darm/thumb2-decoder.c
    darm/thumb2.h
    darm/thumb2-tbl.c
    darm/thumb2-tbl.h
    darm/thumb.c
    darm/thumb-tbl.c
    darm/thumb-tbl.h
)

# Interactive debugger files
file( GLOB debugger_files
    debugger/debugger.c
    debugger/debugger.h
    debugger/linenoise.c
    debugger/linenoise.h
    cpu_debug.h
    lib6502_debug.c
    lib6502_debug.h
    yaze/z80dis.c
    yaze/z80dis.h
    NS32016/32016_debug.c
    NS32016/32016_debug.h
    mame/arm_debug.c
    mame/arm_debug.h
    mc6809nc/mc6809.c
    mc6809nc/mc6809.h
    mc6809nc/mc6809_debug.h
    mc6809nc/mc6809_debug.c
    mc6809nc/mc6809_dis.c
    mc6809nc/mc6809_dis.h
    cpu80186/cpu80186_debug.c
    cpu80186/cpu80186_debug.h
    opc5ls/opc5ls_debug.c
    opc5ls/opc5ls_debug.h
    opc6/opc6_debug.c
    opc6/opc6_debug.h
    opc7/opc7_debug.c
    opc7/opc7_debug.h
    pdp11/pdp11_debug.c
    pdp11/pdp11_debug.h
)

if( ${DEBUG} )

    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG=1 " )

    if( NOT ( ${MINIMAL_BUILD} ) )
        set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DINCLUDE_DEBUGGER=1 " )
    endif()

endif()


if( ${MINIMAL_BUILD} )

    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMINIMAL_BUILD=1" )

    add_executable( tube-client
        ${core_files}
        ${copro_65tube_files}
        ${shared_6502_files}
    )

else()

    if( ${DEBUG} )

        add_executable( tube-client
            ${core_files}
            ${copro_65tube_files}
            ${copro_lib6502_files}
            ${shared_6502_files}
            ${copro_80186_files}
            ${copro_arm2_files}
            ${copro_32016_files}
            ${copro_z80_files}
            ${copro_6809_files}
            ${copro_6809sc_files}
            ${copro_6809nc_files}
            ${shared_6809_files}
            ${copro_opc5ls_files}
            ${copro_opc6_files}
            ${copro_opc7_files}
            ${copro_pdp11_files}
            ${copro_null_files}
            ${copro_arm_files}
            ${debugger_files}
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
            ${copro_6809_files}
            ${copro_6809sc_files}
            ${copro_6809nc_files}
            ${shared_6809_files}
            ${copro_opc5ls_files}
            ${copro_opc6_files}
            ${copro_opc7_files}
            ${copro_pdp11_files}
            ${copro_null_files}
            ${copro_arm_files}
        )

    endif()

endif()
    
target_link_libraries (tube-client m)

add_custom_command(
    TARGET tube-client POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ./tube-client -O binary ${KERNEL_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Convert the ELF output file to a binary image" )

# Generate a header file with the current git version in it

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(gitversion ALL)

add_custom_command(
    TARGET gitversion
    COMMAND bash version.sh
)

add_dependencies(tube-client gitversion)

