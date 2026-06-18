cmake_minimum_required(VERSION 3.18)
project(fenyun_vehicle_backend VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    add_compile_options(/W3 /permissive- /utf-8 /EHsc)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /GL)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -flto)
        add_link_options(-flto)
    endif()
endif()

option(USE_BOOST_LOCKFREE "Use Boost.Lockfree for lock-free queues" OFF)
option(BUILD_TESTS "Build unit tests with GoogleTest" OFF)
option(ENABLE_PROMETHEUS "Enable Prometheus metrics endpoint" ON)
option(ENABLE_SPDLOG "Enable spdlog structured logging" ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

if(ENABLE_SPDLOG)
    message(STATUS "Fetching spdlog 1.14.1 ...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY  https://github.com/gabime/spdlog.git
        GIT_TAG         v1.14.1
        GIT_SHALLOW     TRUE
    )
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
    add_definitions(-DFENYUN_USE_SPDLOG)
    message(STATUS "spdlog: ENABLED")
else()
    message(STATUS "spdlog: DISABLED (using std::cout)")
endif()

if(ENABLE_PROMETHEUS)
    message(STATUS "Fetching prometheus-cpp 1.2.4 ...")
    FetchContent_Declare(
        prometheus-cpp
        GIT_REPOSITORY  https://github.com/jupp0r/prometheus-cpp.git
        GIT_TAG         v1.2.4
        GIT_SHALLOW     TRUE
    )
    set(PROMETHEUS_CPP_ENABLE_PULL ON CACHE BOOL "" FORCE)
    set(PROMETHEUS_CPP_ENABLE_PUSH OFF CACHE BOOL "" FORCE)
    set(PROMETHEUS_CPP_ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
    set(PROMETHEUS_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PROMETHEUS_CPP_USE_THIRDPARTY_LIBRARIES OFF CACHE BOOL "" FORCE)
    set(CURL_LIBRARY "" CACHE STRING "" FORCE)
    set(CURL_INCLUDE_DIR "" CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(prometheus-cpp)
    add_definitions(-DFENYUN_USE_PROMETHEUS)
    message(STATUS "prometheus-cpp: ENABLED (internal pull endpoint /metrics on port 9091)")
else()
    message(STATUS "prometheus-cpp: DISABLED")
endif()

if(USE_BOOST_LOCKFREE)
    find_package(Boost REQUIRED)
    add_definitions(-DFENYUN_USE_BOOST_LOCKFREE)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost.Lockfree: ENABLED")
else()
    message(STATUS "Boost.Lockfree: DISABLED (using internal SPSC lock-free queue)")
endif()

find_package(Threads REQUIRED)

add_library(fenyun_common STATIC
    src/common/metrics_collector.cpp
    src/common/logger_init.cpp
    include/common/data_types.h
    include/common/json.h
    include/common/lockfree_queue.h
    include/common/metrics_collector.h
    include/common/logger_init.h
)
target_link_libraries(fenyun_common PUBLIC Threads::Threads)
target_include_directories(fenyun_common PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(ENABLE_SPDLOG)
    target_link_libraries(fenyun_common PUBLIC spdlog::spdlog_header_only)
endif()
if(USE_BOOST_LOCKFREE)
    target_link_libraries(fenyun_common PUBLIC ${Boost_LIBRARIES})
endif()
if(WIN32)
    target_link_libraries(fenyun_common PUBLIC ws2_32)
endif()

add_library(fenyun_config STATIC
    src/config/config_loader.cpp
    include/config/config_loader.h
)
target_link_libraries(fenyun_config PUBLIC fenyun_common)

add_library(fenyun_impact_simulator STATIC
    src/impact_simulator/impact_simulator.cpp
    include/impact_simulator/impact_simulator.h
)
target_link_libraries(fenyun_impact_simulator PUBLIC fenyun_common fenyun_config)

add_library(fenyun_vehicle_comparator STATIC
    src/vehicle_comparator/vehicle_comparator.cpp
    include/vehicle_comparator/vehicle_comparator.h
)
target_link_libraries(fenyun_vehicle_comparator PUBLIC fenyun_common fenyun_config fenyun_impact_simulator)

add_library(fenyun_era_comparator STATIC
    src/era_comparator/era_comparator.cpp
    include/era_comparator/era_comparator.h
)
target_link_libraries(fenyun_era_comparator PUBLIC fenyun_common fenyun_config fenyun_vehicle_comparator)

add_library(fenyun_formation_optimizer STATIC
    src/formation_optimizer/formation_optimizer.cpp
    include/formation_optimizer/formation_optimizer.h
)
target_link_libraries(fenyun_formation_optimizer PUBLIC fenyun_common fenyun_config)

add_library(fenyun_vr_assault STATIC
    src/vr_assault/vr_assault_engine.cpp
    include/vr_assault/vr_assault_engine.h
)
target_link_libraries(fenyun_vr_assault PUBLIC fenyun_common fenyun_config fenyun_impact_simulator)

add_library(fenyun_protection_optimizer STATIC
    src/protection_optimizer/protection_optimizer.cpp
    include/protection_optimizer/protection_optimizer.h
)
target_link_libraries(fenyun_protection_optimizer PUBLIC fenyun_common fenyun_config)

add_library(fenyun_dtu_receiver STATIC
    src/dtu_receiver/dtu_receiver.cpp
    include/dtu_receiver/dtu_receiver.h
)
target_link_libraries(fenyun_dtu_receiver PUBLIC fenyun_common fenyun_config)

add_library(fenyun_user_session STATIC
    src/user_session/user_session_manager.cpp
    include/user_session/user_session_manager.h
)
target_link_libraries(fenyun_user_session PUBLIC fenyun_common fenyun_config)

add_library(fenyun_alarm_mqtt STATIC
    src/alarm_mqtt/alarm_mqtt.cpp
    include/alarm_mqtt/alarm_mqtt.h
)
target_link_libraries(fenyun_alarm_mqtt PUBLIC fenyun_common fenyun_config)

add_library(fenyun_storage STATIC
    src/storage/clickhouse_client.cpp
    include/storage/clickhouse_client.h
)
target_link_libraries(fenyun_storage PUBLIC fenyun_common fenyun_config)

add_library(fenyun_core STATIC
    src/fenyun_application.cpp
    src/fenyun_http_server.cpp
    include/fenyun_application.h
    include/fenyun_http_server.h
)

target_link_libraries(fenyun_core PUBLIC
    fenyun_common
    fenyun_config
    fenyun_impact_simulator
    fenyun_vehicle_comparator
    fenyun_era_comparator
    fenyun_formation_optimizer
    fenyun_vr_assault
    fenyun_protection_optimizer
    fenyun_dtu_receiver
    fenyun_user_session
    fenyun_alarm_mqtt
    fenyun_storage
)

if(ENABLE_PROMETHEUS)
    target_link_libraries(fenyun_core PUBLIC prometheus-cpp::core prometheus-cpp::pull)
endif()

add_executable(fenyun_backend src/main.cpp)
target_link_libraries(fenyun_backend PRIVATE fenyun_core)

set_target_properties(fenyun_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

file(COPY ${CMAKE_SOURCE_DIR}/config/ DESTINATION ${CMAKE_BINARY_DIR}/bin/config)

if(BUILD_TESTS)
    message(STATUS "Fetching googletest ...")
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY  https://github.com/google/googletest.git
        GIT_TAG         v1.14.0
        GIT_SHALLOW     TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
    message(STATUS "GoogleTest: ENABLED")
endif()

install(TARGETS fenyun_backend RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION config)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "轒辒车仿真后端 v${PROJECT_VERSION} - 工程化构建")
message(STATUS "========================================")
message(STATUS "C++ 标准:         C++17")
message(STATUS "系统平台:         ${CMAKE_SYSTEM_NAME}")
message(STATUS "编译器:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "构建类型:         ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "第三方集成:")
message(STATUS "  spdlog日志:     ${ENABLE_SPDLOG}")
message(STATUS "  Prometheus指标: ${ENABLE_PROMETHEUS} (端口9091 /metrics)")
message(STATUS "  Boost.Lockfree: ${USE_BOOST_LOCKFREE}")
message(STATUS "  单元测试:       ${BUILD_TESTS}")
message(STATUS "")
message(STATUS "业务模块:         dtu_receiver / impact_simulator / protection_optimizer / alarm_mqtt")
message(STATUS "队列通信:         LockFreeQueue (SPSC无锁, 可切换Boost实现)")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "构建命令:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake .. -DCMAKE_BUILD_TYPE=Release")
message(STATUS "  cmake --build . --config Release -j$(nproc)")
message(STATUS "")
message(STATUS "可选开关:")
message(STATUS "  -DUSE_BOOST_LOCKFREE=ON  (切换Boost.Lockfree队列)")
message(STATUS "  -DBUILD_TESTS=ON         (构建单元测试)")
message(STATUS "  -DENABLE_PROMETHEUS=OFF  (禁用Prometheus)")
message(STATUS "")
message(STATUS "运行:")
message(STATUS "  ./bin/fenyun_backend --http-port 8080 --config-dir ./config")
message(STATUS "")
