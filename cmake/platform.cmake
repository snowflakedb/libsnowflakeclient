
if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

if (LINUX)
    set(PLATFORM linux)
    message("Platform: Linux")
endif ()

if (APPLE)
    set(PLATFORM darwin)
    message("Platform: Apple OSX")
endif ()

if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(PLATFORM win64)
    message("Platform: Windows 64bit")
endif ()

if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set(PLATFORM win32)
    message("Platform: Windows 32bit")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set (WIN32_DEBUG ON)
        message("WIN32_DEBUG: ${WIN32_DEBUG}")
    endif ()
endif ()
