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
#include <assimp/scene.h>
#include <assimp/matrix4x4.h>
#include <map>
#include <assimp/Importer.hpp>

class Application;
class Texture;

struct BoneInfo{
    aiMatrix4x4 finalTransformation = aiMatrix4x4();
    aiMatrix4x4 boneOffset = aiMatrix4x4();
};

class Model {
private:
    Application *application;
    Assimp::Importer importer;
    VkDevice device;
    const aiScene *scene;

    aiMatrix4x4 globalInverseTransform;

    std::vector<Texture> textures;

    glm::mat4 modelMatrix;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    //Bones
    std::map<std::string, uint32_t> boneMapping;
    std::vector<BoneInfo> boneInfos;
    std::vector<VertexBoneData> bones;
    uint32_t numberOfBones = 0;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    void loadModel(std::string path);
    void createDescriptorSets();
    void readNodeHierarchy(float animationTime, const aiNode* pNode, const aiMatrix4x4& parentTransform);

public:
    Model(Application *application, VkDevice &device, glm::vec3 position = glm::vec3(0.0f));
    void cleanup();
    void init();

    VkDescriptorSet* getDescriptorSet(uint32_t i);
    std::vector<Vertex> getVertices();
    std::vector<uint32_t> getIndices();

    glm::mat4 getModelMatrix();
    void getBoneTransforms(float timeInSeconds, std::vector<glm::mat4> &transforms);
};


#endif //GAME_ENGINE_MODEL_HPP
