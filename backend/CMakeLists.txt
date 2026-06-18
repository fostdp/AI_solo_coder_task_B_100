cmake_minimum_required(VERSION 3.15)
project(fenyun_vehicle_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W3 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(USE_BOOST_LOCKFREE "Use Boost.Lockfree for lock-free queues" OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)

if(USE_BOOST_LOCKFREE)
    find_package(Boost REQUIRED)
    add_definitions(-DFENYUN_USE_BOOST_LOCKFREE)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost.Lockfree: ENABLED")
else()
    message(STATUS "Boost.Lockfree: DISABLED (using internal SPSC lock-free queue)")
endif()

set(BACKEND_SOURCES
    src/main.cpp
    src/fenyun_application.cpp
    src/fenyun_http_server.cpp

    src/config/config_loader.cpp

    src/dtu_receiver/dtu_receiver.cpp
    src/impact_simulator/impact_simulator.cpp
    src/protection_optimizer/protection_optimizer.cpp
    src/alarm_mqtt/alarm_mqtt.cpp
    src/storage/clickhouse_client.cpp
)

set(BACKEND_HEADERS
    include/fenyun_application.h
    include/fenyun_http_server.h

    include/common/data_types.h
    include/common/json.h
    include/common/lockfree_queue.h

    include/config/config_loader.h

    include/dtu_receiver/dtu_receiver.h
    include/impact_simulator/impact_simulator.h
    include/protection_optimizer/protection_optimizer.h
    include/alarm_mqtt/alarm_mqtt.h
    include/storage/clickhouse_client.h
)

find_package(Threads REQUIRED)

add_executable(fenyun_backend ${BACKEND_SOURCES} ${BACKEND_HEADERS})

target_link_libraries(fenyun_backend PRIVATE Threads::Threads)

if(USE_BOOST_LOCKFREE)
    target_link_libraries(fenyun_backend PRIVATE ${Boost_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(fenyun_backend PRIVATE ws2_32)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/config/ DESTINATION ${CMAKE_BINARY_DIR}/config)

install(TARGETS fenyun_backend RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION config)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "轒辒车仿真后端 - 模块化架构")
message(STATUS "========================================")
message(STATUS "C++ 标准:        C++17")
message(STATUS "系统平台:         ${CMAKE_SYSTEM_NAME}")
message(STATUS "编译器:            ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Boost.Lockfree:   ${USE_BOOST_LOCKFREE}")
message(STATUS "模块数量:         5 (dtu/impact_sim/optimizer/alarm/storage)")
message(STATUS "队列通信:         LockFreeQueue (SPSC无锁)")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "构建命令:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  cmake --build . --config Release")
message(STATUS "")
message(STATUS "可选 Boost.Lockfree:")
message(STATUS "  cmake .. -DUSE_BOOST_LOCKFREE=ON")
message(STATUS "")
message(STATUS "运行命令:")
message(STATUS "  ./fenyun_backend --http-port 8080 --config-dir ./config")
message(STATUS "")
