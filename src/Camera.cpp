//
// Created by cleme on 2020-02-04.
//

#include <glm/trigonometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include "Camera.hpp"

Camera::Camera(GLFWwindow *window){
    this->window = window;
}


/**
 * @return the direction that the camera is facing
 */
glm::vec3 Camera::getDirection(){
    return glm::vec3(
            cos(this->cameraVerticalAngle) * sin(this->cameraHorizontalAngle),
            sin(this->cameraVerticalAngle),
            cos(this->cameraVerticalAngle) * cos(this->cameraHorizontalAngle)
            );
}

/**
 * @return the direction pointing to the right
 */
glm::vec3 Camera::getRightDirection(){
    return glm::vec3(
            sin(this->cameraHorizontalAngle - 3.14f/2.0f),
            0,
            cos(this->cameraHorizontalAngle - 3.14f/2.0f)
            );
}

/**
 * @return The direction pointing upward
 */
glm::vec3 Camera::getUpDirection() {
    return glm::cross(this->getRightDirection(), this->getDirection());
}

/**
 * @return Get the projection matrix
 */
glm::mat4 Camera::getProjectionMatrix() {
    return glm::perspective(glm::radians(this->initialFow), 4.0f / 3.0f, 0.1f, 100.0f);
}

/**
 * @return The view matrix
 */
glm::mat4 Camera::getViewMatrix() {
    this->computeMatricesFromInputs();

    return glm::lookAt(
                this->cameraWorldPos,
                this->cameraWorldPos + this->getDirection(),
                this->getUpDirection()
            );
}

void Camera::computeMatricesFromInputs(){
    static double lastTime = glfwGetTime();

    double currentTime = glfwGetTime();
    float deltaTime = float(currentTime - lastTime);

    double xpos, ypos;
    int width, height;
    glfwGetCursorPos(this->window, &xpos, &ypos);
    glfwGetWindowSize(this->window, &width, &height);
    glfwSetCursorPos(this->window, width/2.0f, height/2.0f);

    this->cameraHorizontalAngle += this->mouseSpeed * deltaTime * float(width/2 - xpos);
    this->cameraVerticalAngle += this->mouseSpeed * deltaTime * float(height/2 - ypos);

    glm::vec3 direction = this->getDirection();
    glm::vec3 right = this->getRightDirection();

    if(glfwGetKey(this->window, GLFW_KEY_W) == GLFW_PRESS){
        this->cameraWorldPos += direction * deltaTime * this->speed;
    }

    if(glfwGetKey(this->window, GLFW_KEY_S) == GLFW_PRESS){
        this->cameraWorldPos -= direction * deltaTime * this->speed;
    }

    if(glfwGetKey(this->window, GLFW_KEY_D) == GLFW_PRESS){
        this->cameraWorldPos += right * deltaTime * this->speed;
    }

    if(glfwGetKey(this->window, GLFW_KEY_A) == GLFW_PRESS){
        this->cameraWorldPos -= right * deltaTime * this->speed;
    }

    lastTime = currentTime;
}