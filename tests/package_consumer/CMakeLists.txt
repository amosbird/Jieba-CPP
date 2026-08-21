cmake_minimum_required(VERSION 3.20)
project(JiebaCPPConsumer LANGUAGES CXX)

find_package(JiebaCPP 1.0 REQUIRED)

add_executable(jiebacpp_consumer main.cpp)
target_link_libraries(jiebacpp_consumer PRIVATE JiebaCPP::jieba)
