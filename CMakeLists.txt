cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Project)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/app_ui.c
    Core/Src/bt.c
    Core/Src/fmt.c
    Core/Src/font_bash.c
    Core/Src/input.c
    Core/Src/mpu6050.c
    Core/Src/start.c
    Core/Src/sys_info.c
    Core/bash_oled/driver/bash_oled_driver.c
    Core/bash_oled/graphics/bash_oled.c
    Core/bash_oled/graphics/bash_oled_bitmap.c
    Core/bash_oled/graphics/bash_oled_draw.c
    Core/bash_oled/graphics/bash_oled_font.c
    Core/bash_ui/src/bash_ui.c
    Core/bash_ui/src/bash_ui_anim.c
    Core/bash_ui/src/bash_ui_home.c
    Core/bash_ui/src/bash_ui_nav.c
    Core/bash_ui/src/bash_ui_page.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src
    Core/bash_oled/driver
    Core/bash_oled/graphics
    Core/bash_oled/include
    Core/bash_ui/include
    Core/bash_ui/src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# 编译后自动生成 .hex / .bin，方便用 STM32CubeProgrammer 或 ST-LINK Utility 直接下载。
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex / ${CMAKE_PROJECT_NAME}.bin"
)
