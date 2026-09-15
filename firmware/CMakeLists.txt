cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 固件版本号 = 构建日期 + git 短哈希(如 20260915-4753467),
# 开机日志 "App version:" 可见; 无法读 git 时退化为纯日期。
string(TIMESTAMP GK_DATE "%Y%m%d")
execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/.." describe --always --dirty --abbrev=7
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE GK_GIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GK_GIT_RC
)
if(GK_GIT_RC EQUAL 0 AND GK_GIT)
    set(PROJECT_VER "${GK_DATE}-${GK_GIT}")
else()
    set(PROJECT_VER "${GK_DATE}")
endif()

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
