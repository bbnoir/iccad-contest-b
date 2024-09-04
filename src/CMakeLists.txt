project(BA C CXX)

include_directories(${BA_SOURCE_DIR}/../include)

set(RUN_SOURCE
    ${BA_SOURCE_DIR}/Bin.cpp
    ${BA_SOURCE_DIR}/Cell.cpp
    ${BA_SOURCE_DIR}/Comb.cpp
    ${BA_SOURCE_DIR}/FF.cpp
    ${BA_SOURCE_DIR}/Pin.cpp
    ${BA_SOURCE_DIR}/Site.cpp
    ${BA_SOURCE_DIR}/Solver.cpp
    ${BA_SOURCE_DIR}/LegalPlacer.cpp
    ${BA_SOURCE_DIR}/main.cpp
    )
add_executable(RUN ${RUN_SOURCE})

