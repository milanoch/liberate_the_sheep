cmake_minimum_required(VERSION 3.16)
project(liberate_the_sheep)

set(CMAKE_CXX_STANDARD 20)
set(BIN_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BIN_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BIN_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIRECTORY})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-DPROFILE)
endif ()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE INCLUDE CONFIGURE_DEPENDS src/*.hpp)

add_library(liberate_the_sheep SHARED "${SOURCES}" liberate_the_sheep.def "${INCLUDE}")

set_target_properties(liberate_the_sheep
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIRECTORY}"
)

# Multi-config generators (Visual Studio, Xcode) append /<CONFIG> to the output
# directories unless the per-config properties are set; the engine loads the
# script library from bin/ directly.
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set_target_properties(liberate_the_sheep
            PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BIN_DIRECTORY}"
            LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BIN_DIRECTORY}"
            RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} "${BIN_DIRECTORY}"
    )
endforeach()

# add_custom_command(TARGET liberate_the_sheep POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:liberate_the_sheep> ${BIN_DIRECTORY}
# )

target_include_directories(liberate_the_sheep PUBLIC ${PROJECT_SOURCE_DIR}/src)

find_package(PandaSDK CONFIG REQUIRED)
target_link_libraries(liberate_the_sheep PandaSDK::PandaSDK)

# Offline animation tracks share the tested C++ stride/contact contract.
# This is an asset-build helper, not an editor/game entry point.
add_executable(liberate_the_sheep_export_trot tools/ExportSheepTrot.cpp)
target_include_directories(liberate_the_sheep_export_trot PRIVATE ${PROJECT_SOURCE_DIR}/src)

include(CTest)
if (BUILD_TESTING)
    # Produces static layout data for the offline source-art preview, not a game.
    add_executable(liberate_the_sheep_export_pasture
            tools/ExportPasturePreview.cpp
            src/Game/BoardModel.cpp
            src/Game/BoardGenerator.cpp
            src/Game/BoardLayout.cpp
            src/Game/LevelCatalog.cpp
            src/Game/SheepAppearance.cpp
            src/Game/PastureSurface.cpp)
    target_include_directories(liberate_the_sheep_export_pasture PRIVATE ${PROJECT_SOURCE_DIR}/src)

    add_executable(liberate_the_sheep_logic_tests
            tests/BoardLogicSmoke.cpp
            src/Game/BoardModel.cpp
            src/Game/BoardGenerator.cpp
            src/Game/LevelCatalog.cpp
            src/Game/BoardLayout.cpp
            src/Game/Localization.cpp
            src/Game/SessionState.cpp
            src/Game/SheepAppearance.cpp
    )
    target_include_directories(liberate_the_sheep_logic_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_logic COMMAND liberate_the_sheep_logic_tests)

    add_executable(liberate_the_sheep_menu_motion_tests
            tests/MenuSheepMotionSmoke.cpp
            src/Game/MenuSheepMotion.cpp
    )
    target_include_directories(liberate_the_sheep_menu_motion_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_menu_motion COMMAND liberate_the_sheep_menu_motion_tests)

    add_executable(liberate_the_sheep_pen_escape_tests
            tests/PenEscapePathSmoke.cpp
            src/Game/PenEscapePath.cpp
            src/Game/BoardLayout.cpp
            src/Game/BoardModel.cpp
            src/Game/BoardGenerator.cpp
            src/Game/LevelCatalog.cpp
    )
    target_include_directories(liberate_the_sheep_pen_escape_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_pen_escape COMMAND liberate_the_sheep_pen_escape_tests)

    add_executable(liberate_the_sheep_escape_herd_tests
            tests/EscapeHerdSmoke.cpp
            src/Game/EscapeHerd.cpp
            src/Game/EscapeTraffic.cpp
            src/Game/PenEscapePath.cpp
            src/Game/BoardModel.cpp
    )
    target_include_directories(liberate_the_sheep_escape_herd_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_escape_herd COMMAND liberate_the_sheep_escape_herd_tests)

    add_executable(liberate_the_sheep_escape_traffic_tests
            tests/EscapeTrafficSmoke.cpp
            src/Game/EscapeHerd.cpp
            src/Game/EscapeTraffic.cpp
            src/Game/PenEscapePath.cpp
            src/Game/BoardModel.cpp
            src/Game/BoardGenerator.cpp
            src/Game/LevelCatalog.cpp
    )
    target_include_directories(liberate_the_sheep_escape_traffic_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_escape_traffic COMMAND liberate_the_sheep_escape_traffic_tests)

    add_executable(liberate_the_sheep_trot_motion_tests tests/SheepTrotMotionSmoke.cpp)
    target_include_directories(liberate_the_sheep_trot_motion_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_trot_motion COMMAND liberate_the_sheep_trot_motion_tests)

    add_executable(liberate_the_sheep_pasture_wear_tests
            tests/PastureWearSmoke.cpp src/Game/BoardModel.cpp)
    target_include_directories(liberate_the_sheep_pasture_wear_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_pasture_wear COMMAND liberate_the_sheep_pasture_wear_tests)

    add_executable(liberate_the_sheep_pasture_ground_tests
            tests/PastureGroundSmoke.cpp src/Game/BoardLayout.cpp src/Game/BoardModel.cpp)
    target_include_directories(liberate_the_sheep_pasture_ground_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_pasture_ground COMMAND liberate_the_sheep_pasture_ground_tests)

    add_executable(liberate_the_sheep_pasture_surface_tests
            tests/PastureSurfaceSmoke.cpp src/Game/PastureSurface.cpp
            src/Game/BoardModel.cpp src/Game/BoardLayout.cpp)
    target_include_directories(liberate_the_sheep_pasture_surface_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_pasture_surface COMMAND liberate_the_sheep_pasture_surface_tests)

    add_executable(liberate_the_sheep_pasture_color_tests tests/PastureColorSpaceSmoke.cpp)
    target_include_directories(liberate_the_sheep_pasture_color_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME liberate_the_sheep_pasture_color COMMAND liberate_the_sheep_pasture_color_tests)
endif ()
