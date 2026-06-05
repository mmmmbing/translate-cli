cmake_minimum_required(VERSION 3.14)
project(TranslateCLI VERSION 1.0 LANGUAGES CXX)

# 设置 C++17 标准 (cpr 和 nlohmann/json 都需要 C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# --- 关键步骤：引入第三方库 ---
include(FetchContent)

option(CPR_USE_SYSTEM_CURL "Use system curl" OFF)
option(CURL_USE_SCHANNEL "Use Windows native SSL backend" ON)

# 在 MinGW/UCRT64 + Windows 下, 默认的 windres 使用 popen 调用预处理器,
# 在某些环境下会因路径中的冒号或管道读取失败而崩溃. 强制改用 ucrt64 下的
# windres 并加上 --use-temp-file, 让其走临时文件路径.
if(MINGW AND WIN32)
    if(EXISTS "D:/env/CPP/ucrt64/bin/windres.exe")
        set(CMAKE_RC_COMPILER "D:/env/CPP/ucrt64/bin/windres.exe"
            CACHE FILEPATH "RC compiler" FORCE)
    endif()
    # 让 windres 使用临时文件, 避免 popen 兼容性问题
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --use-temp-file"
        CACHE STRING "RC compiler flags" FORCE)
endif()

# 关键修复: 在 MinGW / UCRT64 + Windows 下, curl 的 try_compile 测试
# 经常无法正确检测到 ioctlsocket+FIONBIO, 导致 nonblock.c 编译失败.
# 预先将这些 HAVE_* 标记写入 CACHE, 跳过 try_compile 测试.
# 这些值会被 cpr 内部依赖的 curl 子项目读取.
if(MINGW AND WIN32)
    set(HAVE_IOCTLSOCKET 1 CACHE INTERNAL "Defined for curl subproject")
    set(HAVE_IOCTLSOCKET_FIONBIO 1 CACHE INTERNAL "Defined for curl subproject")
    set(HAVE_WINSOCK2_H 1 CACHE INTERNAL "Defined for curl subproject")
    set(HAVE_WINDOWS_H 1 CACHE INTERNAL "Defined for curl subproject")
    set(HAVE_FCNTL_H 1 CACHE INTERNAL "Defined for curl subproject")
    set(HAVE_SYS_IOCTL_H 1 CACHE INTERNAL "Defined for curl subproject")
endif()

# 1. 引入 nlohmann/json (Header-only 库)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 2. 引入 cpr (HTTP 客户端)
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5 # 使用稳定版本，避免 master 分支的不稳定性
)
FetchContent_MakeAvailable(cpr)

# 3. 引入 CLI11 (命令行参数解析, Header-only)
FetchContent_Declare(
    cli11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.1.tar.gz
)
FetchContent_MakeAvailable(cli11)

# --- Patch zlib-ng windres (预存问题修复) ---
# cpr 拉来的 zlib-ng 子项目 (MINGW/MSYS/CYGWIN) 调 windres 时
# 直接把命令写死在 add_custom_command 里, 没有用 ${CMAKE_RC_FLAGS},
# 因此顶层 set(CMAKE_RC_FLAGS ... --use-temp-file) 注入对它无效.
# 在 UTF-8 代码页 (65001) 下 windres 默认通过 popen 调预处理器会失败:
#     windres.exe: can't open file `page:': Invalid argument
# 这里在 FetchContent 之后给 zlib-ng 的 windres 命令补 --use-temp-file,
# 走临时文件路径, 一劳永逸. patch 是幂等的, 已包含 --use-temp-file 时不再写入.
file(GLOB _zlib_cmakelists_paths "${CMAKE_BINARY_DIR}/_deps/zlib-src/CMakeLists.txt")
if(_zlib_cmakelists_paths)
    list(GET _zlib_cmakelists_paths 0 _zlib_cmakelists)
    file(READ "${_zlib_cmakelists}" _zlib_content)
    set(_zlib_search [==[COMMAND ${CMAKE_RC_COMPILER}
            -D GCC_WINDRES]==])
    set(_zlib_replace [==[COMMAND ${CMAKE_RC_COMPILER} --use-temp-file
            -D GCC_WINDRES]==])
    string(REPLACE
        "${_zlib_search}"
        "${_zlib_replace}"
        _zlib_patched
        "${_zlib_content}")
    if(NOT _zlib_content STREQUAL _zlib_patched)
        file(WRITE "${_zlib_cmakelists}" "${_zlib_patched}")
        message(STATUS "Patched zlib-ng CMakeLists.txt: windres +--use-temp-file")
    endif()
endif()

# --- 构建目标 ---
file(GLOB SOURCES "src/*.cpp")
add_executable(translate ${SOURCES})

# 将第三方库链接到你的可执行文件
# nlohmann_json::nlohmann_json 是 json 库的目标名
# cpr::cpr 是 cpr 库的目标名
# CLI11::CLI11 是命令行参数解析库
target_link_libraries(translate PRIVATE
    nlohmann_json::nlohmann_json
    cpr::cpr
    CLI11::CLI11
)

# --- 安装 / 打包 ---
# `translate.exe` 在 Windows 上依赖 cpr / curl / zlib 三个动态库
# (libcpr.dll / libcurl.dll / libzlib.dll). 它们在构建过程中被
# cpr/curl/zlib 子项目输出到 ${CMAKE_BINARY_DIR} (即 build 目录) 顶层.
#
# 由于 FetchContent 拉下来的 zlib 子项目把 install 路径硬编码成
#   C:/Program Files (x86)/TranslateCLI/...
# 在 Windows 上直接 `cmake --install build --prefix dist` 会被子项目
# install 阻断. 因此:
#   - 顶层 install 规则只注册一个名为 "translate_dist" 的 component,
#     负责把 translate.exe + 3 个 dll 复制到 ${CMAKE_INSTALL_PREFIX}/bin/.
#   - 完整发布请用 scripts/release.ps1, 它会直接 cp build/ 顶层
#     的 4 个文件到 dist/bin/, 完全绕开子项目 install, 简单稳定.
install(TARGETS translate
    RUNTIME DESTINATION bin
    COMPONENT translate_dist
)

if(WIN32)
    install(FILES
        ${CMAKE_BINARY_DIR}/libcpr.dll
        ${CMAKE_BINARY_DIR}/libcurl.dll
        ${CMAKE_BINARY_DIR}/libzlib.dll
        DESTINATION bin
        OPTIONAL
        COMPONENT translate_dist
    )
endif()

