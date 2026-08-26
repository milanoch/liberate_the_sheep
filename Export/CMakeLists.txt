cmake_minimum_required(VERSION 3.20)
project(liberate_the_sheep CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)

# Platform detection
if(APPLE)
    if(IOS)
        set(PLATFORM_IOS TRUE)
    else()
        set(PLATFORM_MACOS TRUE)
    endif()
elseif(ANDROID)
    set(PLATFORM_ANDROID TRUE)
elseif(WIN32)
    set(PLATFORM_WIN32 TRUE)
else()
    set(PLATFORM_LINUX TRUE)
endif()

# Find PandaSDK - provides pre-compiled runtime + scripting API
find_package(PandaSDK CONFIG REQUIRED)

if(NOT EXISTS "${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}")
    message(FATAL_ERROR "PandaSDK common resources not found: ${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}")
endif()

if(NOT EXISTS "${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}")
    message(FATAL_ERROR "PandaSDK shader resources not found: ${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}")
endif()

set(APP_SOURCES
    App/StartApp.cpp
)

get_filename_component(GAME_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GAME_SCRIPT_DIR "${GAME_PROJECT_ROOT}/Scripts/liberate_the_sheep/src")

if(EXISTS "${GAME_SCRIPT_DIR}")
    file(GLOB_RECURSE GAME_SCRIPT_SOURCES CONFIGURE_DEPENDS
        "${GAME_SCRIPT_DIR}/*.cpp"
        "${GAME_SCRIPT_DIR}/*.cc"
        "${GAME_SCRIPT_DIR}/*.cxx"
        "${GAME_SCRIPT_DIR}/*.mm"
    )
    file(GLOB_RECURSE GAME_SCRIPT_HEADERS CONFIGURE_DEPENDS
        "${GAME_SCRIPT_DIR}/*.hpp"
        "${GAME_SCRIPT_DIR}/*.h"
    )
    list(APPEND APP_SOURCES ${GAME_SCRIPT_SOURCES} ${GAME_SCRIPT_HEADERS})
else()
    message(STATUS "No game script source directory found: ${GAME_SCRIPT_DIR}")
endif()

if(PLATFORM_MACOS OR PLATFORM_LINUX OR PLATFORM_WIN32)
    list(APPEND APP_SOURCES EntryPoint/Desktop/main.cpp)
elseif(PLATFORM_IOS)
    list(APPEND APP_SOURCES EntryPoint/IOS/main.mm)
endif()

# ---- App executable ----
# Platform entry point and app startup are generated in this project.
# Engine/runtime implementation is pre-compiled in PandaSDK.
if(PLATFORM_MACOS)
    set(APPLE_RESOURCES native/OSX/AssetsOSX.xcassets)
    add_executable(liberate_the_sheep MACOSX_BUNDLE ${APP_SOURCES} ${APPLE_RESOURCES})
    set_target_properties(liberate_the_sheep PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/native/OSX/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.company.liberate_the_sheep"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        RESOURCE "${APPLE_RESOURCES}"
    )
    target_compile_definitions(liberate_the_sheep PUBLIC GL_SILENCE_DEPRECATION)
