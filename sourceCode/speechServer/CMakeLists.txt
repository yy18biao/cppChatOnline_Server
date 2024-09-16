# 声明所需Cmake版本
cmake_minimum_required(VERSION 3.1.3)

# 声明项目工程名称
project(speechServer)

# 声明目标文件名称
set(target "speechServer")
set(test "speechClient")

set(protoPath ${CMAKE_CURRENT_SOURCE_DIR}/../proto/) # 添加所需的proto源文件路径
set(protoFile speech.proto) # 添加所需的proto映射代码源文件名称
set(protoH speech.pb.h) # proto所映射的.h文件名称
set(protoC speech.pb.cc) # proto所映射的.cc文件名称

# 生成的所需框架代码在执行cmake命令的目录下
if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}${protoC})
    # 如果没有生成则预定义生成指令
    add_custom_command(
        PRE_BUILD
        COMMAND protoc
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I ${protoPath} --experimental_allow_proto3_optional ${protoPath}/${protoFile}
        DEPENDS ${protoPath}/${protoFile}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${protoC}
        COMMENT "生成Protobuf框架代码文件:" ${CMAKE_CURRENT_BINARY_DIR}/${protoC}
    )
endif()

# 获取源码目录下的所有源码文件
set(srcFiles "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/source srcFiles)

# 获取源码目录下的所有源码文件
set(testFiles "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/test testFiles)

# 声明目标及依赖
add_executable(${target} ${srcFiles} ${protoC})
add_executable(${test} ${testFiles} ${protoC})

# 设置需要链接的库
target_link_libraries(${target} -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl /usr/local/lib/libjsoncpp.so.1.8.4)
target_link_libraries(${test} -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl /usr/local/lib/libjsoncpp.so.1.8.4)

# 设置头文件默认搜索路径
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/mysql)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../thirdInclude/speechApi)

# 设置安装路径
INSTALL(TARGETS ${target} RUNTIME DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)