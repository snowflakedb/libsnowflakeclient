cmake_minimum_required(VERSION 3.17)
include(FindPackageHandleStandardArgs)
include(${CMAKE_CURRENT_LIST_DIR}/Utils.cmake)

if (LINUX)
    set(AWSLIBBIT "lib64")
elseif(APPLE)
    set(AWSLIBBIT "lib")
endif()

find_library(AWS_CORE_LIB libaws-cpp-sdk-core.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_S3_LIB libaws-cpp-sdk-s3.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_AUTH_LIB libaws-c-auth.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_CAL_LIB libaws-c-cal.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_COMMON_LIB libaws-c-common.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_COMPRESSION_LIB libaws-c-compression.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_EVENT_STREAM_LIB libaws-c-event-stream.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_CHECKSUMS_LIB libaws-checksums.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_HTTP_LIB libaws-c-http.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_IO_LIB libaws-c-io.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_MQTT_LIB libaws-c-mqtt.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_CRT_CPP_LIB libaws-crt-cpp.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_S3_LIB libaws-c-s3.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_library(AWS_C_SDKUTILS_LIB libaws-c-sdkutils.a PATHS ${DEPS_DIR}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
find_path(AWSSDK_INCLUDE_DIR aws/core/Aws.h PATHS ${DEPS_DIR}/aws/include/ NO_DEFAULT_PATH)

find_package_handle_standard_args(AWSSDK REQUIRED_VARS
    AWS_CORE_LIB
    AWS_S3_LIB
    AWS_C_AUTH_LIB
    AWS_C_CAL_LIB
    AWS_C_COMMON_LIB
    AWS_C_COMPRESSION_LIB
    AWS_C_EVENT_STREAM_LIB
    AWS_C_CHECKSUMS_LIB
    AWS_C_HTTP_LIB
    AWS_C_IO_LIB
    AWS_C_MQTT_LIB
    AWS_CRT_CPP_LIB
    AWS_C_S3_LIB
    AWS_C_SDKUTILS_LIB
    AWSSDK_INCLUDE_DIR
)

if (AWSSDK_FOUND)
    mark_as_advanced(AWSSDK_LIB)
    mark_as_advanced(AWSSDK_INCLUDE_DIR)
endif()

if (AWSSDK_FOUND AND NOT TARGET AWSSDK::AWSSDK)
    add_library(AWSSDK::AWSSDK STATIC IMPORTED)
    target_link_libraries(
        AWSSDK::AWSSDK
            INTERFACE ${AWS_S3_LIB}
            INTERFACE ${AWS_C_AUTH_LIB}
            INTERFACE ${AWS_C_CAL_LIB}
            INTERFACE ${AWS_C_COMMON_LIB}
            INTERFACE ${AWS_C_COMPRESSION_LIB}
            INTERFACE ${AWS_C_EVENT_STREAM_LIB}
            INTERFACE ${AWS_C_CHECKSUMS_LIB}
            INTERFACE ${AWS_C_HTTP_LIB}
            INTERFACE ${AWS_C_IO_LIB}
            INTERFACE ${AWS_C_MQTT_LIB}
            INTERFACE ${AWS_C_MQTT_LIB}
            INTERFACE ${AWS_CRT_CPP_LIB}
            INTERFACE ${AWS_C_S3_LIB}
            INTERFACE ${AWS_C_SDKUTILS_LIB}
            INTERFACE Curl::Curl
    )
    target_include_directories(AWSSDK::AWSSDK INTERFACE ${AWSSDK_INCLUDE_DIR})
endif ()
