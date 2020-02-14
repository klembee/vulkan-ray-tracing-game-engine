//
// Created by cleme on 2020-02-03.
//

#ifndef GAME_ENGINE_APPLICATION_HPP
#define GAME_ENGINE_APPLICATION_HPP

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <ctime>
#include <thread>
#include <set>
#include <cstring>

#include "Vertex.hpp"
#include "Model.hpp"
#include "Camera.hpp"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamiliy;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;

    bool isComplete(){
        return graphicsFamiliy.has_value()
               && presentFamily.has_value()
               && transferFamily.has_value();
    }
};

struct ModelMatrix {
    glm::mat4 *model = nullptr;
};

struct BoneMatrices{
    glm::mat4 *transforms = nullptr;
};

struct CameraMatrices {
    glm::mat4 view;
    glm::mat4 proj;
};

class Model;

class Application {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    std::vector<Model*> models;

    GLFWwindow *window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkQueue  graphicsQueue;
    VkQueue presentQueue;
    VkQueue transferQueue;
    VkRenderPass renderPass;
    VkDescriptorSetLayout  descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    VkCommandPool transferCommandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphore;
    std::vector<VkSemaphore> renderFinishedSemaphore;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

    std::vector<VkBuffer> modelUniformBuffers;
    std::vector<VkDeviceMemory> modelUniformBufferMemory;

    std::vector<VkBuffer> cameraUniformBuffers;
    std::vector<VkDeviceMemory> cameraUniformBufferMemory;

    std::vector<VkBuffer> boneUniformBuffers;
    std::vector<VkDeviceMemory> boneUniformBufferMemory;

    ModelMatrix uboInstance = {};
    BoneMatrices boneMatrices = {};
    size_t uniformDynamicAlignment;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t nbVertices = 0;
    uint32_t nbIndices = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkImage depthImage;
    VkImageView depthImageView;
    VkDeviceMemory depthImageMemory;

    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;


    Camera camera = nullptr;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    //FPS calculation
    double lastTime = glfwGetTime();
    int nbFrames = 0;

    bool framebufferResized = false;

    double clockToMilliseconds(clock_t ticks);
    void initWindow();
    void mainLoop();
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void initVulkan();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createUniformBuffers();
    void createSwapChain();
    void recreateSwapChain();
    void createImageViews();
    void createVertexBuffers();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createCommandPool();
    void createColorResources();
    void createDepthResources();
    void createFrameBuffers();
    void createCommandBuffers();
    void createSyncObjects();

    void cleanup();
    void cleanupSwapChain();

    bool isDeviceSuitable(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    bool checkValidationLayerSupport();
    bool hasStencilComponent(VkFormat format);
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height){
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

public:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer &buffer, VkDeviceMemory &bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
    void endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

    uint32_t getSwapChainImagesCount();
    VkDescriptorSetLayout getDescriptorSetLayout();
    VkBuffer getModelUniformBuffer(uint32_t index);
    VkBuffer getCameraUniformBuffer(uint32_t index);
    VkBuffer getBoneUniformBuffer(uint32_t index);

    VkPhysicalDevice getPhysicalDevice();
    VkQueue getGraphicsQueue();
    VkCommandPool getCommandPool();
};


#endif //GAME_ENGINE_APPLICATION_HPP
