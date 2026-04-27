cmake_minimum_required(VERSION 3.20)

project(neopeak_lvgl_preview LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_BUILD_LVGL_H_SIMPLE_INCLUDE ON CACHE BOOL "" FORCE)
set(LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

find_package(SDL2 CONFIG REQUIRED)

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.3.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(lvgl)

target_link_libraries(lvgl PUBLIC SDL2::SDL2)

add_executable(neopeak_lvgl_preview
    main.cpp
    ../../components/ui/imu_cube_ui.cpp
    ../../components/ui/ui_app.cpp
)

target_include_directories(neopeak_lvgl_preview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "../../components/ui"
)

target_compile_definitions(neopeak_lvgl_preview PRIVATE
    SDL_MAIN_HANDLED
)

target_link_libraries(neopeak_lvgl_preview PRIVATE
    lvgl::lvgl
    SDL2::SDL2
)
