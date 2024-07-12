project(BA C CXX)

include_directories(${BA_SOURCE_DIR}/../include)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${BA_SOURCE_DIR}/../cmake")

# Find the SDL2 package
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Include SDL2 directories
include_directories(${SDL2_INCLUDE_DIR}
                    ${SDL2_IMAGE_INCLUDE_DIR}
                    ${SDL2_TTF_INCLUDE_DIR})

# Debug messages to print library paths
# message(STATUS "SDL2 include directory: ${SDL2_INCLUDE_DIRS}")
# message(STATUS "SDL2 libraries: ${SDL2_LIBRARIES}")

set(RUN_SOURCE
    ${BA_SOURCE_DIR}/Bin.cpp
    ${BA_SOURCE_DIR}/Cell.cpp
    ${BA_SOURCE_DIR}/Comb.cpp
    ${BA_SOURCE_DIR}/FF.cpp
    ${BA_SOURCE_DIR}/Net.cpp
    ${BA_SOURCE_DIR}/Pin.cpp
    ${BA_SOURCE_DIR}/Site.cpp
    ${BA_SOURCE_DIR}/Solver.cpp
    ${BA_SOURCE_DIR}/Renderer.cpp
    ${BA_SOURCE_DIR}/Legalizer.cpp
    ${BA_SOURCE_DIR}/main.cpp
    )
add_executable(RUN ${RUN_SOURCE})

target_link_libraries(RUN ${SDL2_LIBRARY}
                             ${SDL2_IMAGE_LIBRARIES}
                             ${SDL2_TTF_LIBRARIES})
