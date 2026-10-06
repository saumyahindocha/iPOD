cmake_minimum_required(VERSION 3.13)

# Build for the Pico 2 W unless told otherwise
if (NOT PICO_BOARD)
    set(PICO_BOARD pico2_w)
endif()

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)   # needs PICO_EXTRAS_PATH (for pico_audio_i2s)

project(pod_bringup C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(POD_COMMON ${CMAKE_CURRENT_LIST_DIR}/common)

# ---------------------------------------------------------------- DAC test
add_executable(pod_dac_test dac_test/main.c)
target_include_directories(pod_dac_test PRIVATE ${POD_COMMON})
target_compile_definitions(pod_dac_test PRIVATE
    PICO_AUDIO_I2S_DATA_PIN=9 PICO_AUDIO_I2S_CLOCK_PIN_BASE=10)
target_link_libraries(pod_dac_test PRIVATE pico_stdlib pico_audio_i2s)
pico_enable_stdio_usb(pod_dac_test 1)
pico_enable_stdio_uart(pod_dac_test 0)
pico_add_extra_outputs(pod_dac_test)

# ------------------------------------------------------ Display + touch test
add_executable(pod_display_test
    display_test/main.c common/ili9341.c common/xpt2046.c)
target_include_directories(pod_display_test PRIVATE ${POD_COMMON})
target_link_libraries(pod_display_test PRIVATE pico_stdlib hardware_spi)
pico_enable_stdio_usb(pod_display_test 1)
pico_enable_stdio_uart(pod_display_test 0)
pico_add_extra_outputs(pod_display_test)

# ------------------------------------------------ Bluetooth A2DP sink -> DAC
add_executable(pod_bt_sink
    bt_sink/main.c
    bt_sink/btstack_audio_pico.c
    bt_sink/a2dp_sink_demo.c
    common/pod_display.c            # Option C now-playing screen, runs on core 1
    common/pod_gfx.c                # anti-aliased framebuffer renderer
    common/pod_fonts.c              # Inter, pre-rendered by tools/gen_fonts.py
    common/ili9341.c
    common/xpt2046.c
    third_party/tjpgd/tjpgd.c)      # ChaN's TJpgDec (via Bodmer's TJpg_Decoder)
target_include_directories(pod_bt_sink PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bt_sink ${POD_COMMON} ${CMAKE_CURRENT_LIST_DIR}/third_party/tjpgd)
target_compile_definitions(pod_bt_sink PRIVATE
    USING_I2C=1                         # (sic) - upstream name; enables the I2S audio sink
    PICO_AUDIO_I2S_DATA_PIN=9
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=10)
target_link_libraries(pod_bt_sink PRIVATE
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_cyw43
    pico_btstack_ble
    pico_btstack_classic
    pico_btstack_sbc_decoder
    pico_audio_i2s
    pico_multicore
    pico_sync
    pico_flash
    hardware_flash
    hardware_spi)
pico_enable_stdio_usb(pod_bt_sink 1)    # logs over USB - no UART adapter needed
pico_enable_stdio_uart(pod_bt_sink 0)
pico_add_extra_outputs(pod_bt_sink)
