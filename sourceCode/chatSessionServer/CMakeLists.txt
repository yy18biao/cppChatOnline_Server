# 声明所需Cmake版本
cmake_minimum_required(VERSION 3.1.3)

# 声明项目工程名称
project(chatSessionServer)

# 声明目标文件名称
set(target "chatSessionServer")

set(protoPath ${CMAKE_CURRENT_SOURCE_DIR}/../proto/) # 添加所需的proto源文件路径
set(protoFiles user.proto base.proto chatSession.proto) # 添加所需的proto映射代码源文件名称
set(protoH "") # proto所映射的.h文件名称
set(protoC "") # proto所映射的.cc文件名称
set(protoCs "") # proto所映射的全部.cc文件名称

set(odbPath ${CMAKE_CURRENT_SOURCE_DIR}/../odb/) # 添加所需的odb源文件路径
set(odbFiles chatSessionUser.hxx) # 添加所需的odb映射代码源文件名称
set(odbH "") # odb所映射的.hxx文件名称
set(odbC "") # odb所映射的.cxx文件名称
set(odbCs "") # odb所映射的全部.cxx文件名称

# 生成的所需框架代码在执行cmake命令的目录下
foreach(file ${protoFiles})
    string(REPLACE ".proto" ".pb.cc" protoC ${file})
    string(REPLACE ".proto" ".pb.h" protoH ${file})

    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}${protoC})
        # 如果没有生成则预定义生成指令
        add_custom_command(
            PRE_BUILD
            COMMAND protoc
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I ${protoPath} --experimental_allow_proto3_optional ${protoPath}/${file}
            DEPENDS ${protoPath}/${file}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${protoC}
            COMMENT "生成Protobuf框架代码文件:" ${CMAKE_CURRENT_BINARY_DIR}/${protoC}
        )
    endif()

    list(APPEND protoCs ${CMAKE_CURRENT_BINARY_DIR}/${protoC})
endforeach()

# 生成的所需框架代码在执行cmake命令的目录下
foreach(file ${odbFiles})
    string(REPLACE ".hxx" "-odb.cxx" odbC ${file})
    string(REPLACE ".proto" "-odb.hxx" odbH ${file})

    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}${odbC})
        # 如果没有生成则预定义生成指令
        add_custom_command(
            PRE_BUILD
            COMMAND odb
            ARGS -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time ${odbPath}/${file}
            DEPENDS ${odbPath}/${file}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${odbC}
            COMMENT "生成Protobuf框架代码文件:" ${CMAKE_CURRENT_BINARY_DIR}/${odbC}
        )
    endif()

    list(APPEND odbCs ${CMAKE_CURRENT_BINARY_DIR}/${odbC})
endforeach()

# 获取源码目录下的所有源码文件
set(srcFiles "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/source srcFiles)

# 声明目标及依赖
add_executable(${target} ${srcFiles} ${protoCs} ${odbCs})

# 设置需要链接的库
target_link_libraries(${target} -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl -ljsoncpp -lodb-mysql -lodb -lodb-boost -lcpr -lelasticlient -lamqpcpp -lev -lpthread -ldl)

# 设置头文件默认搜索路径
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common/mysql)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../odb)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../thirdInclude/speechApi)

# 测试
set(test "chatSessionClient")
set(testFiles "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/test testFiles)
add_executable(${test} ${testFiles} ${protoCs}  ${odbCs})
target_link_libraries(${test} -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl -ljsoncpp -lodb-mysql -lodb -lodb-boost -lcpr -lelasticlient -lamqpcpp -lev -lpthread -ldl)

# 设置安装路径
INSTALL(TARGETS ${target} RUNTIME DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)