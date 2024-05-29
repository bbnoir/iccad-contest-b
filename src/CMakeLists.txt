project(BA C CXX)

include_directories(${BA_SOURCE_DIR}/../include)

set(RUN_SOURCE
    ${BA_SOURCE_DIR}/Cell.cpp
    ${BA_SOURCE_DIR}/main.cpp
    )
add_executable(RUN ${RUN_SOURCE})
target_link_libraries(RUN PRIVATE compiler_flags)

set(TEST_SOURCE
    ${BA_SOURCE_DIR}/test.cpp
    )
add_executable(TEST ${TEST_SOURCE})
target_link_libraries(TEST PRIVATE compiler_flags)