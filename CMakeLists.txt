cmake_minimum_required(VERSION 3.23)
project(Chat)
set(CMAKE_CXX_STANDARD 23)

find_package(Boost 1.55 COMPONENTS system serialization REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

set(IMGUI_PATH "/home/shriller44/dev/c++/sdks/imgui")
set(IMGUI_BACKENDS_PATH "/home/shriller44/dev/c++/sdks/imgui/backends")
set(GLAD_PATH "/home/shriller44/dev/c++/sdks/glad")

include_directories(${OPENGL_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/src/connection ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR} ${IMGUI_PATH} ${IMGUI_PATH}/misc/cpp ./src)

add_executable(server src/server/server.cpp)
add_executable(client  src/client/client.cpp src/client/clientmain.cpp src/glad/glad.c)

file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
file(GLOB IMGUI_SOURCES_B ${IMGUI_BACKENDS_PATH}/*.cpp)

add_library(imgui STATIC ${IMGUI_SOURCES})
add_library(backend STATIC ${IMGUI_BACKENDS_PATH}/imgui_impl_glfw.cpp ${IMGUI_BACKENDS_PATH}/imgui_impl_opengl3.cpp)

target_link_libraries(client ${Boost_LIBRARIES} glfw imgui backend)
target_link_libraries(server ${Boost_LIBRARIES})
