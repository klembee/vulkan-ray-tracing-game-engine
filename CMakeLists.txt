cmake_minimum_required(VERSION 3.15)
project(game_engine)

set(CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(INCLUDES
        include/helper/FileHelper.hpp
        src/Application.hpp
        include/stb_image.h
        include/tiny_obj_loader.h
        src/Vertex.hpp
        src/Model.hpp
        src/Texture.hpp
        src/Camera.hpp
        )

set(SOURCES
        src/Application.cpp
        src/main.cpp
        src/Model.cpp
        src/Texture.cpp
        src/Camera.cpp)


add_executable(game_engine ${INCLUDE} ${SOURCES})

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

find_package(assimp REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${ASSIMP_INCLUDE_DIRS})
set(ASSIMP ${ASSIMP_LIBRARY_DIRS}/${ASSIMP_LIBRARIES})
set(ASSIMP_DLL ${ASSIMP_ROOT_DIR}/bin/libassimp.dll)
target_link_libraries(${PROJECT_NAME} ${ASSIMP})
FILE(COPY ${ASSIMP_DLL} DESTINATION "${CMAKE_BINARY_DIR}/")

FILE(COPY textures DESTINATION "${CMAKE_BINARY_DIR}/")
FILE(COPY models DESTINATION "${CMAKE_BINARY_DIR}/")
configure_file(shaders/build/fragment.spv "${CMAKE_BINARY_DIR}/shaders/build/" COPYONLY)
configure_file(shaders/build/vertice.spv "${CMAKE_BINARY_DIR}/shaders/build/" COPYONLY)

target_link_libraries(game_engine ${ASSIMP} glfw3)