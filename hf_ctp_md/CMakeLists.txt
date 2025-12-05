cmake_minimum_required(VERSION 3.10)
project(HF_CTP_MD)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 优化选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pthread -march=native")

# 头文件目录
include_directories(include)

# 库文件目录
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 源文件
file(GLOB SOURCES "src/*.cpp")

# 可执行文件
add_executable(hf_ctp_md ${SOURCES})

# 链接库
target_link_libraries(hf_ctp_md 
    thostmduserapi_se
    thosttraderapi_se
    pthread
    dl
)

# 将库文件复制到构建目录，方便运行
add_custom_command(TARGET hf_ctp_md POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib
)
