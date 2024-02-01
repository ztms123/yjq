cmake_minimum_required (VERSION 3.5)

project (SeastarExample)

SET(TARGET_NAME "my_web.exe")

# find_package (Seastar REQUIRED)

aux_source_directory(./src CPP_SRC)

add_executable (${TARGET_NAME} ${CPP_SRC} )

# target_link_libraries (${TARGET_NAME}
#   PRIVATE Seastar::seastar)
target_link_libraries (${TARGET_NAME})