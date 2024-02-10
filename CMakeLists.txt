cmake_minimum_required (VERSION 3.5)

project (SeastarExample)

SET(TARGET_NAME "my_web.exe")

# find_package (Seastar REQUIRED)

aux_source_directory(./http HTTP)
aux_source_directory(./locker LOCKER)
aux_source_directory(./threadpool THREADPOOL)
aux_source_directory(. MAIN)

add_executable (${TARGET_NAME} ${CPP_SRC} ${LOCKER} ${THREADPOOL} ${MAIN})

# target_link_libraries (${TARGET_NAME}
#   PRIVATE Seastar::seastar)
target_link_libraries (${TARGET_NAME})