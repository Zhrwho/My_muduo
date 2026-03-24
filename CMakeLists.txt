cmake_minimum_required(VERSION 3.6)
#要求的cmake版本

project(mymuduo)

# mymuduo最终编译生成so动态库，设置动态库路径，放在当前工作空间目录下的lib文件夹
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#设置调试信息，以及首选启动c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

#定义参与编译的源文件
aux_source_directory(. SRC_LIST)
#编译生成动态库mymuduo(SHARED：动态库)
add_library(mymuduo SHARED ${SRC_LIST})


