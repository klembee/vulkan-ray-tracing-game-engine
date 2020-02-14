//
// Created by cleme on 2020-01-29.
//

#ifndef GAME_ENGINE_VERTEX_HPP
#define GAME_ENGINE_VERTEX_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <array>

struct VertexBoneData {
    uint32_t ids[4] = {0};
    float weights[4] = {0.0f};

    void addBoneData(uint32_t boneId, float weight){
        for(uint32_t i = 0 ; i < 4 ; i++){
            if(this->weights[i] == 0.0f){
                this->ids[i] = boneId;
                this->weights[i] = weight;
                return;
            }
        }
    }
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    uint32_t texId;
    glm::vec2 texCoord;
    glm::ivec4 boneIds;
    glm::vec4 boneWeights;

    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions(){
        std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions = {};

        //pos data
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        //Normal data
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        //Texture id data
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32_SINT;
        attributeDescriptions[2].offset = offsetof(Vertex, texId);

        //Texture coord data
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

        //Bone id data
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SINT;
        attributeDescriptions[4].offset = offsetof(Vertex, boneIds);

        //Bone weights data
        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[5].offset = offsetof(Vertex, boneWeights);

        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const{
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord && texId == other.texId;
    }

};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                     (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

#endif //GAME_ENGINE_VERTEX_HPP
