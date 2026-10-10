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
    PICO_AUDIO_I2S_DATA_PIN=13 PICO_AUDIO_I2S_CLOCK_PIN_BASE=14)
target_link_libraries(pod_dac_test PRIVATE pico_stdlib pico_audio_i2s)
pico_enable_stdio_usb(pod_dac_test 1)
pico_enable_stdio_uart(pod_dac_test 0)
pico_add_extra_outputs(pod_dac_test)

# ------------------------------------------------------ Display + touch test
add_executable(pod_display_test
    display_test/main.c common/ili9341.c common/xpt2046.c common/pod_spi_lock.c)
target_include_directories(pod_display_test PRIVATE ${POD_COMMON})
target_link_libraries(pod_display_test PRIVATE pico_stdlib hardware_spi)
pico_enable_stdio_usb(pod_display_test 1)
pico_enable_stdio_uart(pod_display_test 0)
pico_add_extra_outputs(pod_display_test)

# ------------------------------------------------- Touch doctor (diagnosis)
add_executable(pod_touch_doctor
    touch_doctor/main.c common/ili9341.c common/xpt2046.c common/pod_spi_lock.c)
target_include_directories(pod_touch_doctor PRIVATE ${POD_COMMON})
target_link_libraries(pod_touch_doctor PRIVATE pico_stdlib hardware_spi)
pico_enable_stdio_usb(pod_touch_doctor 1)
pico_enable_stdio_uart(pod_touch_doctor 0)
pico_add_extra_outputs(pod_touch_doctor)

# --------------------------------------- Touch setup (calibrate + save)
add_executable(pod_touch_setup
    touch_setup/main.c common/ili9341.c common/xpt2046.c common/pod_spi_lock.c)
target_include_directories(pod_touch_setup PRIVATE ${POD_COMMON})
target_link_libraries(pod_touch_setup PRIVATE pico_stdlib hardware_spi hardware_flash)
pico_enable_stdio_usb(pod_touch_setup 1)
pico_enable_stdio_uart(pod_touch_setup 0)
pico_add_extra_outputs(pod_touch_setup)

# ------------------------------------------ Pin finder (DAC wiring check)
add_executable(pod_pin_finder pin_finder/main.c)
target_include_directories(pod_pin_finder PRIVATE ${POD_COMMON})
target_link_libraries(pod_pin_finder PRIVATE pico_stdlib)
pico_add_extra_outputs(pod_pin_finder)

# ----------------------------------------- Pod main firmware (pod.uf2)
# Home screen -> Phone mode (Bluetooth A2DP sink + album art) or SD card mode
# (MP3/WAV player). Both use the Poster UI on core 1.
add_executable(pod
    bt_sink/main.c
    bt_sink/btstack_audio_pico.c
    bt_sink/a2dp_sink_demo.c
    common/pod_display.c            # home, Now Playing, library list - runs on core 1
    common/pod_gfx.c                # anti-aliased framebuffer renderer
    common/pod_fonts.c              # Inter, pre-rendered by tools/gen_fonts.py
    common/pod_logo.c               # "SH" monogram, tools/gen_logo.py
    common/pod_wallpaper.c          # home-screen picture in its own flash area (tools/make_wallpaper.py)
    common/pod_spi_lock.c           # display, touch and SD card share SPI0
    common/pod_battery.c            # battery level (GP28 divider or MAX17048) + low-battery warning
    common/sd_card.c                # SD over SPI
    common/ili9341.c
    common/xpt2046.c
    sd_player/sd_player.c           # SD mode: browser + MP3/WAV playback on core 0
    sd_player/track_meta.c          # tags, cover art, duration, seek table
    third_party/fatfs/ff.c          # ChaN's FatFs (read-only, FAT32 + exFAT)
    third_party/fatfs/ffunicode.c
    third_party/fatfs/diskio_sd.c
    third_party/tjpgd/tjpgd.c)      # ChaN's TJpgDec (via Bodmer's TJpg_Decoder)
target_include_directories(pod PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bt_sink ${POD_COMMON}
    ${CMAKE_CURRENT_LIST_DIR}/sd_player
    ${CMAKE_CURRENT_LIST_DIR}/third_party/tjpgd
    ${CMAKE_CURRENT_LIST_DIR}/third_party/fatfs
    ${CMAKE_CURRENT_LIST_DIR}/third_party/minimp3)
target_compile_definitions(pod PRIVATE
    USING_I2C=1                         # (sic) - upstream name; enables the I2S audio sink
    PICO_AUDIO_I2S_DATA_PIN=13
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=14
    PICO_STACK_SIZE=0x1000              # core 0: 4 KB (fills SCRATCH_Y); MP3 + FatFs need more than 2 KB
    PICO_CORE1_STACK_SIZE=0x800)
target_link_libraries(pod PRIVATE
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
    hardware_spi
    hardware_i2c
    hardware_adc
    hardware_dma
    hardware_watchdog)
pico_enable_stdio_usb(pod 1)    # logs over USB - no UART adapter needed
pico_enable_stdio_uart(pod 0)
pico_add_extra_outputs(pod)
