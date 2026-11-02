cmake_minimum_required(VERSION 3.20)

# Two builds from one tree:
#   cmake -B build/target -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake   firmware for the board
#   cmake -B build/host                                                       host tests
project(openmusicplayer C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(WARN -Wall -Wextra -Werror -Wshadow -Wdouble-promotion)

# Board-independent logic, tested on the host
set(LOGIC_SRC src/output.c src/ak4493.c)

if (NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(test_output ${LOGIC_SRC} test/fake_board.c test/test_output.c)
    target_include_directories(test_output PRIVATE src test)
    target_compile_options(test_output PRIVATE ${WARN} -fsanitize=address,undefined)
    target_link_options(test_output PRIVATE -fsanitize=address,undefined)
    add_test(NAME output COMMAND test_output)
    return()
endif ()

set(TUSB ${DEPS}/tinyusb)
set(CORE ${DEPS}/mcuxsdk-core/drivers)
set(DEV ${DEPS}/mcux-devices-rt/RT1010/MIMXRT1011)

add_executable(openmusicplayer
    ${LOGIC_SRC}
    src/main.c
    src/usb_audio.c
    src/usb_descriptors.c
    board/board.c
    board/sai.c
    board/pins.c
    board/clock_config.c
    board/flexspi_nor_config.c
    # NXP SDK
    ${CORE}/common/fsl_common.c
    ${CORE}/common/fsl_common_arm.c
    ${CORE}/igpio/fsl_gpio.c
    ${CORE}/lpi2c/fsl_lpi2c.c
    ${CORE}/lpuart/fsl_lpuart.c
    ${DEV}/system_MIMXRT1011.c
    ${DEV}/drivers/fsl_clock.c
    ${DEPS}/mcux-devices-rt/RT1050/MIMXRT1052/xip/fsl_flexspi_nor_boot.c
    ${DEV}/gcc/startup_MIMXRT1011.S
    # TinyUSB
    ${TUSB}/src/tusb.c
    ${TUSB}/src/common/tusb_fifo.c
    ${TUSB}/src/device/usbd.c
    ${TUSB}/src/class/audio/audio_device.c
    ${TUSB}/src/portable/chipidea/ci_hs/dcd_ci_hs.c
)

target_include_directories(openmusicplayer PRIVATE src board)
# Vendor headers as system headers: their warnings aren't ours
target_include_directories(openmusicplayer SYSTEM PRIVATE
    ${DEPS}/CMSIS_6/CMSIS/Core/Include
    ${DEV} ${DEV}/drivers ${DEPS}/mcux-devices-rt/RT1010/periph
    ${CORE}/common ${CORE}/igpio ${CORE}/lpi2c ${CORE}/lpuart
    ${DEPS}/mcux-devices-rt/RT1050/MIMXRT1052/xip
    ${TUSB}/src
)

target_compile_definitions(openmusicplayer PRIVATE
    CPU_MIMXRT1011DAE5A
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    __STARTUP_CLEAR_BSS
    __START=main
    [=[CFG_TUSB_MEM_SECTION=__attribute__((section("NonCacheable")))]=]
)

# Our code builds clean; vendor code gets its own warnings left alone
set_source_files_properties(${LOGIC_SRC} src/main.c src/usb_audio.c src/usb_descriptors.c
    board/board.c board/sai.c board/pins.c board/flexspi_nor_config.c
    PROPERTIES COMPILE_OPTIONS "${WARN}")
target_compile_options(openmusicplayer PRIVATE -Os -g)

target_link_options(openmusicplayer PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/board/omp.ld
    -Wl,-Map=openmusicplayer.map
    -Wl,--print-memory-usage
)
set_target_properties(openmusicplayer PROPERTIES SUFFIX .elf)

add_custom_command(TARGET openmusicplayer POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:openmusicplayer> openmusicplayer.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:openmusicplayer> openmusicplayer.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:openmusicplayer>
)
