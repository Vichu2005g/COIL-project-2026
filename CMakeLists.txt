cmake_minimum_required(VERSION 3.10)
project(COILRobotControl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(src)
include_directories(external)

# Platform-specific settings
if(WIN32)
    set(LIBS ${LIBS} ws2_32.lib)
    add_definitions(-D_WIN32_WINNT=0x0601)
else()
    set(LIBS ${LIBS} pthread atomic)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/RobotController.cpp
    src/Packet.cpp
    src/Socket.cpp
)

# Executable
add_executable(RobotControlServer ${SOURCES})

# Link libraries
target_link_libraries(RobotControlServer ${LIBS} Threads::Threads)

# Static assets (copy or ensure accessible)
# In Docker, we will copy them to the same directory as the executable.
