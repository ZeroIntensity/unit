cmake_minimum_required(VERSION 3.10)

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

option(UNIT_BUILD_EXAMPLES "Build example programs" OFF)

if(UNIT_BUILD_EXAMPLES)
    add_executable(unit_guess "${CMAKE_SOURCE_DIR}/examples/guessing_game.c")
    target_link_libraries(unit_guess PRIVATE unit)
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
