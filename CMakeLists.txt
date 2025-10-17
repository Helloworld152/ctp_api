cmake_minimum_required(VERSION 3.10)
project(CTP_API)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 头文件目录
include_directories(include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 添加可执行文件
add_executable(md_client src/md_client.cpp)

# 链接库
target_link_libraries(md_client 
    thostmduserapi_se
    thosttraderapi_se
    pthread
)