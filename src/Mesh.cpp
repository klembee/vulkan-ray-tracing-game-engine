//
// Created by cleme on 2020-02-07.
//

#include "Mesh.hpp"

Mesh::Mesh(){

}

Mesh::Mesh(Application *application, VkDevice &device){
    this->application = application;
    this->device = device;
}

std::vector<Vertex>* Mesh::getVertices(){
    return &this->vertices;
}

std::vector<uint32_t>* Mesh::getIndices(){
    return &this->indices;
}

std::vector<VkDescriptorSet>* Mesh::getDescriptorSets(){
    return &this->descriptorSets;
}

VkBuffer* Mesh::getVertexBuffer(){
    return &this->vertexBuffer;
}

void Mesh::setTexture(Texture &texture) {
    this->texture = &texture;
}

Texture* Mesh::getTexture(){
    return this->texture;
}

void Mesh::cleanup(){
    this->texture->cleanup();
    vkDestroyBuffer(this->device, this->vertexBuffer, nullptr);
    vkFreeMemory(this->device, this->vertexBufferMemory, nullptr);
}
