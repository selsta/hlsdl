cmake_minimum_required(VERSION 3.21)

project(hlsdl VERSION 0.27 LANGUAGES C)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_executable(hlsdl
    src/main.c
    src/aes_openssl.c
    src/curl.c
    src/hls.c
    src/misc.c
    src/msg.c
    src/mpegts.c
)

target_compile_features(hlsdl PRIVATE c_std_11)
target_link_libraries(hlsdl PRIVATE CURL::libcurl OpenSSL::Crypto)

if(MSVC)
    find_package(PThreads4W CONFIG REQUIRED)
    target_link_libraries(hlsdl PRIVATE PThreads4W::PThreads4W)
    target_sources(hlsdl PRIVATE src/compat/getopt.c)
    target_compile_definitions(hlsdl PRIVATE
        HAVE_STRUCT_TIMESPEC
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )
else()
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    if(NOT CMAKE_USE_PTHREADS_INIT)
        message(FATAL_ERROR "hlsdl requires POSIX threads")
    endif()
    target_link_libraries(hlsdl PRIVATE Threads::Threads)
    target_compile_definitions(hlsdl PRIVATE _GNU_SOURCE=1)
    target_compile_options(hlsdl PRIVATE
        -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
        -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
    )
endif()

if(WIN32)
    target_sources(hlsdl PRIVATE src/compat/memmem.c)
endif()

include(GNUInstallDirs)
install(TARGETS hlsdl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES hlsdl.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
