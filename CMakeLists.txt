cmake_minimum_required(VERSION 3.24)


include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(texgan VERSION 2.0.0 LANGUAGES CXX)


find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Stb REQUIRED)
find_package(cpr REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenCV REQUIRED)
find_package(assimp CONFIG REQUIRED)

# file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
#     src/*.cpp
#     src/*.h
# )


# add_executable(${PROJECT_NAME} ${SOURCES})
# add_executable(${PROJECT_NAME} src/attempt.cpp src/attemptDefinitions.cpp)
# add_executable(${PROJECT_NAME} src/model.cpp)
add_executable(${PROJECT_NAME} src/main.cpp src/definitions.cpp)


# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/src")


# Link libraries(3rd-party dependencies).
target_link_libraries(${PROJECT_NAME} PUBLIC 
    glfw GLEW::GLEW
    imgui::imgui glm::glm nlohmann_json::nlohmann_json
    cpr::cpr
    spdlog::spdlog
    assimp::assimp
    ${OpenCV_LIBS}
)

