
if (UNIX)
    # Linux and OSX
    if (USE_EXTRA_WARNINGS)
        add_compile_options(-Wextra -Wall)
    else ()
        add_compile_options(-Werror -Wno-error=deprecated-declarations)
        if ("$ENV{GCCVERSION}" STRGREATER "9")
            add_compile_options(-Wno-error=unused-result)
        endif()
    endif ()
else()
    # Windows
    add_compile_options(/ZH:SHA_256 /guard:cf /Qspectre /sdl)
    if ($ENV{ARROW_FROM_SOURCE})
        add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING BOOST_ALL_NO_LIB)
    endif()
endif()

if (LINUX)
    # Profiler for Linux
    if (NOT "$ENV{BUILD_WITH_PROFILE_OPTION}" STREQUAL "")
        add_compile_options(-pg)
        add_link_options(-pg)
    endif ()

    # Code coverage for Linux
    if (CLIENT_CODE_COVERAGE)    # Only when code coverage is enabled
        message("Code coverage is enabled CLIENT_CODE_COVERAGE=" ${CLIENT_CODE_COVERAGE})
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage -O0 $<$<COMPILE_LANGUAGE:CXX>:-fno-elide-constructors> -fno-inline -fno-inline-small-functions -fno-default-inline)
        add_link_options(--coverage)
    else()
        message("Code coverage is disabled CLIENT_CODE_COVERAGE=" ${CLIENT_CODE_COVERAGE})
    endif ()

    # Enable mocks
    if (MOCK)
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap=http_perform")
        add_definitions(-DMOCK_ENABLED)
    endif ()
endif ()
