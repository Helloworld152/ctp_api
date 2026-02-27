cmake_minimum_required(VERSION 3.10)
project(RohonTraderAuthDemo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 头文件目录
include_directories(include)

# 库文件目录
link_directories(lib)

# 添加可执行文件
add_executable(trader_auth_demo trader_auth_demo.cpp)

# 链接库
target_link_libraries(trader_auth_demo 
    thosttraderapi_se
    rohonbase
    LinuxDataCollect
    pthread
)
