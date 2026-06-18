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

include_directories(${CMAKE_SOURCE_DIR}/include)

set(BACKEND_SOURCES
    src/main.cpp
    src/fenyun_server.cpp
    src/structural_simulation.cpp
    src/ahp_evaluator.cpp
    src/clickhouse_client.cpp
    src/mqtt_alert_manager.cpp
)

set(BACKEND_HEADERS
    include/common.h
    include/fenyun_server.h
    include/structural_simulation.h
    include/ahp_evaluator.h
    include/clickhouse_client.h
    include/mqtt_alert_manager.h
    include/json.h
)

find_package(Threads REQUIRED)

add_executable(fenyun_backend ${BACKEND_SOURCES} ${BACKEND_HEADERS})

target_link_libraries(fenyun_backend PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(fenyun_backend PRIVATE ws2_32)
endif()

install(TARGETS fenyun_backend
    RUNTIME DESTINATION bin
)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "轒辒车仿真后端构建配置完成")
message(STATUS "C++ 标准:       C++17")
message(STATUS "系统平台:        ${CMAKE_SYSTEM_NAME}")
message(STATUS "编译器:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "依赖库:           Threads (内置JSON解析)")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "构建命令:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  cmake --build .")
message(STATUS "")
message(STATUS "运行命令:")
message(STATUS "  ./fenyun_backend --http-port 8080")
message(STATUS "")
