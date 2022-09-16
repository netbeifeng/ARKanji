cmake_minimum_required(VERSION 3.6)

SET(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE)
SET(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE PATH "Path to your vcpkg")

project(ARKanji)

set(CMAKE_CXX_STANDARD 14)  
find_package(OpenCV REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(soil2 CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(FTGL CONFIG REQUIRED)
find_library(GLFW3_LIBRARY glfw3dll)

set(ARKanji_SOURCES 
        PoseEstimation.cpp
        Main.cpp
        Tracker.cpp
)

set(ARKanji_HEADERS 
        PoseEstimation.h
        Util.h
        Model.h
        Tracker.h
        MetaManager.h
)

add_executable(ARKanji ${ARKanji_SOURCES} ${ARKanji_HEADERS})

link_directories( ${OpenCV_INCLUDE_DIRS} )
target_include_directories(ARKanji PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ARKanji assimp::assimp)
target_link_libraries(ARKanji libtesseract)
target_link_libraries(ARKanji ${OpenCV_LIBS})
target_link_libraries(ARKanji jsoncpp_lib jsoncpp_object JsonCpp::JsonCpp)
target_link_libraries(ARKanji glm::glm)
target_link_libraries(ARKanji GLEW::GLEW)
target_link_libraries(ARKanji ${GLFW3_LIBRARY})
target_link_libraries(ARKanji soil2)
target_link_libraries(ARKanji ftgl)