

set(SOURCE_FILES
        EncryptionProvider.cpp
        FileTransferAgent.cpp
        SnowflakeS3Client.cpp
        StorageClientFactory.cpp
        )

add_library(snowflakeclientcpp STATIC ${SOURCE_FILES})

target_link_libraries(snowflakeclient)

