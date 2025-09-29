cmake_minimum_required(VERSION 3.10)
project(BufferTests)

# 查找GTest
find_package(GTest REQUIRED)

# 添加测试可执行文件
add_executable(buffer_test buffer_test.cpp ../src/buffer.cpp)

# 链接GTest
target_link_libraries(buffer_test GTest::GTest GTest::Main pthread)

# 添加测试
enable_testing()
add_test(NAME BufferTests COMMAND buffer_test)