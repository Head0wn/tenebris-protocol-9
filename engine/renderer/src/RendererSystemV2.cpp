#include <tenebris/renderer/RendererSystem.hpp>

#include <tenebris/scene/GpuUploadPlan.hpp>
#include <tenebris/scene/RenderScene.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tenebris::renderer {

namespace {

constexpr std::uint32_t invalidQueueFamily = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint64_t infiniteTimeout = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint32_t spirvMagic = 0x07230203U;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData
) {
    static_cast<void>(type);
    static_cast<void>(userData);
    if (callbackData == nullptr || callbackData->pMessage == nullptr) {
        return VK_FALSE;
    }

    const char* label = "INFO";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        label = "ERROR";
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
        label = "WARNING";
    }
    std::cerr << "[Vulkan " << label << "] " << callbackData->pMessage << '\n';
    return VK_FALSE;
}

[[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    return createInfo;
}

[[nodiscard]] std::string makeSdlError(const char* context) {
    return std::string{context} + ": " + SDL_GetError();
}

[[nodiscard]] std::string makeVulkanError(const char* context, VkResult result) {
    return std::string{context} + " (VkResult "
        + std::to_string(static_cast<std::int32_t>(result)) + ')';
}

[[nodiscard]] bool hasExtension(
    const std::vector<VkExtensionProperties>& extensions,
    const char* expected
) {
    return std::any_of(
        extensions.begin(),
        extensions.end(),
        [expected](const VkExtensionProperties& extension) {
            return std::strcmp(extension.extensionName, expected) == 0;
        }
    );
}

[[nodiscard]] bool hasLayer(const std::vector<VkLayerProperties>& layers, const char* expected) {
    return std::any_of(
        layers.begin(),
        layers.end(),
        [expected](const VkLayerProperties& layer) {
            return std::strcmp(layer.layerName, expected) == 0;
        }
    );
}

[[nodiscard]] bool hasStencilComponent(VkFormat format) noexcept {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

struct VulkanDispatch final {
    PFN_vkGetInstanceProcAddr getInstanceProcAddr{nullptr};
    PFN_vkEnumerateInstanceVersion enumerateInstanceVersion{nullptr};
    PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties{nullptr};
    PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties{nullptr};
    PFN_vkCreateInstance createInstance{nullptr};

    PFN_vkDestroyInstance destroyInstance{nullptr};
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices{nullptr};
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties{nullptr};
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties{nullptr};
    PFN_vkGetPhysicalDeviceFormatProperties getPhysicalDeviceFormatProperties{nullptr};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getPhysicalDeviceQueueFamilyProperties{nullptr};
    PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties{nullptr};
    PFN_vkCreateDevice createDevice{nullptr};
    PFN_vkGetDeviceProcAddr getDeviceProcAddr{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupport{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities{nullptr};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormats{nullptr};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModes{nullptr};
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger{nullptr};
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger{nullptr};

    PFN_vkDestroyDevice destroyDevice{nullptr};
    PFN_vkGetDeviceQueue getDeviceQueue{nullptr};
    PFN_vkCreateSwapchainKHR createSwapchain{nullptr};
    PFN_vkDestroySwapchainKHR destroySwapchain{nullptr};
    PFN_vkGetSwapchainImagesKHR getSwapchainImages{nullptr};
    PFN_vkCreateImageView createImageView{nullptr};
    PFN_vkDestroyImageView destroyImageView{nullptr};
    PFN_vkCreateRenderPass createRenderPass{nullptr};
    PFN_vkDestroyRenderPass destroyRenderPass{nullptr};
    PFN_vkCreateFramebuffer createFramebuffer{nullptr};
    PFN_vkDestroyFramebuffer destroyFramebuffer{nullptr};
    PFN_vkCreateCommandPool createCommandPool{nullptr};
    PFN_vkDestroyCommandPool destroyCommandPool{nullptr};
    PFN_vkAllocateCommandBuffers allocateCommandBuffers{nullptr};
    PFN_vkFreeCommandBuffers freeCommandBuffers{nullptr};
    PFN_vkResetCommandBuffer resetCommandBuffer{nullptr};
    PFN_vkBeginCommandBuffer beginCommandBuffer{nullptr};
    PFN_vkEndCommandBuffer endCommandBuffer{nullptr};
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass{nullptr};
    PFN_vkCmdEndRenderPass cmdEndRenderPass{nullptr};
    PFN_vkCmdCopyBuffer cmdCopyBuffer{nullptr};
    PFN_vkCmdBindPipeline cmdBindPipeline{nullptr};
    PFN_vkCmdSetViewport cmdSetViewport{nullptr};
    PFN_vkCmdSetScissor cmdSetScissor{nullptr};
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers{nullptr};
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer{nullptr};
    PFN_vkCmdPushConstants cmdPushConstants{nullptr};
    PFN_vkCmdDrawIndexed cmdDrawIndexed{nullptr};
    PFN_vkCreateSemaphore createSemaphore{nullptr};
    PFN_vkDestroySemaphore destroySemaphore{nullptr};
    PFN_vkCreateFence createFence{nullptr};
    PFN_vkDestroyFence destroyFence{nullptr};
    PFN_vkWaitForFences waitForFences{nullptr};
    PFN_vkResetFences resetFences{nullptr};
    PFN_vkAcquireNextImageKHR acquireNextImage{nullptr};
    PFN_vkQueueSubmit queueSubmit{nullptr};
    PFN_vkQueuePresentKHR queuePresent{nullptr};
    PFN_vkQueueWaitIdle queueWaitIdle{nullptr};
    PFN_vkDeviceWaitIdle deviceWaitIdle{nullptr};

    PFN_vkCreateBuffer createBuffer{nullptr};
    PFN_vkDestroyBuffer destroyBuffer{nullptr};
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements{nullptr};
    PFN_vkAllocateMemory allocateMemory{nullptr};
    PFN_vkFreeMemory freeMemory{nullptr};
    PFN_vkBindBufferMemory bindBufferMemory{nullptr};
    PFN_vkMapMemory mapMemory{nullptr};
    PFN_vkUnmapMemory unmapMemory{nullptr};
    PFN_vkCreateImage createImage{nullptr};
    PFN_vkDestroyImage destroyImage{nullptr};
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements{nullptr};
    PFN_vkBindImageMemory bindImageMemory{nullptr};
    PFN_vkCreateShaderModule createShaderModule{nullptr};
    PFN_vkDestroyShaderModule destroyShaderModule{nullptr};
    PFN_vkCreatePipelineLayout createPipelineLayout{nullptr};
    PFN_vkDestroyPipelineLayout destroyPipelineLayout{nullptr};
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines{nullptr};
    PFN_vkDestroyPipeline destroyPipeline{nullptr};

    template <typename Function>
    [[nodiscard]] bool loadGlobal(Function& function, const char* name) const {
        function = reinterpret_cast<Function>(getInstanceProcAddr(VK_NULL_HANDLE, name));
        return function != nullptr;
    }

    template <typename Function>
    [[nodiscard]] bool loadInstance(Function& function, VkInstance instance, const char* name) const {
        function = reinterpret_cast<Function>(getInstanceProcAddr(instance, name));
        return function != nullptr;
    }

    template <typename Function>
    [[nodiscard]] bool loadDevice(Function& function, VkDevice device, const char* name) const {
        function = reinterpret_cast<Function>(getDeviceProcAddr(device, name));
        return function != nullptr;
    }
};

struct SwapchainSupport final {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct DeviceCandidate final {
    VkPhysicalDevice device{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily{invalidQueueFamily};
    std::uint32_t presentQueueFamily{invalidQueueFamily};
    std::uint64_t score{0U};
};

struct FrameSync final {
    VkSemaphore imageAvailable{VK_NULL_HANDLE};
    VkSemaphore renderFinished{VK_NULL_HANDLE};
    VkFence inFlight{VK_NULL_HANDLE};
};

struct BufferAllocation final {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0U};
};

[[nodiscard]] std::optional<std::vector<std::uint32_t>> loadSpirvFile(
    const std::filesystem::path& path,
    std::string& error
) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        error = "Unable to open SPIR-V shader: " + path.string();
        return std::nullopt;
    }

    const std::streampos endPosition = stream.tellg();
    if (endPosition <= 0) {
        error = "SPIR-V shader is empty: " + path.string();
        return std::nullopt;
    }

    const auto byteCount = static_cast<std::uint64_t>(endPosition);
    if ((byteCount % sizeof(std::uint32_t)) != 0U
        || byteCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        error = "SPIR-V shader has an invalid byte length: " + path.string();
        return std::nullopt;
    }

    std::vector<std::uint32_t> words(
        static_cast<std::size_t>(byteCount / sizeof(std::uint32_t))
    );
    stream.seekg(0, std::ios::beg);
    stream.read(
        reinterpret_cast<char*>(words.data()),
        static_cast<std::streamsize>(byteCount)
    );
    if (!stream || words.empty() || words.front() != spirvMagic) {
        error = "SPIR-V shader failed validation: " + path.string();
        return std::nullopt;
    }
    return words;
}

} // namespace

struct RendererSystem::Impl final {
    explicit Impl(RendererConfig rendererConfig)
        : config(std::move(rendererConfig)) {
        config.framesInFlight = std::clamp(config.framesInFlight, 1U, 3U);
    }

    RendererConfig config;
    RendererState state{RendererState::Stopped};
    RendererStats stats{};
    std::string lastError;
    SDL_Window* window{nullptr};
    bool headless{false};
    bool validationEnabled{false};
    bool framebufferResized{false};

    VulkanDispatch vk{};
    VkInstance instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkDevice device{VK_NULL_HANDLE};
    std::uint32_t graphicsQueueFamily{invalidQueueFamily};
    std::uint32_t presentQueueFamily{invalidQueueFamily};
    VkQueue graphicsQueue{VK_NULL_HANDLE};
    VkQueue presentQueue{VK_NULL_HANDLE};

    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat swapchainFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D swapchainExtent{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    VkImage depthImage{VK_NULL_HANDLE};
    VkDeviceMemory depthMemory{VK_NULL_HANDLE};
    VkImageView depthImageView{VK_NULL_HANDLE};
    VkRenderPass renderPass{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers;

    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<FrameSync> frameSync;
    std::vector<VkFence> imagesInFlight;
    std::uint32_t currentFrame{0U};

    scene::RenderMesh sceneMesh;
    scene::GpuUploadPlan uploadPlan{};
    scene::CameraConfig cameraConfig{};
    scene::VoxelPushConstants pushConstants{};
    BufferAllocation sceneBuffer{};
    std::uint64_t uploadedRevision{0U};
    bool hasScene{false};

    [[nodiscard]] bool fail(std::string message) {
        lastError = std::move(message);
        state = RendererState::Failed;
        return false;
    }

    [[nodiscard]] bool reject(std::string message) {
        lastError = std::move(message);
        return false;
    }

    [[nodiscard]] bool initialize(SDL_Window* targetWindow, bool runHeadless) {
        if (state != RendererState::Stopped) {
            return fail("RendererSystem is already initialized.");
        }

        lastError.clear();
        stats = {};
        window = targetWindow;
        headless = runHeadless;

        if (headless) {
            state = RendererState::Headless;
            return true;
        }
        if (window == nullptr) {
            return fail("RendererSystem requires a valid SDL window.");
        }

        const SDL_FunctionPointer rawGetInstanceProcAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
        if (rawGetInstanceProcAddr == nullptr) {
            return fail(makeSdlError("SDL_Vulkan_GetVkGetInstanceProcAddr failed"));
        }
        vk.getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(rawGetInstanceProcAddr);

        if (!loadGlobalFunctions() || !createVulkanInstance() || !loadInstanceFunctions()
            || !createDebugMessenger()) {
            return initializationFailure();
        }
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            fail(makeSdlError("SDL_Vulkan_CreateSurface failed"));
            return initializationFailure();
        }
        if (!selectPhysicalDevice() || !createLogicalDevice() || !loadDeviceFunctions()) {
            return initializationFailure();
        }

        vk.getDeviceQueue(device, graphicsQueueFamily, 0U, &graphicsQueue);
        vk.getDeviceQueue(device, presentQueueFamily, 0U, &presentQueue);
        vk.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        if (!createCommandResources() || !createSwapchainResources() || !createSynchronization()) {
            return initializationFailure();
        }

        state = state == RendererState::Suspended ? RendererState::Suspended : RendererState::Ready;
        return true;
    }

    [[nodiscard]] bool initializationFailure() {
        const std::string error = lastError;
        shutdown();
        lastError = error;
        state = RendererState::Failed;
        return false;
    }

    [[nodiscard]] bool setVoxelScene(
        const scene::RenderMesh& mesh,
        const scene::SceneCamera& camera
    ) {
        if (state != RendererState::Headless && state != RendererState::Ready
            && state != RendererState::Suspended) {
            return reject("RendererSystem must be initialized before submitting a voxel scene.");
        }

        const scene::GpuUploadPlan plan = scene::makeGpuUploadPlan(mesh);
        if (!plan.valid() || !plan.drawable()) {
            return reject("Voxel scene rejected: " + plan.errorMessage());
        }

        cameraConfig = camera.config();
        sceneMesh = mesh;
        uploadPlan = plan;
        hasScene = true;
        refreshPushConstants();

        if (headless) {
            uploadedRevision = mesh.sourceRevision;
            lastError.clear();
            return true;
        }

        if (uploadedRevision == mesh.sourceRevision && sceneBuffer.buffer != VK_NULL_HANDLE) {
            lastError.clear();
            return true;
        }

        waitIdle();
        destroySceneBuffer();
        if (!uploadSceneBuffer()) {
            return false;
        }

        uploadedRevision = mesh.sourceRevision;
        ++stats.sceneUploads;
        stats.uploadedBytes += uploadPlan.stagingSize;
        lastError.clear();
        return true;
    }

    void refreshPushConstants() noexcept {
        if (!hasScene) {
            return;
        }
        scene::CameraConfig adjusted = cameraConfig;
        if (swapchainExtent.width > 0U && swapchainExtent.height > 0U) {
            adjusted.aspectRatio = static_cast<float>(swapchainExtent.width)
                / static_cast<float>(swapchainExtent.height);
        }
        const scene::SceneCamera camera{adjusted};
        pushConstants = scene::makeVoxelPushConstants(camera);
    }

    [[nodiscard]] bool loadGlobalFunctions() {
        const bool loaded = vk.loadGlobal(
            vk.enumerateInstanceExtensionProperties,
            "vkEnumerateInstanceExtensionProperties"
        ) && vk.loadGlobal(
            vk.enumerateInstanceLayerProperties,
            "vkEnumerateInstanceLayerProperties"
        ) && vk.loadGlobal(vk.createInstance, "vkCreateInstance");
        static_cast<void>(vk.loadGlobal(vk.enumerateInstanceVersion, "vkEnumerateInstanceVersion"));
        return loaded || fail("Vulkan loader is missing required global functions.");
    }

    [[nodiscard]] std::vector<VkExtensionProperties> enumerateInstanceExtensions() {
        std::uint32_t count = 0U;
        VkResult result = vk.enumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkEnumerateInstanceExtensionProperties failed", result));
            return {};
        }
        std::vector<VkExtensionProperties> values(count);
        if (count > 0U) {
            result = vk.enumerateInstanceExtensionProperties(nullptr, &count, values.data());
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkEnumerateInstanceExtensionProperties failed", result));
                return {};
            }
            values.resize(count);
        }
        return values;
    }

    [[nodiscard]] std::vector<VkLayerProperties> enumerateInstanceLayers() {
        std::uint32_t count = 0U;
        VkResult result = vk.enumerateInstanceLayerProperties(&count, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkEnumerateInstanceLayerProperties failed", result));
            return {};
        }
        std::vector<VkLayerProperties> values(count);
        if (count > 0U) {
            result = vk.enumerateInstanceLayerProperties(&count, values.data());
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkEnumerateInstanceLayerProperties failed", result));
                return {};
            }
            values.resize(count);
        }
        return values;
    }

    [[nodiscard]] bool createVulkanInstance() {
        const std::vector<VkExtensionProperties> availableExtensions = enumerateInstanceExtensions();
        const std::vector<VkLayerProperties> availableLayers = enumerateInstanceLayers();
        if (state == RendererState::Failed) {
            return false;
        }

        std::uint32_t sdlExtensionCount = 0U;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (sdlExtensions == nullptr) {
            return fail(makeSdlError("SDL_Vulkan_GetInstanceExtensions failed"));
        }

        std::vector<const char*> enabledExtensions;
        enabledExtensions.reserve(static_cast<std::size_t>(sdlExtensionCount) + 1U);
        for (std::uint32_t index = 0U; index < sdlExtensionCount; ++index) {
            enabledExtensions.push_back(sdlExtensions[index]);
        }

        constexpr const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
        validationEnabled = config.requestValidation && hasLayer(availableLayers, validationLayerName);
        const bool debugUtilsAvailable = hasExtension(availableExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (validationEnabled && debugUtilsAvailable) {
            enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        std::vector<const char*> enabledLayers;
        if (validationEnabled) {
            enabledLayers.push_back(validationLayerName);
        }

        std::uint32_t runtimeVersion = VK_API_VERSION_1_0;
        if (vk.enumerateInstanceVersion != nullptr) {
            const VkResult result = vk.enumerateInstanceVersion(&runtimeVersion);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkEnumerateInstanceVersion failed", result));
            }
        }

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = config.applicationName.c_str();
        applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0U, 0U, 6U, 0U);
        applicationInfo.pEngineName = "TENEBRIS Engine";
        applicationInfo.engineVersion = VK_MAKE_API_VERSION(0U, 0U, 6U, 0U);
        applicationInfo.apiVersion = std::min(runtimeVersion, VK_API_VERSION_1_2);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (validationEnabled && debugUtilsAvailable) {
            debugCreateInfo = makeDebugMessengerCreateInfo();
            createInfo.pNext = &debugCreateInfo;
        }

        const VkResult result = vk.createInstance(&createInfo, nullptr, &instance);
        return result == VK_SUCCESS || fail(makeVulkanError("vkCreateInstance failed", result));
    }

    [[nodiscard]] bool loadInstanceFunctions() {
        const bool loaded = vk.loadInstance(vk.destroyInstance, instance, "vkDestroyInstance")
            && vk.loadInstance(vk.enumeratePhysicalDevices, instance, "vkEnumeratePhysicalDevices")
            && vk.loadInstance(vk.getPhysicalDeviceProperties, instance, "vkGetPhysicalDeviceProperties")
            && vk.loadInstance(
                vk.getPhysicalDeviceMemoryProperties,
                instance,
                "vkGetPhysicalDeviceMemoryProperties"
            )
            && vk.loadInstance(
                vk.getPhysicalDeviceFormatProperties,
                instance,
                "vkGetPhysicalDeviceFormatProperties"
            )
            && vk.loadInstance(
                vk.getPhysicalDeviceQueueFamilyProperties,
                instance,
                "vkGetPhysicalDeviceQueueFamilyProperties"
            )
            && vk.loadInstance(
                vk.enumerateDeviceExtensionProperties,
                instance,
                "vkEnumerateDeviceExtensionProperties"
            )
            && vk.loadInstance(vk.createDevice, instance, "vkCreateDevice")
            && vk.loadInstance(vk.getDeviceProcAddr, instance, "vkGetDeviceProcAddr")
            && vk.loadInstance(
                vk.getPhysicalDeviceSurfaceSupport,
                instance,
                "vkGetPhysicalDeviceSurfaceSupportKHR"
            )
            && vk.loadInstance(
                vk.getPhysicalDeviceSurfaceCapabilities,
                instance,
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
            )
            && vk.loadInstance(
                vk.getPhysicalDeviceSurfaceFormats,
                instance,
                "vkGetPhysicalDeviceSurfaceFormatsKHR"
            )
            && vk.loadInstance(
                vk.getPhysicalDeviceSurfacePresentModes,
                instance,
                "vkGetPhysicalDeviceSurfacePresentModesKHR"
            );

        static_cast<void>(
            vk.loadInstance(vk.createDebugUtilsMessenger, instance, "vkCreateDebugUtilsMessengerEXT")
        );
        static_cast<void>(
            vk.loadInstance(vk.destroyDebugUtilsMessenger, instance, "vkDestroyDebugUtilsMessengerEXT")
        );
        return loaded || fail("Vulkan loader is missing required instance functions.");
    }

    [[nodiscard]] bool createDebugMessenger() {
        if (!validationEnabled || vk.createDebugUtilsMessenger == nullptr) {
            return true;
        }
        const VkDebugUtilsMessengerCreateInfoEXT createInfo = makeDebugMessengerCreateInfo();
        const VkResult result = vk.createDebugUtilsMessenger(instance, &createInfo, nullptr, &debugMessenger);
        return result == VK_SUCCESS
            || fail(makeVulkanError("vkCreateDebugUtilsMessengerEXT failed", result));
    }

    [[nodiscard]] bool deviceSupportsSwapchain(VkPhysicalDevice candidate) {
        std::uint32_t count = 0U;
        VkResult result = vk.enumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
        if (result != VK_SUCCESS) {
            return false;
        }
        std::vector<VkExtensionProperties> extensions(count);
        if (count > 0U) {
            result = vk.enumerateDeviceExtensionProperties(candidate, nullptr, &count, extensions.data());
            if (result != VK_SUCCESS) {
                return false;
            }
            extensions.resize(count);
        }
        return hasExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    [[nodiscard]] SwapchainSupport querySwapchainSupport(VkPhysicalDevice candidate) {
        SwapchainSupport support{};
        VkResult result = vk.getPhysicalDeviceSurfaceCapabilities(candidate, surface, &support.capabilities);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed", result));
            return {};
        }

        std::uint32_t formatCount = 0U;
        result = vk.getPhysicalDeviceSurfaceFormats(candidate, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkGetPhysicalDeviceSurfaceFormatsKHR failed", result));
            return {};
        }
        support.formats.resize(formatCount);
        if (formatCount > 0U) {
            result = vk.getPhysicalDeviceSurfaceFormats(candidate, surface, &formatCount, support.formats.data());
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkGetPhysicalDeviceSurfaceFormatsKHR failed", result));
                return {};
            }
            support.formats.resize(formatCount);
        }

        std::uint32_t modeCount = 0U;
        result = vk.getPhysicalDeviceSurfacePresentModes(candidate, surface, &modeCount, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed", result));
            return {};
        }
        support.presentModes.resize(modeCount);
        if (modeCount > 0U) {
            result = vk.getPhysicalDeviceSurfacePresentModes(
                candidate,
                surface,
                &modeCount,
                support.presentModes.data()
            );
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed", result));
                return {};
            }
            support.presentModes.resize(modeCount);
        }
        return support;
    }

    [[nodiscard]] std::optional<DeviceCandidate> evaluateDevice(VkPhysicalDevice candidate) {
        if (!deviceSupportsSwapchain(candidate)) {
            return std::nullopt;
        }

        std::uint32_t familyCount = 0U;
        vk.getPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        if (familyCount == 0U) {
            return std::nullopt;
        }
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vk.getPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        DeviceCandidate evaluated{};
        evaluated.device = candidate;
        for (std::uint32_t index = 0U; index < familyCount; ++index) {
            if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U
                && evaluated.graphicsQueueFamily == invalidQueueFamily) {
                evaluated.graphicsQueueFamily = index;
            }
            VkBool32 supportsPresent = VK_FALSE;
            if (vk.getPhysicalDeviceSurfaceSupport(candidate, index, surface, &supportsPresent) != VK_SUCCESS) {
                return std::nullopt;
            }
            if (supportsPresent == VK_TRUE && evaluated.presentQueueFamily == invalidQueueFamily) {
                evaluated.presentQueueFamily = index;
            }
        }

        if (evaluated.graphicsQueueFamily == invalidQueueFamily
            || evaluated.presentQueueFamily == invalidQueueFamily) {
            return std::nullopt;
        }
        const SwapchainSupport support = querySwapchainSupport(candidate);
        if (state == RendererState::Failed || support.formats.empty() || support.presentModes.empty()) {
            return std::nullopt;
        }

        VkPhysicalDeviceProperties properties{};
        vk.getPhysicalDeviceProperties(candidate, &properties);
        evaluated.score = properties.limits.maxImageDimension2D;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            evaluated.score += 10000U;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            evaluated.score += 5000U;
        }
        return evaluated;
    }

    [[nodiscard]] bool selectPhysicalDevice() {
        std::uint32_t count = 0U;
        VkResult result = vk.enumeratePhysicalDevices(instance, &count, nullptr);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkEnumeratePhysicalDevices failed", result));
        }
        if (count == 0U) {
            return fail("No Vulkan physical device is available.");
        }

        std::vector<VkPhysicalDevice> devices(count);
        result = vk.enumeratePhysicalDevices(instance, &count, devices.data());
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkEnumeratePhysicalDevices failed", result));
        }
        devices.resize(count);

        std::optional<DeviceCandidate> best;
        for (VkPhysicalDevice candidate : devices) {
            const std::optional<DeviceCandidate> evaluated = evaluateDevice(candidate);
            if (state == RendererState::Failed) {
                return false;
            }
            if (evaluated.has_value() && (!best.has_value() || evaluated->score > best->score)) {
                best = evaluated;
            }
        }
        if (!best.has_value()) {
            return fail("No Vulkan device supports graphics, presentation and swapchain requirements.");
        }

        physicalDevice = best->device;
        graphicsQueueFamily = best->graphicsQueueFamily;
        presentQueueFamily = best->presentQueueFamily;
        return true;
    }

    [[nodiscard]] bool createLogicalDevice() {
        constexpr float priority = 1.0F;
        const std::array<std::uint32_t, 2U> queueFamilies{graphicsQueueFamily, presentQueueFamily};
        const std::size_t uniqueCount = graphicsQueueFamily == presentQueueFamily ? 1U : 2U;
        std::array<VkDeviceQueueCreateInfo, 2U> queueInfos{};
        for (std::size_t index = 0U; index < uniqueCount; ++index) {
            queueInfos[index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfos[index].queueFamilyIndex = queueFamilies[index];
            queueInfos[index].queueCount = 1U;
            queueInfos[index].pQueuePriorities = &priority;
        }

        constexpr std::array<const char*, 1U> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(uniqueCount);
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        const VkResult result = vk.createDevice(physicalDevice, &createInfo, nullptr, &device);
        return result == VK_SUCCESS || fail(makeVulkanError("vkCreateDevice failed", result));
    }

    [[nodiscard]] bool loadDeviceFunctions() {
#define TENEBRIS_LOAD_DEVICE(member, name) vk.loadDevice(vk.member, device, name)
        const bool loaded = TENEBRIS_LOAD_DEVICE(destroyDevice, "vkDestroyDevice")
            && TENEBRIS_LOAD_DEVICE(getDeviceQueue, "vkGetDeviceQueue")
            && TENEBRIS_LOAD_DEVICE(createSwapchain, "vkCreateSwapchainKHR")
            && TENEBRIS_LOAD_DEVICE(destroySwapchain, "vkDestroySwapchainKHR")
            && TENEBRIS_LOAD_DEVICE(getSwapchainImages, "vkGetSwapchainImagesKHR")
            && TENEBRIS_LOAD_DEVICE(createImageView, "vkCreateImageView")
            && TENEBRIS_LOAD_DEVICE(destroyImageView, "vkDestroyImageView")
            && TENEBRIS_LOAD_DEVICE(createRenderPass, "vkCreateRenderPass")
            && TENEBRIS_LOAD_DEVICE(destroyRenderPass, "vkDestroyRenderPass")
            && TENEBRIS_LOAD_DEVICE(createFramebuffer, "vkCreateFramebuffer")
            && TENEBRIS_LOAD_DEVICE(destroyFramebuffer, "vkDestroyFramebuffer")
            && TENEBRIS_LOAD_DEVICE(createCommandPool, "vkCreateCommandPool")
            && TENEBRIS_LOAD_DEVICE(destroyCommandPool, "vkDestroyCommandPool")
            && TENEBRIS_LOAD_DEVICE(allocateCommandBuffers, "vkAllocateCommandBuffers")
            && TENEBRIS_LOAD_DEVICE(freeCommandBuffers, "vkFreeCommandBuffers")
            && TENEBRIS_LOAD_DEVICE(resetCommandBuffer, "vkResetCommandBuffer")
            && TENEBRIS_LOAD_DEVICE(beginCommandBuffer, "vkBeginCommandBuffer")
            && TENEBRIS_LOAD_DEVICE(endCommandBuffer, "vkEndCommandBuffer")
            && TENEBRIS_LOAD_DEVICE(cmdBeginRenderPass, "vkCmdBeginRenderPass")
            && TENEBRIS_LOAD_DEVICE(cmdEndRenderPass, "vkCmdEndRenderPass")
            && TENEBRIS_LOAD_DEVICE(cmdCopyBuffer, "vkCmdCopyBuffer")
            && TENEBRIS_LOAD_DEVICE(cmdBindPipeline, "vkCmdBindPipeline")
            && TENEBRIS_LOAD_DEVICE(cmdSetViewport, "vkCmdSetViewport")
            && TENEBRIS_LOAD_DEVICE(cmdSetScissor, "vkCmdSetScissor")
            && TENEBRIS_LOAD_DEVICE(cmdBindVertexBuffers, "vkCmdBindVertexBuffers")
            && TENEBRIS_LOAD_DEVICE(cmdBindIndexBuffer, "vkCmdBindIndexBuffer")
            && TENEBRIS_LOAD_DEVICE(cmdPushConstants, "vkCmdPushConstants")
            && TENEBRIS_LOAD_DEVICE(cmdDrawIndexed, "vkCmdDrawIndexed")
            && TENEBRIS_LOAD_DEVICE(createSemaphore, "vkCreateSemaphore")
            && TENEBRIS_LOAD_DEVICE(destroySemaphore, "vkDestroySemaphore")
            && TENEBRIS_LOAD_DEVICE(createFence, "vkCreateFence")
            && TENEBRIS_LOAD_DEVICE(destroyFence, "vkDestroyFence")
            && TENEBRIS_LOAD_DEVICE(waitForFences, "vkWaitForFences")
            && TENEBRIS_LOAD_DEVICE(resetFences, "vkResetFences")
            && TENEBRIS_LOAD_DEVICE(acquireNextImage, "vkAcquireNextImageKHR")
            && TENEBRIS_LOAD_DEVICE(queueSubmit, "vkQueueSubmit")
            && TENEBRIS_LOAD_DEVICE(queuePresent, "vkQueuePresentKHR")
            && TENEBRIS_LOAD_DEVICE(queueWaitIdle, "vkQueueWaitIdle")
            && TENEBRIS_LOAD_DEVICE(deviceWaitIdle, "vkDeviceWaitIdle")
            && TENEBRIS_LOAD_DEVICE(createBuffer, "vkCreateBuffer")
            && TENEBRIS_LOAD_DEVICE(destroyBuffer, "vkDestroyBuffer")
            && TENEBRIS_LOAD_DEVICE(getBufferMemoryRequirements, "vkGetBufferMemoryRequirements")
            && TENEBRIS_LOAD_DEVICE(allocateMemory, "vkAllocateMemory")
            && TENEBRIS_LOAD_DEVICE(freeMemory, "vkFreeMemory")
            && TENEBRIS_LOAD_DEVICE(bindBufferMemory, "vkBindBufferMemory")
            && TENEBRIS_LOAD_DEVICE(mapMemory, "vkMapMemory")
            && TENEBRIS_LOAD_DEVICE(unmapMemory, "vkUnmapMemory")
            && TENEBRIS_LOAD_DEVICE(createImage, "vkCreateImage")
            && TENEBRIS_LOAD_DEVICE(destroyImage, "vkDestroyImage")
            && TENEBRIS_LOAD_DEVICE(getImageMemoryRequirements, "vkGetImageMemoryRequirements")
            && TENEBRIS_LOAD_DEVICE(bindImageMemory, "vkBindImageMemory")
            && TENEBRIS_LOAD_DEVICE(createShaderModule, "vkCreateShaderModule")
            && TENEBRIS_LOAD_DEVICE(destroyShaderModule, "vkDestroyShaderModule")
            && TENEBRIS_LOAD_DEVICE(createPipelineLayout, "vkCreatePipelineLayout")
            && TENEBRIS_LOAD_DEVICE(destroyPipelineLayout, "vkDestroyPipelineLayout")
            && TENEBRIS_LOAD_DEVICE(createGraphicsPipelines, "vkCreateGraphicsPipelines")
            && TENEBRIS_LOAD_DEVICE(destroyPipeline, "vkDestroyPipeline");
#undef TENEBRIS_LOAD_DEVICE
        return loaded || fail("Vulkan loader is missing required device functions.");
    }

    [[nodiscard]] std::optional<VkExtent2D> windowExtent() {
        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
            fail(makeSdlError("SDL_GetWindowSizeInPixels failed"));
            return std::nullopt;
        }
        if (width <= 0 || height <= 0) {
            return VkExtent2D{0U, 0U};
        }
        return VkExtent2D{
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
        };
    }

    [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats
    ) const {
        const auto preferred = std::find_if(
            formats.begin(),
            formats.end(),
            [](const VkSurfaceFormatKHR& format) {
                return format.format == VK_FORMAT_B8G8R8A8_SRGB
                    && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }
        );
        return preferred != formats.end() ? *preferred : formats.front();
    }

    [[nodiscard]] VkPresentModeKHR choosePresentMode(
        const std::vector<VkPresentModeKHR>& modes
    ) const {
        const auto mailbox = std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
        return mailbox != modes.end() ? *mailbox : VK_PRESENT_MODE_FIFO_KHR;
    }

    [[nodiscard]] std::optional<VkExtent2D> chooseExtent(
        const VkSurfaceCapabilitiesKHR& capabilities
    ) {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        std::optional<VkExtent2D> extent = windowExtent();
        if (!extent.has_value()) {
            return std::nullopt;
        }
        extent->width = std::clamp(
            extent->width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        );
        extent->height = std::clamp(
            extent->height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );
        return extent;
    }

    [[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
        VkCompositeAlphaFlagsKHR supported
    ) const {
        constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4U> candidates{
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        for (VkCompositeAlphaFlagBitsKHR candidate : candidates) {
            if ((supported & candidate) != 0U) {
                return candidate;
            }
        }
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    [[nodiscard]] std::optional<std::uint32_t> findMemoryType(
        std::uint32_t typeBits,
        VkMemoryPropertyFlags required
    ) const noexcept {
        for (std::uint32_t index = 0U; index < memoryProperties.memoryTypeCount; ++index) {
            const bool supported = (typeBits & (1U << index)) != 0U;
            const bool matches = (memoryProperties.memoryTypes[index].propertyFlags & required) == required;
            if (supported && matches) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool createBufferAllocation(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        BufferAllocation& allocation
    ) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vk.createBuffer(device, &bufferInfo, nullptr, &allocation.buffer);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateBuffer failed", result));
        }

        VkMemoryRequirements requirements{};
        vk.getBufferMemoryRequirements(device, allocation.buffer, &requirements);
        const std::optional<std::uint32_t> memoryType = findMemoryType(
            requirements.memoryTypeBits,
            properties
        );
        if (!memoryType.has_value()) {
            return fail("No compatible Vulkan memory type was found for a buffer.");
        }

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = *memoryType;
        result = vk.allocateMemory(device, &allocateInfo, nullptr, &allocation.memory);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkAllocateMemory failed", result));
        }
        result = vk.bindBufferMemory(device, allocation.buffer, allocation.memory, 0U);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkBindBufferMemory failed", result));
        }
        allocation.size = size;
        return true;
    }

    void destroyBufferAllocation(BufferAllocation& allocation) noexcept {
        if (device != VK_NULL_HANDLE && allocation.buffer != VK_NULL_HANDLE) {
            vk.destroyBuffer(device, allocation.buffer, nullptr);
        }
        if (device != VK_NULL_HANDLE && allocation.memory != VK_NULL_HANDLE) {
            vk.freeMemory(device, allocation.memory, nullptr);
        }
        allocation = {};
    }

    [[nodiscard]] bool uploadSceneBuffer() {
        if (!hasScene || !uploadPlan.drawable()) {
            return true;
        }

        const VkDeviceSize totalSize = static_cast<VkDeviceSize>(uploadPlan.stagingSize);
        BufferAllocation staging{};
        if (!createBufferAllocation(
                totalSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                staging
            )) {
            destroyBufferAllocation(staging);
            return false;
        }

        void* mapped = nullptr;
        VkResult result = vk.mapMemory(device, staging.memory, 0U, totalSize, 0U, &mapped);
        if (result != VK_SUCCESS || mapped == nullptr) {
            destroyBufferAllocation(staging);
            return fail(makeVulkanError("vkMapMemory failed", result));
        }

        std::memset(mapped, 0, static_cast<std::size_t>(uploadPlan.stagingSize));
        auto* bytes = static_cast<std::byte*>(mapped);
        std::memcpy(
            bytes + static_cast<std::size_t>(uploadPlan.vertexRegion.offset),
            sceneMesh.vertices.data(),
            static_cast<std::size_t>(uploadPlan.vertexRegion.size)
        );
        std::memcpy(
            bytes + static_cast<std::size_t>(uploadPlan.indexRegion.offset),
            sceneMesh.indices.data(),
            static_cast<std::size_t>(uploadPlan.indexRegion.size)
        );
        vk.unmapMemory(device, staging.memory);

        if (!createBufferAllocation(
                totalSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                sceneBuffer
            )) {
            destroyBufferAllocation(staging);
            destroySceneBuffer();
            return false;
        }

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1U;
        result = vk.allocateCommandBuffers(device, &allocateInfo, &commandBuffer);
        if (result != VK_SUCCESS) {
            destroyBufferAllocation(staging);
            destroySceneBuffer();
            return fail(makeVulkanError("vkAllocateCommandBuffers failed", result));
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vk.beginCommandBuffer(commandBuffer, &beginInfo);
        if (result == VK_SUCCESS) {
            VkBufferCopy copy{};
            copy.size = totalSize;
            vk.cmdCopyBuffer(commandBuffer, staging.buffer, sceneBuffer.buffer, 1U, &copy);
            result = vk.endCommandBuffer(commandBuffer);
        }
        if (result == VK_SUCCESS) {
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1U;
            submitInfo.pCommandBuffers = &commandBuffer;
            result = vk.queueSubmit(graphicsQueue, 1U, &submitInfo, VK_NULL_HANDLE);
        }
        if (result == VK_SUCCESS) {
            result = vk.queueWaitIdle(graphicsQueue);
        }

        vk.freeCommandBuffers(device, commandPool, 1U, &commandBuffer);
        destroyBufferAllocation(staging);
        if (result != VK_SUCCESS) {
            destroySceneBuffer();
            return fail(makeVulkanError("Voxel scene GPU upload failed", result));
        }
        return true;
    }

    void destroySceneBuffer() noexcept {
        destroyBufferAllocation(sceneBuffer);
        uploadedRevision = 0U;
    }

    [[nodiscard]] std::optional<VkFormat> chooseDepthFormat() const noexcept {
        constexpr std::array<VkFormat, 3U> candidates{
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        for (VkFormat candidate : candidates) {
            VkFormatProperties properties{};
            vk.getPhysicalDeviceFormatProperties(physicalDevice, candidate, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U) {
                return candidate;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool createSwapchainResources() {
        const SwapchainSupport support = querySwapchainSupport(physicalDevice);
        if (state == RendererState::Failed) {
            return false;
        }
        if (support.formats.empty() || support.presentModes.empty()) {
            return fail("The selected Vulkan device no longer exposes a usable swapchain.");
        }

        const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
        const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
        const std::optional<VkExtent2D> extent = chooseExtent(support.capabilities);
        if (!extent.has_value()) {
            return false;
        }
        if (extent->width == 0U || extent->height == 0U) {
            state = RendererState::Suspended;
            return true;
        }

        std::uint32_t imageCount = support.capabilities.minImageCount + 1U;
        if (support.capabilities.maxImageCount > 0U) {
            imageCount = std::min(imageCount, support.capabilities.maxImageCount);
        }
        const std::array<std::uint32_t, 2U> queueFamilyIndices{
            graphicsQueueFamily,
            presentQueueFamily,
        };

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = *extent;
        createInfo.imageArrayLayers = 1U;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (graphicsQueueFamily != presentQueueFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = chooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        VkResult result = vk.createSwapchain(device, &createInfo, nullptr, &swapchain);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateSwapchainKHR failed", result));
        }
        swapchainFormat = surfaceFormat.format;
        swapchainExtent = *extent;
        refreshPushConstants();

        result = vk.getSwapchainImages(device, swapchain, &imageCount, nullptr);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkGetSwapchainImagesKHR failed", result));
        }
        swapchainImages.resize(imageCount);
        result = vk.getSwapchainImages(device, swapchain, &imageCount, swapchainImages.data());
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkGetSwapchainImagesKHR failed", result));
        }
        swapchainImages.resize(imageCount);

        const std::optional<VkFormat> selectedDepthFormat = chooseDepthFormat();
        if (!selectedDepthFormat.has_value()) {
            return fail("No supported Vulkan depth format is available.");
        }
        depthFormat = *selectedDepthFormat;

        if (!createSwapchainImageViews() || !createDepthResources() || !createRenderPass()
            || !createGraphicsPipeline() || !createFramebuffers()) {
            return false;
        }

        imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);
        state = RendererState::Ready;
        return true;
    }

    [[nodiscard]] bool createSwapchainImageViews() {
        swapchainImageViews.reserve(swapchainImages.size());
        for (VkImage image : swapchainImages) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchainFormat;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.levelCount = 1U;
            createInfo.subresourceRange.layerCount = 1U;

            VkImageView imageView = VK_NULL_HANDLE;
            const VkResult result = vk.createImageView(device, &createInfo, nullptr, &imageView);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateImageView failed", result));
            }
            swapchainImageViews.push_back(imageView);
        }
        return true;
    }

    [[nodiscard]] bool createDepthResources() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {swapchainExtent.width, swapchainExtent.height, 1U};
        imageInfo.mipLevels = 1U;
        imageInfo.arrayLayers = 1U;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vk.createImage(device, &imageInfo, nullptr, &depthImage);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateImage failed for depth buffer", result));
        }

        VkMemoryRequirements requirements{};
        vk.getImageMemoryRequirements(device, depthImage, &requirements);
        const std::optional<std::uint32_t> memoryType = findMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        if (!memoryType.has_value()) {
            return fail("No compatible Vulkan memory type was found for the depth image.");
        }

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = requirements.size;
        allocateInfo.memoryTypeIndex = *memoryType;
        result = vk.allocateMemory(device, &allocateInfo, nullptr, &depthMemory);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkAllocateMemory failed for depth buffer", result));
        }
        result = vk.bindImageMemory(device, depthImage, depthMemory, 0U);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkBindImageMemory failed", result));
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(depthFormat)) {
            viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        viewInfo.subresourceRange.levelCount = 1U;
        viewInfo.subresourceRange.layerCount = 1U;
        result = vk.createImageView(device, &viewInfo, nullptr, &depthImageView);
        return result == VK_SUCCESS
            || fail(makeVulkanError("vkCreateImageView failed for depth buffer", result));
    }

    [[nodiscard]] bool createRenderPass() {
        std::array<VkAttachmentDescription, 2U> attachments{};
        attachments[0].format = swapchainFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[1].format = depthFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        const VkAttachmentReference colorReference{0U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        const VkAttachmentReference depthReference{1U, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1U;
        subpass.pColorAttachments = &colorReference;
        subpass.pDepthStencilAttachment = &depthReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0U;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = dependency.srcStageMask;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1U;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1U;
        createInfo.pDependencies = &dependency;
        const VkResult result = vk.createRenderPass(device, &createInfo, nullptr, &renderPass);
        return result == VK_SUCCESS || fail(makeVulkanError("vkCreateRenderPass failed", result));
    }

    [[nodiscard]] bool createShaderModule(
        const std::vector<std::uint32_t>& words,
        VkShaderModule& module
    ) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = words.size() * sizeof(std::uint32_t);
        createInfo.pCode = words.data();
        const VkResult result = vk.createShaderModule(device, &createInfo, nullptr, &module);
        return result == VK_SUCCESS || fail(makeVulkanError("vkCreateShaderModule failed", result));
    }

    [[nodiscard]] bool createGraphicsPipeline() {
        std::string shaderError;
        const std::filesystem::path shaderDirectory{config.shaderDirectory};
        const auto vertexWords = loadSpirvFile(shaderDirectory / "voxel.vert.spv", shaderError);
        if (!vertexWords.has_value()) {
            return fail(shaderError);
        }
        const auto fragmentWords = loadSpirvFile(shaderDirectory / "voxel.frag.spv", shaderError);
        if (!fragmentWords.has_value()) {
            return fail(shaderError);
        }

        VkShaderModule vertexModule = VK_NULL_HANDLE;
        VkShaderModule fragmentModule = VK_NULL_HANDLE;
        if (!createShaderModule(*vertexWords, vertexModule)
            || !createShaderModule(*fragmentWords, fragmentModule)) {
            if (vertexModule != VK_NULL_HANDLE) {
                vk.destroyShaderModule(device, vertexModule, nullptr);
            }
            if (fragmentModule != VK_NULL_HANDLE) {
                vk.destroyShaderModule(device, fragmentModule, nullptr);
            }
            return false;
        }

        const std::array<VkPipelineShaderStageCreateInfo, 2U> stages{
            VkPipelineShaderStageCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0U,
                VK_SHADER_STAGE_VERTEX_BIT,
                vertexModule,
                "main",
                nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0U,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                fragmentModule,
                "main",
                nullptr,
            },
        };

        const VkVertexInputBindingDescription binding{
            0U,
            static_cast<std::uint32_t>(sizeof(scene::GpuVoxelVertex)),
            VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const std::array<VkVertexInputAttributeDescription, 3U> attributes{
            VkVertexInputAttributeDescription{
                scene::voxelPositionLocation,
                0U,
                VK_FORMAT_R32G32B32_SFLOAT,
                static_cast<std::uint32_t>(offsetof(scene::GpuVoxelVertex, x)),
            },
            VkVertexInputAttributeDescription{
                scene::voxelNormalLocation,
                0U,
                VK_FORMAT_R8G8B8A8_SNORM,
                static_cast<std::uint32_t>(offsetof(scene::GpuVoxelVertex, normalX)),
            },
            VkVertexInputAttributeDescription{
                scene::voxelMaterialLocation,
                0U,
                VK_FORMAT_R16_UINT,
                static_cast<std::uint32_t>(offsetof(scene::GpuVoxelVertex, material)),
            },
        };

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1U;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1U;
        viewportState.scissorCount = 1U;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0F;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1U;
        colorBlend.pAttachments = &colorBlendAttachment;

        constexpr std::array<VkDynamicState, 2U> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.size = static_cast<std::uint32_t>(sizeof(scene::VoxelPushConstants));
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pushConstantRangeCount = 1U;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        VkResult result = vk.createPipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
        if (result != VK_SUCCESS) {
            vk.destroyShaderModule(device, vertexModule, nullptr);
            vk.destroyShaderModule(device, fragmentModule, nullptr);
            return fail(makeVulkanError("vkCreatePipelineLayout failed", result));
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0U;
        result = vk.createGraphicsPipelines(
            device,
            VK_NULL_HANDLE,
            1U,
            &pipelineInfo,
            nullptr,
            &pipeline
        );

        vk.destroyShaderModule(device, vertexModule, nullptr);
        vk.destroyShaderModule(device, fragmentModule, nullptr);
        return result == VK_SUCCESS
            || fail(makeVulkanError("vkCreateGraphicsPipelines failed", result));
    }

    [[nodiscard]] bool createFramebuffers() {
        framebuffers.reserve(swapchainImageViews.size());
        for (VkImageView colorImageView : swapchainImageViews) {
            const std::array<VkImageView, 2U> attachments{colorImageView, depthImageView};
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass;
            createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            createInfo.width = swapchainExtent.width;
            createInfo.height = swapchainExtent.height;
            createInfo.layers = 1U;

            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            const VkResult result = vk.createFramebuffer(device, &createInfo, nullptr, &framebuffer);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateFramebuffer failed", result));
            }
            framebuffers.push_back(framebuffer);
        }
        return true;
    }

    [[nodiscard]] bool createCommandResources() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;
        VkResult result = vk.createCommandPool(device, &poolInfo, nullptr, &commandPool);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateCommandPool failed", result));
        }

        commandBuffers.resize(config.framesInFlight);
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());
        result = vk.allocateCommandBuffers(device, &allocateInfo, commandBuffers.data());
        return result == VK_SUCCESS
            || fail(makeVulkanError("vkAllocateCommandBuffers failed", result));
    }

    [[nodiscard]] bool createSynchronization() {
        frameSync.resize(config.framesInFlight);
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (FrameSync& frame : frameSync) {
            VkResult result = vk.createSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable);
            if (result == VK_SUCCESS) {
                result = vk.createSemaphore(device, &semaphoreInfo, nullptr, &frame.renderFinished);
            }
            if (result == VK_SUCCESS) {
                result = vk.createFence(device, &fenceInfo, nullptr, &frame.inFlight);
            }
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("Vulkan frame synchronization creation failed", result));
            }
        }
        return true;
    }

    [[nodiscard]] bool recordCommandBuffer(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vk.beginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkBeginCommandBuffer failed", result));
        }

        std::array<VkClearValue, 2U> clearValues{};
        clearValues[0].color.float32[0] = 0.008F;
        clearValues[0].color.float32[1] = 0.006F;
        clearValues[0].color.float32[2] = 0.009F;
        clearValues[0].color.float32[3] = 1.0F;
        clearValues[1].depthStencil = {1.0F, 0U};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.extent = swapchainExtent;
        renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        vk.cmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        if (hasScene && sceneBuffer.buffer != VK_NULL_HANDLE && uploadPlan.drawable()) {
            vk.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            const VkViewport viewport{
                0.0F,
                0.0F,
                static_cast<float>(swapchainExtent.width),
                static_cast<float>(swapchainExtent.height),
                0.0F,
                1.0F,
            };
            const VkRect2D scissor{{0, 0}, swapchainExtent};
            vk.cmdSetViewport(commandBuffer, 0U, 1U, &viewport);
            vk.cmdSetScissor(commandBuffer, 0U, 1U, &scissor);

            const VkDeviceSize vertexOffset = static_cast<VkDeviceSize>(uploadPlan.vertexRegion.offset);
            vk.cmdBindVertexBuffers(commandBuffer, 0U, 1U, &sceneBuffer.buffer, &vertexOffset);
            vk.cmdBindIndexBuffer(
                commandBuffer,
                sceneBuffer.buffer,
                static_cast<VkDeviceSize>(uploadPlan.indexRegion.offset),
                VK_INDEX_TYPE_UINT32
            );
            vk.cmdPushConstants(
                commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT,
                0U,
                static_cast<std::uint32_t>(sizeof(scene::VoxelPushConstants)),
                &pushConstants
            );
            vk.cmdDrawIndexed(
                commandBuffer,
                uploadPlan.draw.indexCount,
                uploadPlan.draw.instanceCount,
                uploadPlan.draw.firstIndex,
                uploadPlan.draw.vertexOffset,
                uploadPlan.draw.firstInstance
            );
        }

        vk.cmdEndRenderPass(commandBuffer);
        result = vk.endCommandBuffer(commandBuffer);
        return result == VK_SUCCESS || fail(makeVulkanError("vkEndCommandBuffer failed", result));
    }

    [[nodiscard]] bool recreateSwapchain() {
        const std::optional<VkExtent2D> extent = windowExtent();
        if (!extent.has_value()) {
            return false;
        }
        if (extent->width == 0U || extent->height == 0U) {
            state = RendererState::Suspended;
            return true;
        }

        const VkResult idleResult = vk.deviceWaitIdle(device);
        if (idleResult != VK_SUCCESS) {
            return fail(makeVulkanError("vkDeviceWaitIdle failed", idleResult));
        }
        destroySwapchainResources();
        if (!createSwapchainResources()) {
            return false;
        }
        framebufferResized = false;
        ++stats.swapchainRebuilds;
        return true;
    }

    [[nodiscard]] bool drawFrame() {
        if (state == RendererState::Headless) {
            return true;
        }
        if (state == RendererState::Suspended) {
            if (!recreateSwapchain() || state == RendererState::Suspended) {
                return state != RendererState::Failed;
            }
        }
        if (state != RendererState::Ready) {
            return false;
        }
        if (framebufferResized && !recreateSwapchain()) {
            return false;
        }
        if (state == RendererState::Suspended) {
            return true;
        }

        FrameSync& frame = frameSync[currentFrame];
        VkResult result = vk.waitForFences(device, 1U, &frame.inFlight, VK_TRUE, infiniteTimeout);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkWaitForFences failed", result));
        }

        std::uint32_t imageIndex = 0U;
        result = vk.acquireNextImage(
            device,
            swapchain,
            infiniteTimeout,
            frame.imageAvailable,
            VK_NULL_HANDLE,
            &imageIndex
        );
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return recreateSwapchain();
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            return fail(makeVulkanError("vkAcquireNextImageKHR failed", result));
        }
        const bool acquiredSuboptimal = result == VK_SUBOPTIMAL_KHR;

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            result = vk.waitForFences(
                device,
                1U,
                &imagesInFlight[imageIndex],
                VK_TRUE,
                infiniteTimeout
            );
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkWaitForFences failed", result));
            }
        }
        imagesInFlight[imageIndex] = frame.inFlight;

        result = vk.resetFences(device, 1U, &frame.inFlight);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkResetFences failed", result));
        }
        VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
        result = vk.resetCommandBuffer(commandBuffer, 0U);
        if (result != VK_SUCCESS || !recordCommandBuffer(commandBuffer, imageIndex)) {
            return result == VK_SUCCESS ? false : fail(makeVulkanError("vkResetCommandBuffer failed", result));
        }

        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1U;
        submitInfo.pWaitSemaphores = &frame.imageAvailable;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1U;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1U;
        submitInfo.pSignalSemaphores = &frame.renderFinished;
        result = vk.queueSubmit(graphicsQueue, 1U, &submitInfo, frame.inFlight);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkQueueSubmit failed", result));
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1U;
        presentInfo.pWaitSemaphores = &frame.renderFinished;
        presentInfo.swapchainCount = 1U;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        result = vk.queuePresent(presentQueue, &presentInfo);
        const bool requiresRebuild = result == VK_ERROR_OUT_OF_DATE_KHR
            || result == VK_SUBOPTIMAL_KHR || acquiredSuboptimal || framebufferResized;
        if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR
            && result != VK_SUBOPTIMAL_KHR) {
            return fail(makeVulkanError("vkQueuePresentKHR failed", result));
        }

        ++stats.submittedFrames;
        if (hasScene && sceneBuffer.buffer != VK_NULL_HANDLE) {
            ++stats.drawCalls;
            stats.drawnTriangles += uploadPlan.draw.indexCount / 3U;
        }
        currentFrame = (currentFrame + 1U) % config.framesInFlight;
        return !requiresRebuild || recreateSwapchain();
    }

    void destroySynchronization() noexcept {
        if (device == VK_NULL_HANDLE) {
            frameSync.clear();
            return;
        }
        for (FrameSync& frame : frameSync) {
            if (frame.inFlight != VK_NULL_HANDLE) {
                vk.destroyFence(device, frame.inFlight, nullptr);
            }
            if (frame.renderFinished != VK_NULL_HANDLE) {
                vk.destroySemaphore(device, frame.renderFinished, nullptr);
            }
            if (frame.imageAvailable != VK_NULL_HANDLE) {
                vk.destroySemaphore(device, frame.imageAvailable, nullptr);
            }
            frame = {};
        }
        frameSync.clear();
    }

    void destroySwapchainResources() noexcept {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        for (VkFramebuffer framebuffer : framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vk.destroyFramebuffer(device, framebuffer, nullptr);
            }
        }
        framebuffers.clear();

        if (pipeline != VK_NULL_HANDLE) {
            vk.destroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vk.destroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vk.destroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
        if (depthImageView != VK_NULL_HANDLE) {
            vk.destroyImageView(device, depthImageView, nullptr);
            depthImageView = VK_NULL_HANDLE;
        }
        if (depthImage != VK_NULL_HANDLE) {
            vk.destroyImage(device, depthImage, nullptr);
            depthImage = VK_NULL_HANDLE;
        }
        if (depthMemory != VK_NULL_HANDLE) {
            vk.freeMemory(device, depthMemory, nullptr);
            depthMemory = VK_NULL_HANDLE;
        }
        for (VkImageView imageView : swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vk.destroyImageView(device, imageView, nullptr);
            }
        }
        swapchainImageViews.clear();
        swapchainImages.clear();
        imagesInFlight.clear();
        if (swapchain != VK_NULL_HANDLE) {
            vk.destroySwapchain(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        swapchainFormat = VK_FORMAT_UNDEFINED;
        depthFormat = VK_FORMAT_UNDEFINED;
        swapchainExtent = {};
    }

    void waitIdle() noexcept {
        if (device != VK_NULL_HANDLE && vk.deviceWaitIdle != nullptr) {
            static_cast<void>(vk.deviceWaitIdle(device));
        }
    }

    void shutdown() noexcept {
        waitIdle();
        destroySynchronization();
        destroySceneBuffer();
        destroySwapchainResources();

        if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
            vk.destroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        commandBuffers.clear();
        if (device != VK_NULL_HANDLE && vk.destroyDevice != nullptr) {
            vk.destroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        graphicsQueue = VK_NULL_HANDLE;
        presentQueue = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        memoryProperties = {};
        graphicsQueueFamily = invalidQueueFamily;
        presentQueueFamily = invalidQueueFamily;

        if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (debugMessenger != VK_NULL_HANDLE && vk.destroyDebugUtilsMessenger != nullptr) {
            vk.destroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE && vk.destroyInstance != nullptr) {
            vk.destroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }

        vk = {};
        window = nullptr;
        headless = false;
        validationEnabled = false;
        framebufferResized = false;
        currentFrame = 0U;
        hasScene = false;
        sceneMesh = {};
        uploadPlan = {};
        state = RendererState::Stopped;
    }
};

RendererSystem::RendererSystem(RendererConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {
}

RendererSystem::~RendererSystem() {
    impl_->shutdown();
}

bool RendererSystem::initialize(SDL_Window* window, bool headless) {
    return impl_->initialize(window, headless);
}

bool RendererSystem::setVoxelScene(
    const scene::RenderMesh& mesh,
    const scene::SceneCamera& camera
) {
    return impl_->setVoxelScene(mesh, camera);
}

bool RendererSystem::drawFrame() {
    return impl_->drawFrame();
}

void RendererSystem::notifyFramebufferResized() noexcept {
    impl_->framebufferResized = true;
}

void RendererSystem::waitIdle() noexcept {
    impl_->waitIdle();
}

void RendererSystem::shutdown() noexcept {
    impl_->shutdown();
}

RendererState RendererSystem::state() const noexcept {
    return impl_->state;
}

bool RendererSystem::isValidationEnabled() const noexcept {
    return impl_->validationEnabled;
}

const RendererStats& RendererSystem::stats() const noexcept {
    return impl_->stats;
}

const std::string& RendererSystem::lastError() const noexcept {
    return impl_->lastError;
}

} // namespace tenebris::renderer
