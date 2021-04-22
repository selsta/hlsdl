cmake_minimum_required(VERSION 3.5)

project(hlsdl VERSION 0.27 LANGUAGES C)

if (APPLE AND NOT IOS)
  if (NOT OPENSSL_ROOT_DIR)
    EXECUTE_PROCESS(COMMAND brew --prefix openssl
      OUTPUT_VARIABLE OPENSSL_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(hlsdl main.c aes_openssl.c curl.c hls.c misc.c msg.c mpegts.c)

target_include_directories(hlsdl PRIVATE ${OPENSSL_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})

if (WIN32)
    target_sources(hlsdl PRIVATE "../msvc/win/memmem.c")
endif()

target_link_libraries(hlsdl PRIVATE OpenSSL::Crypto CURL::libcurl Threads::Threads)
