//
// Created by cleme on 2020-02-04.
//

#ifndef GAME_ENGINE_CAMERA_HPP
#define GAME_ENGINE_CAMERA_HPP


#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <GLFW/glfw3.h>

class Camera {
private:
    GLFWwindow *window;

    glm::vec3 cameraWorldPos = glm::vec3(0.0f, 5.0f, 0.0f);
    float cameraHorizontalAngle = 3.14f;
    float cameraVerticalAngle = 0.0f;
    float initialFow = 45.0f;
    float speed = 3.5f;
    float mouseSpeed = 0.1f;

    glm::vec3 getDirection();
    glm::vec3 getRightDirection();
    glm::vec3 getUpDirection();

    void computeMatricesFromInputs();
public:
    Camera(GLFWwindow *window);
    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();
};


#endif //GAME_ENGINE_CAMERA_HPP
