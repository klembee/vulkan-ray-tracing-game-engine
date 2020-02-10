//
// Created by cleme on 2020-02-03.
//

#include "Model.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../include/tiny_obj_loader.h"

Model::Model(Application *application, VkDevice &device, glm::vec3 position){
    this->application = application;
    this->device = device;

    this->loadModel("../models/elf/Elf01_posed.obj");

    this->modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)), position);
}

void Model::loadModel(std::string path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    const char *modelDir = path.substr(0, path.find_last_of('/')).c_str();

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), modelDir)){
        throw std::runtime_error(warn + err);
    }

    //One for each texture
    std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

    for(size_t i = 0 ; i < materials.size() ; i++){
        tinyobj::material_t *material = &materials[i];
        if(material->diffuse_texname.length() > 0){
            std::string materialPath = std::string(modelDir) + "/" + material->diffuse_texname;
            this->mesh.textures.push_back(Texture(this->application, this->device, materialPath));
        }
    }



    for(uint32_t i = 0 ; i < shapes.size() ; i++){
        tinyobj::shape_t &shape = shapes[i];

        printf("%d\n", shape.mesh.material_ids.size());
        printf("%d\n", shape.mesh.indices.size());
//        printf("%d\n", attrib.vertices[0].a );

        for(size_t i = 0 ; i  < shape.mesh.indices.size() ; i++){
            tinyobj::index_t indice = shape.mesh.indices[i];
            Vertex vertex = {};

            vertex.texId = shape.mesh.material_ids[i / 3];

            vertex.pos = {
                    attrib.vertices[3 * indice.vertex_index],
                    attrib.vertices[3 * indice.vertex_index + 1],
                    attrib.vertices[3 * indice.vertex_index + 2],
            };

            vertex.texCoord = {
                    attrib.texcoords[2 * indice.texcoord_index],
                    1.0f - attrib.texcoords[2 * indice.texcoord_index + 1],
            };


            vertex.color = {1.0f, 1.0f, 1.0f};

            if(uniqueVertices.count(vertex) == 0){
                uniqueVertices[vertex] = uniqueVertices.size();
                this->mesh.vertices.push_back(vertex);
            }

            this->mesh.indices.push_back(uniqueVertices[vertex]);
        }
    }
}

void Model::createDescriptorSets() {
    uint32_t nbFrameBuffers = application->getSwapChainImagesCount();

    //Create the descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[0].descriptorCount = nbFrameBuffers;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = nbFrameBuffers * 8;


    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(nbFrameBuffers);
    poolInfo.flags = 0;

    if(vkCreateDescriptorPool(this->device, &poolInfo, nullptr, &this->descriptorPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor pool.");
    }

    //Create the descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(nbFrameBuffers, application->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = this->descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    //swapchainImages
    this->descriptorSets.resize(nbFrameBuffers);
    if(vkAllocateDescriptorSets(this->device, &allocInfo, this->descriptorSets.data()) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate descriptor sets");
    }


    for(size_t i = 0 ; i < nbFrameBuffers; i++){
        VkDescriptorBufferInfo modelBufferInfo = {};
        modelBufferInfo.buffer = application->getModelUniformBuffer(i);
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = sizeof(glm::mat4);

        std::vector<VkDescriptorImageInfo> imageInfos;
        for(size_t i = 0 ; i < 8 ; i++){
            VkDescriptorImageInfo info = {};
            if(i < this->mesh.textures.size()) {
                Texture &texture = this->mesh.textures[i];

                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                info.imageView = texture.getImageView();
                info.sampler = texture.getTextureSampler();

            }else{
                Texture &texture = this->mesh.textures[0];
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                info.imageView = texture.getImageView();
                info.sampler = texture.getTextureSampler();
            }
            imageInfos.push_back(info);
        }

//        VkDescriptorImageInfo imageInfos = {};
//        imageInfos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//        imageInfos.imageView = mesh.textures[0].getImageView();
//        imageInfos.sampler = mesh.textures[0].getTextureSampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = this->descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &modelBufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;

//        printf("b: %d\n", imageInfos.size());

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = this->descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 8;
        descriptorWrites[1].pImageInfo = &imageInfos[0];


        vkUpdateDescriptorSets(this->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    }
}

void Model::init(){
    this->createDescriptorSets();
}

std::vector<Vertex> Model::getVertices() {
    return this->mesh.vertices;
}

std::vector<uint32_t> Model::getIndices() {
    return this->mesh.indices;
}

VkDescriptorSet Model::getDescriptorSet(uint32_t i){
    return this->descriptorSets[i];
}

glm::mat4 Model::getModelMatrix() {
    return this->modelMatrix;
}

void Model::nextStep(float time) {
    this->modelMatrix = glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(0.08f, 0.08f, 0.08f)), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}

void Model::cleanup() {
    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);

    for(Texture &texture : this->mesh.textures){
        texture.cleanup();
    }
}

