cmake_minimum_required(VERSION 3.16)

# A stand-in for a third-party consumer: it knows nothing about this repository
# except the installed package.  The `consumer` CI job builds it against a real
# install prefix, which is the only reliable way to catch a broken export().
project(logforge_consumer LANGUAGES CXX)

find_package(logforge 0.1 REQUIRED)

add_executable(logforge_consumer main.cpp)
target_link_libraries(logforge_consumer PRIVATE logforge::core)
target_compile_features(logforge_consumer PRIVATE cxx_std_11)
