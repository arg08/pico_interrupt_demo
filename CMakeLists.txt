cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()


project(irq_demo C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

include(FetchContent)
set(GIT_IGNORE_UNTRACKED TRUE)
FetchContent_Declare(cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 904dbda1336ba4b9a1415a68d5f203f576b696bb
)
FetchContent_MakeAvailable(cmake_git_version_tracking)



add_executable(irq_demo
        main.c
        )

# Using USB and/or UART console
pico_enable_stdio_usb(irq_demo 1)
pico_enable_stdio_uart(irq_demo 1)


# pull in common dependencies
target_link_libraries(irq_demo
	pico_stdlib
	pico_multicore
	cmake_git_version_tracking
	)

# create map/bin/hex file etc.
pico_add_extra_outputs(irq_demo)

