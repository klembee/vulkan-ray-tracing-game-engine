//
// Created by cleme on 2020-02-03.
//

#ifndef GAME_ENGINE_TEXTURE_HPP
#define GAME_ENGINE_TEXTURE_HPP


#include <cstdint>
#include <vulkan/vulkan.h>
#include <string>
#include "Application.hpp"

class Application;

class Texture {
private:
    Application *application;
    VkDevice device;
    uint32_t mipLevels;
    VkImage textureImage;
    VkImageView textureImageView;
    VkDeviceMemory textureImageMemory;
    VkSampler textureSampler;

    void createTextureImageView();
    void createTextureSampler();

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

public:
    Texture(Application *application, VkDevice &device);
    Texture(Application *application, VkDevice &device, std::string texturePath);
    void createTextureImage(std::string texturePath);

    VkImageView getImageView();
    VkSampler getTextureSampler();
    void cleanup();
};


#endif //GAME_ENGINE_TEXTURE_HPP
