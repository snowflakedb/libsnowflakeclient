cmake_minimum_required(VERSION 3.17)
project(XD)

set(PLATFORM darwin)
set(CMAKE_BUILD_TYPE Debug)
find_package(Cmocka REQUIRED)
add_executable(test_cmocka test_cmocka.cpp)
target_link_libraries(test_cmocka Cmocka::Cmocka)
set_property(TARGET test_cmocka PROPERTY CXX_STANDARD 17)

find_package(Telemetry REQUIRED)
find_package(Curl REQUIRED)
find_package(AWSSDK REQUIRED)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(test_curl test_curl.cpp)
target_link_libraries(test_curl Curl::Curl Telemetry::Telemetry OpenSSL::SSL OpenSSL::Crypto)
set_property(TARGET test_curl PROPERTY CXX_STANDARD 17)



#set(CMAKE_PREFIX_PATH  "/Users/jszczerbinski/git/libsnowflakeclient/deps-build/darwin/Debug/aws/lib/cmake;/Users/jszczerbinski/git/libsnowflakeclient/deps-build/darwin/Debug/aws/lib")
#find_package(ZLIB REQUIRED)
#find_package(AWSSDK REQUIRED COMPONENTS s3)
#target_link_libraries(test aws-cpp-sdk-s3)