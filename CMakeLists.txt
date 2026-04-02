cmake_minimum_required(VERSION 3.16)
project(QtButtonApp)

# Find the Qt6 libraries
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Create the executable
add_executable(roku_sleep_timer roku-sleep-timer.cpp)

# Link the Qt6 libraries
target_link_libraries(qt_app PRIVATE Qt6::Widgets Qt6::Network)
