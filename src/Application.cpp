//
// Created by cleme on 2020-02-03.
//
#include <vector>
#include <zconf.h>
#include "../include/helper/FileHelper.hpp"
#include "Application.hpp"
#include "glm/ext.hpp"
#include <unistd.h>

const int WIDTH = 1600;
const int HEIGHT = 1200;
const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAME_RATE = 300;

void* alignedAlloc(size_t size, size_t alignment)
{
    void *data = nullptr;
#if defined(_MSC_VER) || defined(__MINGW32__)
    data = _aligned_malloc(size, alignment);
#else
    int res = posix_memalign(&data, alignment, size);
	if (res != 0)
		data = nullptr;
#endif
    return data;
}

void alignedFree(void* data)
{
#if	defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(data);
#else
    free(data);
#endif
}


const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void *pUserData
){
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkSurfaceKHR surface;
const std::vector<const char*> deviceExtensionsRequired = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger){
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr){
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }else{
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator){
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(func != nullptr){
        func(instance, debugMessenger, pAllocator);
    }
}

VkShaderModule createShaderModule(VkDevice *device, const std::vector<char>& code){
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if(vkCreateShaderModule(*device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shader module.");
    }
    return shaderModule;
}

double Application::clockToMilliseconds(clock_t ticks){
    return (ticks / (double)CLOCKS_PER_SEC) * 1000.0;
}

void Application::initWindow(){
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    this->window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(this->window, this);
    glfwSetFramebufferSizeCallback(this->window, framebufferResizeCallback);

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //Setup the camera
    this->camera = Camera(this->window);
}

void Application::mainLoop() {
    while(!glfwWindowShouldClose(this->window)){
        glfwPollEvents();

        if(glfwGetKey(this->window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
            glfwSetWindowShouldClose(this->window, GLFW_TRUE);
        }

        double startTime = glfwGetTime();
        this->drawFrame();
        double deltaTime = glfwGetTime() - startTime;

        float shouldWaitTime = (1.0/(double)MAX_FRAME_RATE) - deltaTime;

        std::this_thread::sleep_for(std::chrono::milliseconds((int)(shouldWaitTime * 1000)));
    }

    vkDeviceWaitIdle(this->device);
}

void Application::drawFrame(){
    vkWaitForFences(this->device, 1, &inFlightFences[this->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(this->device, this->swapChain, UINT64_MAX,
                                            this->imageAvailableSemaphore[this->currentFrame], VK_NULL_HANDLE, &imageIndex);

    if(result == VK_ERROR_OUT_OF_DATE_KHR){
        this->recreateSwapChain();
        return;
    }else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
        throw std::runtime_error("Failed to acquire swap chain image.");
    }

    //Check if a previous frame is using this image
    if(this->imagesInFlight[imageIndex] != VK_NULL_HANDLE){
        vkWaitForFences(this->device, 1, &this->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    //Mark the image as being use by the frame
    this->imagesInFlight[imageIndex] = this->inFlightFences[this->currentFrame];

    this->updateUniformBuffer(imageIndex);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore  waitSemaphores[] = {this->imageAvailableSemaphore[this->currentFrame]};
    VkPipelineStageFlags  waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &this->commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {this->renderFinishedSemaphore[this->currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(this->device, 1, &this->inFlightFences[this->currentFrame]);

    if(vkQueueSubmit(this->graphicsQueue, 1, &submitInfo, this->inFlightFences[currentFrame]) != VK_SUCCESS){
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = {this->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(this->presentQueue, &presentInfo);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || this->framebufferResized){
        this->recreateSwapChain();
    }else if(result != VK_SUCCESS){
        throw std::runtime_error("Failed to present swap chain image.");
    }

    this->currentFrame = (this->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    CameraMatrices projview = {};
    projview.view = this->camera.getViewMatrix();
    projview.proj = this->camera.getProjectionMatrix();
    projview.proj[1][1] *= -1;


    //Send the model matrix as uniform
    for(size_t i = 0 ; i < this->models.size() ; i++){
        Model *model = this->models[i];
        glm::mat4 *modelMat = (glm::mat4*)(((uint64_t)uboInstance.model + (i * this->uniformDynamicAlignment)));
        glm::mat4 *boneMatrix = (glm::mat4*)(((uint64_t)this->boneMatrices.transforms) + (i * 100 * this->uniformDynamicAlignment));

        *modelMat = model->getModelMatrix();

        std::vector<glm::mat4> boneTransforms(100);
        model->getBoneTransforms(time, boneTransforms);

        for(size_t boneIndex = 0 ; boneIndex < 100 ; boneIndex++){
            if(boneIndex < boneTransforms.size()) {
                boneMatrix[boneIndex] = boneTransforms[boneIndex];
            }else{
                boneMatrix[boneIndex] = glm::mat4(1.0f);
            }
        }
    }

    //Copy data to buffer
    void *data;
    size_t boneMatricesBufferSize = this->models.size() * 100 * this->uniformDynamicAlignment;
    size_t modelMatrixBuffersize = this->models.size() * this->uniformDynamicAlignment;
    size_t viewMatricesBufferSize = sizeof(CameraMatrices);

    //Camera matrices data
    vkMapMemory(this->device, this->cameraUniformBufferMemory[currentImage], 0, viewMatricesBufferSize, 0, &data);
    memcpy(data, &projview, viewMatricesBufferSize);
    vkUnmapMemory(this->device, this->cameraUniformBufferMemory[currentImage]);


    void *data2;
    //Model matrices data
    vkMapMemory(this->device, this->modelUniformBufferMemory[currentImage], 0, modelMatrixBuffersize, 0, &data2);
    memcpy(data2, &this->uboInstance.model[0], modelMatrixBuffersize);
    vkUnmapMemory(this->device, modelUniformBufferMemory[currentImage]);

    //Bone matrices data
    vkMapMemory(this->device, this->boneUniformBufferMemory[currentImage], 0, boneMatricesBufferSize, 0, &data2);
    memcpy(data2, &this->boneMatrices.transforms[0], boneMatricesBufferSize);
    vkUnmapMemory(this->device, boneUniformBufferMemory[currentImage]);
}


void Application::initVulkan() {
    this->createInstance();
    this->setupDebugMessenger();
    this->createSurface();
    this->pickPhysicalDevice();
    this->createLogicalDevice();
    this->createSwapChain();
    this->createImageViews();
    this->createRenderPass();
    this->createDescriptorSetLayout();
    this->createCommandPool();
    printf("1\n");
    this->models = {
            new Model(this, this->device),
//            new Model(this, this->device, glm::vec3(0.0f, 0.0f, 50.0f)),
//            new Model(this, this->device, glm::vec3(50.0f, 0.0f, 0.0f)),
//            new Model(this, this->device, glm::vec3(50.0f, 0.0f, 50.0f))
    };

    this->createVertexBuffers();
    this->createUniformBuffers();
    this->createGraphicsPipeline();
    this->createColorResources();
    this->createDepthResources();
    this->createFrameBuffers();

    //Init the models
    for(Model *model : this->models){
        model->init();
    }


    this->createCommandBuffers();
    this->createSyncObjects();
}

void Application::createInstance(){
    if(enableValidationLayers && !this->checkValidationLayerSupport()){
        throw std::runtime_error("Validation layers requested, but not available !");
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan testing";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if(enableValidationLayers){
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }else{
        createInfo.enabledLayerCount = 0;
    }

    auto extensions = this->getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    //Create the vulkan instance
    if(vkCreateInstance(&createInfo, nullptr, &this->instance) != VK_SUCCESS){
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void Application::setupDebugMessenger(){
    if(!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
    if(CreateDebugUtilsMessengerEXT(this->instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS){
        throw std::runtime_error("Failed to setup debug messenger.");
    }
}

/**
 * Selects the GPU of the user
 */
void Application::pickPhysicalDevice(){
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
    if(deviceCount == 0){
        throw std::runtime_error("Failed to find GPU with Vulkan support.");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, devices.data());
    for(const auto& device : devices){
        if(this->isDeviceSuitable(device)){
            this->physicalDevice = device;
            this->msaaSamples = this->getMaxUsableSampleCount();
            break;
        }
    }

    if(this->physicalDevice == VK_NULL_HANDLE){
        throw std::runtime_error("Failed to find a suitable GPU.");
    }
}

bool Application::isDeviceSuitable(VkPhysicalDevice device){

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indices = this->findQueueFamilies(device);

    bool extensionsSupported = this->checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if(extensionsSupported){
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
           && deviceFeatures.geometryShader
           && indices.isComplete()
           && extensionsSupported
           && swapChainAdequate
           && deviceFeatures.samplerAnisotropy;
}

SwapChainSupportDetails Application::querySwapChainSupport(VkPhysicalDevice device){
    SwapChainSupportDetails details = {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if(formatCount > 0){
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if(presentModeCount > 0){
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Application::checkDeviceExtensionSupport(VkPhysicalDevice device){
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensionsRequired.begin(), deviceExtensionsRequired.end());

    for(const auto& extension : availableExtensions){
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice device){
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for(const auto& queueFamily : queueFamilies){
        if((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
           && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)){
            //Transfer queue
            indices.transferFamily = i;
        } else{
            if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
                indices.graphicsFamiliy = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if(presentSupport){
                indices.presentFamily = i;
            }
        }


        if(indices.isComplete()){
            break;
        }

        i++;
    }

    return indices;
}

void Application::createLogicalDevice(){
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamiliy.value(), indices.presentFamily.value(), indices.transferFamily.value()};

    float queuePriority = 1.0f;
    for(uint32_t queueFamily : uniqueQueueFamilies){
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }


    VkPhysicalDeviceFeatures  deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionsRequired.size());
    createInfo.ppEnabledExtensionNames = deviceExtensionsRequired.data();

    if(enableValidationLayers){
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }else{
        createInfo.enabledLayerCount = 0;
    }

    if(vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->device) != VK_SUCCESS){
        throw std::runtime_error("Failed to create logical device.");
    }

    vkGetDeviceQueue(this->device, indices.graphicsFamiliy.value(), 0, &this->graphicsQueue);
    vkGetDeviceQueue(this->device, indices.presentFamily.value(), 0, &this->presentQueue);
    vkGetDeviceQueue(this->device, indices.transferFamily.value(), 0, &this->transferQueue);
}

/**
 * Get the list of required extensions
 * @return the list of required extensions by GLFW
 */
std::vector<const char*> Application::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Application::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = this->querySwapChainSupport(this->physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = this->chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = this->chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = this->chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount){
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = this->findQueueFamilies(this->physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamiliy.value(), indices.presentFamily.value()};
    if(indices.graphicsFamiliy != indices.presentFamily){
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }else{
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &this->swapChain) != VK_SUCCESS){
        throw std::runtime_error("Failed to create swap chain.");
    }

    vkGetSwapchainImagesKHR(device, this->swapChain, &imageCount, nullptr);
    this->swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, this->swapChain, &imageCount, this->swapChainImages.data());

    this->swapChainImageFormat = surfaceFormat.format;
    this->swapChainExtent = extent;
}

void Application::createImageViews(){
    this->swapChainImageViews.resize(this->swapChainImages.size());

    for(size_t i = 0; i < swapChainImages.size() ; i++){
        this->swapChainImageViews[i] = this->createImageView(this->swapChainImages[i], this->swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

VkSurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for(const auto& availableFormat : availableFormats){
        if(availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM
           && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Application::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes){
    for(const auto& availablePresentMode : availablePresentModes){
        if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR){
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities){
    if(capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    }else{
        int width, height;
        glfwGetFramebufferSize(this->window, &width, &height);

        VkExtent2D actualExtent = {
                static_cast<uint32_t>(WIDTH),
                static_cast<uint32_t>(HEIGHT)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width,
                                      std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
                                       std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

bool Application::checkValidationLayerSupport(){
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const char* layerName : validationLayers){
        bool layerFound = false;
        for(const auto& layerProperties : availableLayers){
            if(strcmp(layerName, layerProperties.layerName) == 0){
                layerFound = true;
                break;
            }
        }
        if(!layerFound){
            return false;
        }
    }

    return true;
}

/**
 * Create the render pass
 */
void Application::createRenderPass(){
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = this->swapChainImageFormat;
    colorAttachment.samples = this->msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve = {};
    colorAttachmentResolve.format = this->swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = this->findDepthFormat();
    depthAttachment.samples = this->msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef = {};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;


    if(vkCreateRenderPass(this->device, &renderPassInfo, nullptr, &this->renderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create render pass.");
    }
}

void Application::createDescriptorSetLayout(){
    VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    //Model matrix
    modelLayoutBinding.binding = 0;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelLayoutBinding.pImmutableSamplers = nullptr;

    //Camera matrices
    VkDescriptorSetLayoutBinding viewMatricesBinding = {};
    viewMatricesBinding.binding = 1;
    viewMatricesBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    viewMatricesBinding.descriptorCount = 1;
    viewMatricesBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    viewMatricesBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bonesLayoutBinding = {};
    bonesLayoutBinding.binding = 2;
    bonesLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bonesLayoutBinding.descriptorCount = 1;
    bonesLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bonesLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 3;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 8;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;


    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {modelLayoutBinding, viewMatricesBinding, bonesLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if(vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &this->descriptorSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
}

void Application::createVertexBuffers() {
    for(Model *model : this->models){
        for(Vertex vertex : model->getVertices()){
            vertices.push_back(vertex);
        }
        for(uint32_t index : model->getIndices()){
            indices.push_back(index);
        }
    }

    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

    this->nbVertices = vertices.size();
    this->nbIndices = indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;


    //To CPU
    this->createBuffer(vertexBufferSize + indexBufferSize,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              stagingBuffer,
                              stagingBufferMemory);
    void *data;
    vkMapMemory(device, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    //Add the index data after vertex data
    vkMapMemory(device, stagingBufferMemory, vertexBufferSize, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    //To GPU
    this->createBuffer(vertexBufferSize + indexBufferSize,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              this->vertexBuffer,
                              this->vertexBufferMemory);

    this->copyBuffer(stagingBuffer, this->vertexBuffer, vertexBufferSize + indexBufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Application::createUniformBuffers(){
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(this->physicalDevice, &physicalDeviceProperties);

    size_t minUboAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    this->uniformDynamicAlignment = sizeof(glm::mat4);
    if(minUboAlignment > 0){
        this->uniformDynamicAlignment = (this->uniformDynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    size_t modelMatrixBufferSize = this->models.size() * this->uniformDynamicAlignment;
    size_t viewMatricesBufferSize = sizeof(CameraMatrices);
    size_t boneMatricesBufferSize = this->models.size() * 100 * this->uniformDynamicAlignment;

    this->uboInstance.model = (glm::mat4*)alignedAlloc(modelMatrixBufferSize, this->uniformDynamicAlignment);
    this->boneMatrices.transforms = (glm::mat4*)alignedAlloc(boneMatricesBufferSize, this->uniformDynamicAlignment);

    this->modelUniformBuffers.resize(swapChainImages.size());
    this->modelUniformBufferMemory.resize(swapChainImages.size());
    this->cameraUniformBuffers.resize(swapChainImages.size());
    this->cameraUniformBufferMemory.resize(swapChainImages.size());
    this->boneUniformBuffers.resize(swapChainImages.size());
    this->boneUniformBufferMemory.resize(swapChainImages.size());

    for(size_t i = 0; i < this->swapChainImages.size() ; i++){
        //Create the uniform for model data
        this->createBuffer(modelMatrixBufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                this->modelUniformBuffers[i],
                this->modelUniformBufferMemory[i]);

        //Create the uniform for camera data
        this->createBuffer(viewMatricesBufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                this->cameraUniformBuffers[i],
                this->cameraUniformBufferMemory[i]);

        //Create the bone matrices uniform data
        this->createBuffer(boneMatricesBufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    this->boneUniformBuffers[i],
                    this->boneUniformBufferMemory[i]);
    }
}

/**
 * Creates the graphics pipeline to draw
 */
void Application::createGraphicsPipeline(){
    auto vertShaderCode = readFile("./shaders/build/vertice.spv");
    auto fragShaderCode = readFile("./shaders/build/fragment.spv");

    VkShaderModule vertexShaderModule = createShaderModule(&this->device, vertShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(&this->device, fragShaderCode);

    //Vertex stage
    VkPipelineShaderStageCreateInfo vertexShaderStageInfo = {};
    vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";

    //Fragment stage
    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = {};
    fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageInfo, fragmentShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) this->swapChainExtent.width;
    viewport.height = (float) this->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    //Which part of the framebuffer to draw
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = this->swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = this->msaaSamples;
    multisampling.minSampleShading = 0.2f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    //Push constants
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.size = sizeof(CameraMatrices);
    pushConstantRange.offset = 0;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &this->descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if(vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    //Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    //Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = this->pipelineLayout;
    pipelineInfo.renderPass = this->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if(vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->graphicsPipeline) != VK_SUCCESS){
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(this->device, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(this->device, vertexShaderModule, nullptr);
}

void Application::createFrameBuffers(){
    this->swapChainFramebuffers.resize(this->swapChainImageViews.size());

    for(size_t i = 0; i < this->swapChainImageViews.size() ; i++){
        std::array<VkImageView,3> attachments = {
                this->colorImageView,
                depthImageView,
                this->swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = this->renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = this->swapChainExtent.width;
        framebufferInfo.height = this->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if(vkCreateFramebuffer(this->device, &framebufferInfo, nullptr, &this->swapChainFramebuffers[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create the framebuffers.");
        }
    }
}

void Application::createColorResources(){
    VkFormat colorFormat = this->swapChainImageFormat;

    this->createImage(this->swapChainExtent.width,
                      this->swapChainExtent.height,
                      1,
                      this->msaaSamples,
                      colorFormat,
                      VK_IMAGE_TILING_OPTIMAL,
                      VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      this->colorImage,
                      this->colorImageMemory);

    this->colorImageView = this->createImageView(this->colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void Application::createDepthResources(){
    VkFormat depthformat = this->findDepthFormat();

    this->createImage(
            this->swapChainExtent.width,
            this->swapChainExtent.height,
            1,
            this->msaaSamples,
            depthformat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            this->depthImage,
            this->depthImageMemory
    );

    this->depthImageView = this->createImageView(this->depthImage, depthformat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

bool Application::hasStencilComponent(VkFormat format){
    return format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkFormat Application::findDepthFormat(){
    return this->findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat Application::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features){
    for(VkFormat format: candidates){
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(this->physicalDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR &&
           (props.linearTilingFeatures & features) == features){
            return format;
        }else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features){
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format.");
}

void Application::createCommandPool(){
    QueueFamilyIndices queueFamilyIndices = this->findQueueFamilies(this->physicalDevice);
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamiliy.value();
    poolInfo.flags = 0;

    if(vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->commandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create command pool");
    }

    //Create transfer command pool
    poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if(vkCreateCommandPool(this->device, &poolInfo, nullptr, &this->transferCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create transfer command pool");
    }
}

VkSampleCountFlagBits Application::getMaxUsableSampleCount(){
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(this->physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if(counts & VK_SAMPLE_COUNT_64_BIT){ return VK_SAMPLE_COUNT_64_BIT; }
    if(counts & VK_SAMPLE_COUNT_32_BIT){ return VK_SAMPLE_COUNT_32_BIT; }
    if(counts & VK_SAMPLE_COUNT_16_BIT){ return VK_SAMPLE_COUNT_16_BIT; }
    if(counts & VK_SAMPLE_COUNT_8_BIT){ return VK_SAMPLE_COUNT_8_BIT; }
    if(counts & VK_SAMPLE_COUNT_4_BIT){ return VK_SAMPLE_COUNT_4_BIT; }
    if(counts & VK_SAMPLE_COUNT_2_BIT){ return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

VkImageView Application::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels){
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if(vkCreateImageView(this->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS){
        throw std::runtime_error("Failed to create texture image view.");
    }

    return imageView;
}

void Application::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory){
    QueueFamilyIndices queueFamilies = this->findQueueFamilies(this->physicalDevice);
    uint32_t queueFamilyindices[] = {queueFamilies.transferFamily.value(), queueFamilies.presentFamily.value()};

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//        imageInfo.queueFamilyIndexCount = 1;
//        imageInfo.pQueueFamilyIndices = &queueFamilies.graphicsFamiliy.value();
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;
    imageInfo.samples = numSamples;

    if(vkCreateImage(this->device, &imageInfo, nullptr, &image) != VK_SUCCESS){
        throw std::runtime_error("Failed to create image.");
    }

    //Transfer data to the image
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(this->device, image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = this->findMemoryType(memoryRequirements.memoryTypeBits, properties);

    if(vkAllocateMemory(this->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate image memory.");
    }

    vkBindImageMemory(this->device, image, imageMemory, 0);
}

void Application::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height){
    VkCommandBuffer commandBuffer = this->beginSingleTimeCommands(this->transferCommandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
            width,
            height,
            1
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    this->endSingleTimeCommands(this->transferQueue, this->transferCommandPool, commandBuffer);
}

void Application::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels){
    VkCommandBuffer commandBuffer = this->beginSingleTimeCommands(this->commandPool);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }else{
        throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer,
                         sourceStage, destinationStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    this->endSingleTimeCommands(this->graphicsQueue, this->commandPool, commandBuffer);
}



VkCommandBuffer Application::beginSingleTimeCommands(VkCommandPool commandPool){
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(this->device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Application::endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer){
    vkEndCommandBuffer(commandBuffer);

    //Run the command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(this->device, commandPool, 1, &commandBuffer);
}

uint32_t Application::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount ; i++){
        if(typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties){
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type.");
}

void Application::createCommandBuffers(){
    this->commandBuffers.resize(this->swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = this->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) this->commandBuffers.size();

    if(vkAllocateCommandBuffers(this->device, &allocInfo, this->commandBuffers.data()) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate command buffers");
    }

    for(size_t i = 0 ; i < this->commandBuffers.size() ; i++){
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if(vkBeginCommandBuffer(this->commandBuffers[i], &beginInfo) != VK_SUCCESS){
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        std::array<VkClearValue, 2> clearValues = {};
        clearValues[0].color = {53.0f/255.0f, 81.0f/255.0f, 92.0f/255.0f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = this->renderPass;
        renderPassInfo.framebuffer = this->swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = this->swapChainExtent;
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(this->commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(this->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkDeviceSize indicesOffset = sizeof(Vertex) * this->nbVertices;
        VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &this->vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], this->vertexBuffer, indicesOffset, VK_INDEX_TYPE_UINT32);

        for(size_t j = 0 ; j < this->models.size() ; j++){
            Model *model = this->models[j];
            uint32_t modelDynamicOffset = j * static_cast<uint32_t>(this->uniformDynamicAlignment);

            VkDescriptorSet* modelDescriptorSet =  model->getDescriptorSet(i);
            vkCmdBindDescriptorSets(this->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, modelDescriptorSet, 1,
                                    &modelDynamicOffset);

            vkCmdDrawIndexed(commandBuffers[i], this->nbIndices, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(this->commandBuffers[i]);

        //END
        if(vkEndCommandBuffer(this->commandBuffers[i]) != VK_SUCCESS ){
            throw std::runtime_error("Failed to record command buffer.");
        }

    }
}

void Application::createSyncObjects(){
    this->imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    this->renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    this->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    this->imagesInFlight.resize(this->swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(size_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i++){
        if(vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->imageAvailableSemaphore[i]) != VK_SUCCESS ||
           vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, &this->renderFinishedSemaphore[i]) != VK_SUCCESS ||
           vkCreateFence(this->device, &fenceInfo, nullptr, &this->inFlightFences[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create semaphores");
        }
    }

}

void Application::createSurface(){
    if(glfwCreateWindowSurface(this->instance, this->window, nullptr, &surface) != VK_SUCCESS){
        throw std::runtime_error("Failed to create window surface.");
    }
}

void Application::recreateSwapChain(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(this->window, &width, &height);
    while(width == 0 || height == 0){
        glfwGetFramebufferSize(window, &width, &height);
        //Pause the application if minified
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(this->device);
    this->cleanupSwapChain();
    this->createSwapChain();
    this->createImageViews();
    this->createRenderPass();
    this->createGraphicsPipeline();
    this->createColorResources();
    this->createDepthResources();
    this->createFrameBuffers();
    this->createUniformBuffers();
    this->createCommandBuffers();

    this->framebufferResized = false;
}

void Application::cleanup() {
    if(enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(this->instance, this->debugMessenger, nullptr);
    }

    this->cleanupSwapChain();
    for(Model *model : this->models){
        model->cleanup();
    }
    if(this->uboInstance.model){
        alignedFree(this->uboInstance.model);
    }

    vkDestroyDescriptorSetLayout(this->device, this->descriptorSetLayout, nullptr);

    vkDestroyCommandPool(this->device, this->commandPool, nullptr);
    vkDestroyCommandPool(this->device, this->transferCommandPool, nullptr);

    for(size_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i++){
        vkDestroySemaphore(this->device, this->renderFinishedSemaphore[i], nullptr);
        vkDestroySemaphore(this->device, this->imageAvailableSemaphore[i], nullptr);
        vkDestroyFence(this->device, this->inFlightFences[i], nullptr);
    }

    vkDestroyDevice(this->device, nullptr);
    vkDestroySurfaceKHR(this->instance, surface, nullptr);
    vkDestroyInstance(this->instance, nullptr);

    glfwDestroyWindow(this->window);
    glfwTerminate();
}

void Application::cleanupSwapChain(){
    for(auto framebuffer : this->swapChainFramebuffers){
        vkDestroyFramebuffer(this->device, framebuffer, nullptr);
    }

    vkDestroyBuffer(this->device, this->vertexBuffer, nullptr);
    vkFreeMemory(this->device, this->vertexBufferMemory, nullptr);

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroyBuffer(device, this->modelUniformBuffers[i], nullptr);
        vkFreeMemory(device, this->modelUniformBufferMemory[i], nullptr);
        vkDestroyBuffer(device, this->cameraUniformBuffers[i], nullptr);
        vkFreeMemory(device, this->cameraUniformBufferMemory[i], nullptr);
        vkDestroyBuffer(device, this->boneUniformBuffers[i], nullptr);
        vkFreeMemory(device, this->boneUniformBufferMemory[i], nullptr);
    }

    vkDestroyImageView(this->device, this->colorImageView, nullptr);
    vkDestroyImage(this->device, this->colorImage, nullptr);
    vkFreeMemory(this->device, this->colorImageMemory, nullptr);

    vkDestroyImage(this->device, this->depthImage, nullptr);
    vkDestroyImageView(this->device, this->depthImageView, nullptr);
    vkFreeMemory(this->device, this->depthImageMemory, nullptr);

    vkFreeCommandBuffers(this->device, this->commandPool, static_cast<uint32_t>(this->commandBuffers.size()), this->commandBuffers.data());

    vkDestroyPipeline(this->device, this->graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);

    vkDestroyRenderPass(this->device, this->renderPass, nullptr);
    vkDestroySwapchainKHR(this->device, this->swapChain, nullptr);

    //Destroy image views
    for(auto imageView : this->swapChainImageViews){
        vkDestroyImageView(this->device, imageView, nullptr);
    }
}


void Application::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer &buffer, VkDeviceMemory &bufferMemory){

    auto queueIndices = this->findQueueFamilies(this->physicalDevice);
    uint32_t queueFamilies[] = {queueIndices.graphicsFamiliy.value(), queueIndices.transferFamily.value()};

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    bufferInfo.queueFamilyIndexCount = 2;
    bufferInfo.pQueueFamilyIndices = queueFamilies;
    bufferInfo.flags = 0;

    if(vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create vertex buffer.");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

    if(vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate vertex buffer memory.");
    }

    vkBindBufferMemory(this->device, buffer, bufferMemory, 0);
}

void Application::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size){
    auto commandBuffer = this->beginSingleTimeCommands(this->transferCommandPool);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    this->endSingleTimeCommands(this->transferQueue, this->transferCommandPool, commandBuffer);
}

VkPhysicalDevice Application::getPhysicalDevice(){
    return this->physicalDevice;
}

VkQueue Application::getGraphicsQueue(){
    return this->graphicsQueue;
};
VkCommandPool Application::getCommandPool(){
    return this->commandPool;
}

uint32_t Application::getSwapChainImagesCount(){
    return this->swapChainImages.size();
}

VkDescriptorSetLayout Application::getDescriptorSetLayout(){
    return this->descriptorSetLayout;
}
VkBuffer Application::getModelUniformBuffer(uint32_t index){
    return this->modelUniformBuffers[index];
}

VkBuffer Application::getCameraUniformBuffer(uint32_t index){
    return this->cameraUniformBuffers[index];
}

VkBuffer Application::getBoneUniformBuffer(uint32_t index){
    return this->boneUniformBuffers[index];
}
