cmake_minimum_required(VERSION 3.5)
project(proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -DMUDUO_STD_STRING")

include_directories("./")
#set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB MUDUO_SRCS
        muduo/base/*.cc
        muduo/base/*.h
        muduo/net/*.cc
        muduo/net/*.h
        muduo/net/poller/*.cc
        muduo/net/poller/*.h
        )

file(GLOB KCPPROXY_SRCS
        KcpProxy/*.cpp
        KcpProxy/*.h
        )

set(SOURCE_FILES
        KcpProxy/main.cpp
        kcp/ikcp.c
        kcp/ikcp.h
        ${MUDUO_SRCS}
        ${KCPPROXY_SRCS} KcpProxy/Option.h)

add_executable(proxy ${SOURCE_FILES})

target_link_libraries(proxy pthread)

file(GLOB KCPSERVER_SRCS
        kcp/*.c
        kcp/*.h
        ${MUDUO_SRCS}
        ./*.cpp
        ./*.h
        )

add_executable(server ${KCPSERVER_SRCS}
        KcpServer/KcpServer.cpp
        KcpServer/KcpServer.h
        KcpServer/main.cpp
        KcpServer/KcpServerSession.cpp KcpServer/KcpServerSession.h KcpServer/Option.h)
target_link_libraries(server pthread)

add_executable(echoserver
        ${MUDUO_SRCS}
        EchoServer/main.cpp
        )

target_link_libraries(echoserver pthread)


add_executable(echoclient
        ${MUDUO_SRCS}
        EchoClient/main.cpp
        )

target_link_libraries(echoclient pthread)

add_executable(downcli ${MUDUO_SRCS} DownloadTest/DownloadClient.cpp)
target_link_libraries(downcli pthread)

add_executable(downsvr ${MUDUO_SRCS} DownloadTest/DownloadServer.cpp)
target_link_libraries(downsvr pthread)