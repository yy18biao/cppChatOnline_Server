# 声明所需Cmake版本
cmake_minimum_required(VERSION 3.1.3)

# 声明项目工程名称
project(main)

# 添加odb映射文件的路径 根据CMakeLists.txt文件所做相对路径
set(odbPath ${CMAKE_CURRENT_SOURCE_DIR}/entity)

# 添加所需的odb映射文件名
set(odbFiles student.hxx)
set(odbHxx "")
set(odbCxx "")
set(odbAll "")

# 检测所需框架代码文件是否已经生成
foreach(odbFile ${odbFiles})
    # 所需框架代码文件前缀一致，则用替换后缀生成新变量方便处理
    string(REPLACE ".hxx" "-odb.hxx" odbHxx ${odbFile})
    string(REPLACE ".hxx" "-odb.cxx" odbCxx ${odbFile})

    # 生成的所需框架代码在执行cmake命令的目录下
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${odbCxx})
        # 如果没有生成则预定义生成指令
        add_custom_command(
            PRE_BUILD
            COMMAND odb
            ARGS -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time ${odbPath}/${odbFile}
            DEPENDS ${odbPath}/${odbFile}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${odbCxx}
            COMMENT "生成ODB所需框架代码文件: " ${CMAKE_CURRENT_BINARY_DIR}/${odbCxx}
        )
    endif()

    # 将所有生成的框架文件名保存
    list(APPEND odbAll ${CMAKE_CURRENT_BINARY_DIR}/${odbCxx})
endforeach()

# 获取所有的源码目录下的文件名
set(srcFiles "")
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/source srcFiles)

# 声明目标文件和依赖
add_executable(main ${srcFiles} ${odbAll})

# 设置头文件默认搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/entity)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# 设置需要链接的库
target_link_libraries(main -lodb-mysql -lodb -lodb-boost -lgflags)

# ########################################## 操作详情 ##################################

# # 声明所需Cmake版本
# cmake_minimum_required(VERSION 3.0)

# # 声明项目工程名称
# project(main)

# # 定义变量
# set(target "main")
# set(src "")

# # 列表定义
# list(APPEND src main.cc)

# # 设置生成目标
# add_executable(target main.cc)

# # cmake命令执行时的路径
# CMAKE_CURRENT_BINARY_DIR

# # CMakeLists文件的所在路径
# CMAKE_CURRENT_SOURCE_DIR

# # 默认的安装路径
# CMAKE_INSTALL_PREFIX()

# # 设置安装路径
# # 可以在默认的安装路径中设置子路径
# INSTALL()

# # 字符串替换，将src中的所有文件名中有.cc的替换为.o之后放到 new 中
# string(REPLACE ".cc" ".o" new ${src})

# # 添加头文件路径，参数根据cmake命令执行时的路径或者CMakeLists文件的所在路径的相对路径
# include_directories()

# # 添加链接库(动态库)，参数一为目标文件，后面为 ...
# target_link_libraries(main -ljsoncpp)

# # 错误提示
# # STATUS 普通提示
# # FATAL_ERROR 致命错误会退出cmake
# message()

# # 源文件查找 .cc .c .cxx .cpp ...
# # 参数一为指定目录查找将查找到的所有符合文件名放到参数二中
# aux_source_directory()

# # 执行外部指令
# # PRE_BUILD 项目执行之前先执行自定义命令
# # COMMAND 需要执行的指令名称
# # ARGS 需要执行指令的参数选项
# # DEPENDS 指定命令的依赖项
# # OUTPUT 指定要生成的目标名称
# # COMMENT 执行命令时要打印的内容
# add_custom_command()

# # 添加嵌套子 cmake 目录
# add_subdirectory()