//
// Created by cleme on 2020-02-03.
//

#ifndef GAME_ENGINE_MODEL_HPP
#define GAME_ENGINE_MODEL_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "Vertex.hpp"
#include "Application.hpp"
#include "Texture.hpp"

class Application;
class Mesh;
class Texture;

struct Mesh{
    std::vector<Texture> textures;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class Model {
private:
    Application *application;
    VkDevice device;
    Mesh mesh;
    glm::mat4 modelMatrix;
    //One descriptor set for each image in the buffer and one for each textures

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    void loadModel(std::string path);
    void createDescriptorSets();

public:
    Model(Application *application, VkDevice &device, glm::vec3 position = glm::vec3(0.0f));
    void cleanup();
    void nextStep(float time);
    void init();

    std::vector<Vertex> getVertices();
    std::vector<uint32_t> getIndices();
    VkDescriptorSet getDescriptorSet(uint32_t i);

    glm::mat4 getModelMatrix();
};


#endif //GAME_ENGINE_MODEL_HPP
