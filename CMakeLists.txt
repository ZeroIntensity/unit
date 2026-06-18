cmake_minimum_required(VERSION 3.15)

project(unit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.c")

add_library(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(UNIT_BUILD_EXAMPLES "Build example programs" OFF)

if(UNIT_BUILD_EXAMPLES)
    add_executable(unit_guess "${CMAKE_SOURCE_DIR}/examples/guessing_game.c")
    target_link_libraries(unit_guess PRIVATE unit)

    add_executable(unit_brainf "${CMAKE_SOURCE_DIR}/examples/brainf.c")
    target_link_libraries(unit_brainf PRIVATE unit)

    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_executable(unit_factorial "${CMAKE_SOURCE_DIR}/examples/factorial.cpp")
    target_link_libraries(unit_factorial PRIVATE unit)
endif()

option(UNIT_BUILD_TESTS "Build tests" OFF)

if(UNIT_BUILD_TESTS)
    enable_testing()

    set(TEST_TARGETS "")
    file(GLOB TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/test_*.c")
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE unit)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        list(APPEND TEST_TARGETS ${TEST_NAME})
    endforeach()

    if(UNIT_BUILD_EXAMPLES)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        file(GLOB INTEGRATION_TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/test_*.py")
        foreach(INTEGRATION_TEST_SOURCE ${INTEGRATION_TEST_SOURCES})
            get_filename_component(INTEGRATION_TEST_NAME ${INTEGRATION_TEST_SOURCE} NAME_WE)
            add_test(
                NAME ${INTEGRATION_TEST_NAME}
                COMMAND ${Python3_EXECUTABLE} ${INTEGRATION_TEST_SOURCE}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(${INTEGRATION_TEST_NAME} PROPERTIES
                ENVIRONMENT "BUILD_DIR=${CMAKE_BINARY_DIR}"
            )
        endforeach()
    endif()

    add_custom_target(tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS ${TEST_TARGETS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

option(UNIT_BUILD_PYTHON "Build Python bindings" OFF)

if(UNIT_BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
    Python3_add_library(_core MODULE python/unit/core.c)
    target_link_libraries(_core PRIVATE unit)
    install(TARGETS _core DESTINATION unit)
endif()

include(GNUInstallDirs)

install(TARGETS unit
    EXPORT unitTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/unit
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT unitTargets
    FILE unitConfig.cmake
    NAMESPACE unit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/unit
)

configure_file(unit.pc.in unit.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/unit.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
