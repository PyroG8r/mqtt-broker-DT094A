cmake_minimum_required(VERSION 3.16)

project(MqttBroker LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MQTT_ENABLE_WARNINGS "Enable extra compiler warnings" ON)

set(_warning_flags -Wall -Wextra -Wpedantic)

add_executable(mqtt-broker
    src/main.cpp
    src/broker/MqttBroker.cpp
    src/connection/Connection.cpp
    src/protocol/MqttPacket.cpp
    src/metrics/BrokerMetrics.cpp
)

if(MQTT_ENABLE_WARNINGS)
    target_compile_options(mqtt-broker PRIVATE ${_warning_flags})
endif()

target_include_directories(mqtt-broker PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Add prometheus-cpp as a dependency
find_package(prometheus-cpp CONFIG REQUIRED)

target_link_libraries(mqtt-broker 
    prometheus-cpp::pull  # For HTTP server with /metrics endpoint
    prometheus-cpp::core
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)