elseif(PLATFORM_IOS)
    set(APPLE_RESOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/LaunchScreen.storyboard")
        list(APPEND APPLE_RESOURCES native/IOS/LaunchScreen.storyboard)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Assets.xcassets")
        list(APPEND APPLE_RESOURCES native/IOS/Assets.xcassets)
    endif()
    add_executable(liberate_the_sheep MACOSX_BUNDLE ${APP_SOURCES} ${APPLE_RESOURCES})
    set_target_properties(liberate_the_sheep PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.company.liberate_the_sheep"
        RESOURCE "${APPLE_RESOURCES}"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
        BUILD_RPATH "@executable_path"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native/IOS/Assets.xcassets")
        set_target_properties(liberate_the_sheep PROPERTIES XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
    endif()
    target_compile_definitions(liberate_the_sheep PUBLIC GL_SILENCE_DEPRECATION GLES_SILENCE_DEPRECATION)
elseif(PLATFORM_LINUX)
    add_executable(liberate_the_sheep ${APP_SOURCES})
elseif(PLATFORM_WIN32)
    # Подсистема WINDOWS (без консоли), но точка входа — обычный main()
    # из EntryPoint/Desktop/main.cpp, а не WinMain.
    add_executable(liberate_the_sheep WIN32 ${APP_SOURCES})
    target_link_options(liberate_the_sheep PRIVATE "/ENTRY:mainCRTStartup")
elseif(PLATFORM_ANDROID)
    add_library(liberate_the_sheep SHARED ${APP_SOURCES})
endif()

# Link against pre-compiled runtime. Entry point remains in generated app sources.
target_link_libraries(liberate_the_sheep PandaSDK::PandaRuntime)
if(EXISTS "${GAME_SCRIPT_DIR}")
    target_include_directories(liberate_the_sheep PRIVATE "${GAME_SCRIPT_DIR}")
endif()

set(PANDA_GAME_RUNTIME_DATA_DIR "${GAME_PROJECT_ROOT}/.Panda/Cooked/Runtime")
if(NOT EXISTS "${PANDA_GAME_RUNTIME_DATA_DIR}/project.bin")
    message(WARNING "Cooked runtime data not found. Run Export Game in PandaEditor before packaging: ${PANDA_GAME_RUNTIME_DATA_DIR}")
endif()
set(PANDA_CHECK_RUNTIME_DATA_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/CheckRuntimeData.cmake")
file(WRITE "${PANDA_CHECK_RUNTIME_DATA_SCRIPT}"
    "if(NOT EXISTS \"${PANDA_GAME_RUNTIME_DATA_DIR}/project.bin\")\n"
    "    message(FATAL_ERROR \"Cooked runtime data not found. Run Export Game in PandaEditor before packaging: ${PANDA_GAME_RUNTIME_DATA_DIR}\")\n"
    "endif()\n"
)
add_custom_target(PandaCheckRuntimeData
    COMMAND ${CMAKE_COMMAND} -P "${PANDA_CHECK_RUNTIME_DATA_SCRIPT}"
    COMMENT "Checking cooked runtime data"
)
add_dependencies(liberate_the_sheep PandaCheckRuntimeData)

# Copy engine resources and cooked game runtime data into fixed runtime locations
if(PLATFORM_MACOS)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:liberate_the_sheep>/../Resources")
elseif(PLATFORM_IOS)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:liberate_the_sheep>")
elseif(PLATFORM_LINUX OR PLATFORM_WIN32)
    set(APP_RESOURCES_OUTPUT_DIR "$<TARGET_FILE_DIR:liberate_the_sheep>")
endif()

if(PLATFORM_MACOS OR PLATFORM_IOS OR PLATFORM_LINUX OR PLATFORM_WIN32)
    set(RUNTIME_DATA_OUTPUT_DIR "${APP_RESOURCES_OUTPUT_DIR}/.Panda")
    add_custom_command(TARGET liberate_the_sheep POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${APP_RESOURCES_OUTPUT_DIR}/Content"
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${RUNTIME_DATA_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PandaSDK_ENGINE_COMMON_RESOURCES_DIR}"
            "${APP_RESOURCES_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PandaSDK_ENGINE_SHADER_RESOURCES_DIR}"
            "${APP_RESOURCES_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RUNTIME_DATA_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PANDA_GAME_RUNTIME_DATA_DIR}"
            "${RUNTIME_DATA_OUTPUT_DIR}"
        COMMENT "Copying cooked runtime data to app bundle"
    )
endif()

if(APPLE)
    add_custom_command(TARGET liberate_the_sheep POST_BUILD
        COMMAND /usr/bin/xattr -cr "$<TARGET_BUNDLE_DIR:liberate_the_sheep>"
        COMMENT "Removing extended attributes from app bundle"
    )
endif()
