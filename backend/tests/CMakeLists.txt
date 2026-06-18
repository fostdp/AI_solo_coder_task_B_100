enable_testing()

set(TEST_SOURCES
    test_main.cpp
    vehicle_comparator_test.cpp
    formation_optimizer_test.cpp
    user_session_manager_test.cpp
    config_loader_test.cpp
    integration_test.cpp
    defect_fix_verification_test.cpp
)

add_executable(fenyun_tests ${TEST_SOURCES})

target_link_libraries(fenyun_tests PRIVATE
    fenyun_core
    gtest
    gtest_main
)

target_compile_definitions(fenyun_tests PRIVATE FENYUN_TEST_MODE=1)

if(WIN32)
    target_link_libraries(fenyun_tests PRIVATE ws2_32)
endif()

add_test(NAME fenyun_unit_tests COMMAND fenyun_tests
    --gtest_filter="VehicleComparatorUnit.*:FormationOptimizerUnit.*:UserSessionUnit.*:ConfigLoaderUnit.*"
)

add_test(NAME fenyun_integration_tests COMMAND fenyun_tests
    --gtest_filter="Integration.*"
)

target_compile_definitions(fenyun_tests PRIVATE
    FENYUN_TEST_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config"
)
