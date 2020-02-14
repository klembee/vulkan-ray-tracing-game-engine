//
// Created by cleme on 2020-02-07.
//

#ifndef GAME_ENGINE_MESH_HPP
#define GAME_ENGINE_MESH_HPP


#include "Texture.hpp"
#include "Application.hpp"

class Application;
class Texture;

class Mesh {
private:
    Application *application;
    VkDevice device;
public:
    Mesh();
    Mesh(Application *application, VkDevice &device);
    std::vector<Vertex>* getVertices();
    std::vector<uint32_t>* getIndices();
    std::vector<VkDescriptorSet>* getDescriptorSets();
    void setTexture(Texture &texture);
    Texture* getTexture();
    VkBuffer* getVertexBuffer();
    void cleanup();
    void createVertexBuffer();
};


#endif //GAME_ENGINE_MESH_HPP
