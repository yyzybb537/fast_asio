cmake_minimum_required(VERSION 2.8)

###################################################################################
project(fast_asio_samples)

enable_language(C ASM)

if (CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
    #set(CMAKE_BUILD_TYPE DEBUG)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("  CMAKE_COMMAND: ${CMAKE_COMMAND}")

option(WITH_OPENSSL "compile with openssl" OFF)
if (WITH_OPENSSL)
    message ("  use openssl: yes")
else()
    message ("  use openssl: no")
endif()

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("----------------------------------")

if (UNIX)
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -m64 ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-g")
    set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG")
elseif (WIN32)
    # windows platform
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

message("------------ Cxx flags -------------")
message("  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message("------------------------------------")

# boost
find_package(Boost REQUIRED thread system)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

include_directories(${PROJECT_SOURCE_DIR})

if (UNIX)
    add_custom_target(debug
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=DEBUG ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Switch CMAKE_BUILD_TYPE to Debug"
    )

    add_custom_target(release
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=RELEASE ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Switch CMAKE_BUILD_TYPE to Release"
    )

    set(PROFILE_FLAGS "-pg ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")

    #message("PROFILE_FLAGS: ${PROFILE_FLAGS}")
    add_custom_target(profile
        COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=PROFILE -DCMAKE_CXX_FLAGS_PROFILE=\\'${PROFILE_FLAGS}\\' ${CMAKE_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Switch CMAKE_BUILD_TYPE to PROFILE"
    )
endif()

# samples
function(Exec target source)
    add_executable(${target} ${source})
    if (UNIX)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        #target_link_libraries(${target} -ltcmalloc -lunwind)
        target_link_libraries(${target} -pthread -ldl)
    elseif (WIN32)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        set_target_properties(${target} PROPERTIES COMPILE_FLAGS "/wd4819 /wd4267")
    endif()
endfunction()

# ssl
function(SSLExec target source)
    add_executable(${target} ${source})
    if (UNIX)
        target_include_directories(${target} SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/libquic/third_party/boringssl/src/include)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/ssl)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/crypto)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/decrepit)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        target_link_libraries(${target} ssl crypto decrepit)
        #target_link_libraries(${target} -ltcmalloc -lunwind)
        target_link_libraries(${target} -pthread -ldl)
    elseif (WIN32)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        set_target_properties(${target} PROPERTIES COMPILE_FLAGS "/wd4819 /wd4267")
    endif()
endfunction()

# quic
function(QuicExec target source)
    add_executable(${target} ${source})
    if (UNIX)
        target_include_directories(${target} SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/libquic)
        target_include_directories(${target} SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/libquic/third_party/boringssl/src/include)
        target_include_directories(${target} SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/libquic/third_party/protobuf/src)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/ssl)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/crypto)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/boringssl/src/decrepit)
        link_directories(${CMAKE_BINARY_DIR}/third_party/libquic/third_party/protobuf/src)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        target_link_libraries(${target} quic ssl crypto decrepit protobuf)
        #target_link_libraries(${target} -ltcmalloc -lunwind)
        target_link_libraries(${target} -pthread -ldl)
    elseif (WIN32)
        target_link_libraries(${target} ${Boost_LIBRARIES})
        set_target_properties(${target} PROPERTIES COMPILE_FLAGS "/wd4819 /wd4267")
    endif()
endfunction()

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libquic)

Exec(benchmark_client ${PROJECT_SOURCE_DIR}/benchmark/client.cc)
Exec(benchmark_server ${PROJECT_SOURCE_DIR}/benchmark/server.cc)
Exec(native_asio_client ${PROJECT_SOURCE_DIR}/benchmark/native_asio_client.cc)
Exec(native_asio_server ${PROJECT_SOURCE_DIR}/benchmark/native_asio_server.cc)
Exec(client ${PROJECT_SOURCE_DIR}/examples/client.cc)
Exec(server ${PROJECT_SOURCE_DIR}/examples/server.cc)
Exec(websocket_client ${PROJECT_SOURCE_DIR}/examples/websocket_client.cc)
Exec(websocket_server ${PROJECT_SOURCE_DIR}/examples/websocket_server.cc)

SSLExec(ssl_client ${PROJECT_SOURCE_DIR}/examples/ssl_client.cc)
SSLExec(ssl_server ${PROJECT_SOURCE_DIR}/examples/ssl_server.cc)
