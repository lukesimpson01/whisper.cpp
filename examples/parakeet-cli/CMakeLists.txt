set(TARGET parakeet-cli)
add_executable(${TARGET} parakeet-cli.cpp)

include(DefaultTargetOptions)

target_link_libraries(${TARGET} PRIVATE common parakeet ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${TARGET} RUNTIME)

set(DAEMON_TARGET parakeet-daemon)
add_executable(${DAEMON_TARGET} parakeet-daemon.cpp)
target_link_libraries(${DAEMON_TARGET} PRIVATE common parakeet ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS ${DAEMON_TARGET} RUNTIME)
