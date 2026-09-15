cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(guitar-kit)

# 按日期命名的成品: 每次构建额外产出 build/guitar-kit-YYYYMMDD.bin
# (与 guitar-kit.bin 同内容; 历史日期的文件保留, 便于回溯刷过的版本)。
string(TIMESTAMP GK_BUILD_DATE "%Y%m%d")
add_custom_target(gk_dated_bin ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/guitar-kit.bin"
            "${CMAKE_BINARY_DIR}/guitar-kit-${GK_BUILD_DATE}.bin"
    DEPENDS gen_project_binary
)
