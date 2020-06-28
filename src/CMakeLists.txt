if(STATIC)
  message(STATUS "Initiating static build")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  if(WIN32)
    add_definitions(-DCURL_STATICLIB)
  endif()
endif()

if(APPLE AND NOT OPENSSL_ROOT_DIR)
    EXECUTE_PROCESS(COMMAND brew --prefix openssl OUTPUT_VARIABLE OPENSSL_ROOT_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(hlsdl main.c aes_openssl.c curl.c hls.c misc.c msg.c mpegts.c)

target_include_directories(hlsdl PRIVATE ${OPENSSL_INCLUDE_DIR} ${CURL_INCLUDE_DIRS})

if(WIN32)
    target_sources(hlsdl PRIVATE "win/memmem.c")
endif()

target_link_libraries(hlsdl PRIVATE OpenSSL::Crypto CURL::libcurl Threads::Threads)

if(STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
  if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
  endif()
endif()

set_target_properties(hlsdl PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(hlsdl PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
