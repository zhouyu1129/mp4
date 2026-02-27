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
set(CMAKE_PROJECT_NAME mp4)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# 1. 通用编译器标志
set(COMMON_FLAGS " -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# 2. 为 C, C++, ASM 分别设置标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}${COMMON_FLAGS}")

# 3. 为链接器设置标志 (这是最关键的一步)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -Wl,--gc-sections -static -u _printf_float -u _scanf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group")

# 将这些标志添加到C和C++编译器中
set(CMAKE_C_FLAGS "${MCU_FLAGS}${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${MCU_FLAGS}${CMAKE_CXX_FLAGS}")

# Enable CMake support for ASM and C languages
enable_language(C ASM)
enable_language(CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Core/Src/main.cpp
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
        SUFFIX ".elf"
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
file(GLOB_RECURSE ST7735_SOURCES "./st7735/*.c*")
file(GLOB_RECURSE STATIC_PIC_SOURCES "./data/pic/*.c*")
file(GLOB_RECURSE TJPGDEC_SOURCES "./TJpgDec/*.c*")
file(GLOB_RECURSE EASYMENU_SOURCES "./EasyMenu/*.c*")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
        ${ST7735_SOURCES}
        ${STATIC_PIC_SOURCES}
        ${TJPGDEC_SOURCES}
        ${EASYMENU_SOURCES}
        ./Core/Src/fs.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        ./st7735
        ./data/font
        ./data/pic
        ./TJpgDec
        ./EasyMenu
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
