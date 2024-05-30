project(BA C CXX)

include_directories(${BA_SOURCE_DIR}/../include)

set(RUN_SOURCE
    ${BA_SOURCE_DIR}/Cell.cpp
    ${BA_SOURCE_DIR}/main.cpp
    )
add_executable(RUN ${RUN_SOURCE})

set(TEST_SOURCE
    ${BA_SOURCE_DIR}/test.cpp
    )
add_executable(TEST ${TEST_SOURCE})