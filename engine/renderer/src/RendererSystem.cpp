#include <tenebris/renderer/RendererSystem.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
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
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0U) {
        label = "VERBOSE";
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
    std::string message{context};
    message += ": ";
    message += SDL_GetError();
    return message;
}

[[nodiscard]] std::string makeVulkanError(const char* context, VkResult result) {
    std::string message{context};
    message += " (VkResult ";
    message += std::to_string(static_cast<std::int32_t>(result));
    message += ')';
    return message;
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

struct VulkanDispatch final {
    PFN_vkGetInstanceProcAddr getInstanceProcAddr{nullptr};
    PFN_vkEnumerateInstanceVersion enumerateInstanceVersion{nullptr};
    PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties{nullptr};
    PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties{nullptr};
    PFN_vkCreateInstance createInstance{nullptr};

    PFN_vkDestroyInstance destroyInstance{nullptr};
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices{nullptr};
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties{nullptr};
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
    PFN_vkResetCommandBuffer resetCommandBuffer{nullptr};
    PFN_vkBeginCommandBuffer beginCommandBuffer{nullptr};
    PFN_vkEndCommandBuffer endCommandBuffer{nullptr};
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass{nullptr};
    PFN_vkCmdEndRenderPass cmdEndRenderPass{nullptr};
    PFN_vkCreateSemaphore createSemaphore{nullptr};
    PFN_vkDestroySemaphore destroySemaphore{nullptr};
    PFN_vkCreateFence createFence{nullptr};
    PFN_vkDestroyFence destroyFence{nullptr};
    PFN_vkWaitForFences waitForFences{nullptr};
    PFN_vkResetFences resetFences{nullptr};
    PFN_vkAcquireNextImageKHR acquireNextImage{nullptr};
    PFN_vkQueueSubmit queueSubmit{nullptr};
    PFN_vkQueuePresentKHR queuePresent{nullptr};
    PFN_vkDeviceWaitIdle deviceWaitIdle{nullptr};

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
    VkRenderPass renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> framebuffers;

    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<FrameSync> frameSync;
    std::vector<VkFence> imagesInFlight;
    std::uint32_t currentFrame{0U};

    [[nodiscard]] bool fail(std::string message) {
        lastError = std::move(message);
        state = RendererState::Failed;
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

        if (!loadGlobalFunctions()) {
            return initializationFailure();
        }
        if (!createVulkanInstance()) {
            return initializationFailure();
        }
        if (!loadInstanceFunctions()) {
            return initializationFailure();
        }
        if (!createDebugMessenger()) {
            return initializationFailure();
        }
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            fail(makeSdlError("SDL_Vulkan_CreateSurface failed"));
            return initializationFailure();
        }
        if (!selectPhysicalDevice()) {
            return initializationFailure();
        }
        if (!createLogicalDevice()) {
            return initializationFailure();
        }
        if (!loadDeviceFunctions()) {
            return initializationFailure();
        }

        vk.getDeviceQueue(device, graphicsQueueFamily, 0U, &graphicsQueue);
        vk.getDeviceQueue(device, presentQueueFamily, 0U, &presentQueue);

        if (!createCommandResources()) {
            return initializationFailure();
        }
        if (!createSwapchainResources()) {
            return initializationFailure();
        }
        if (!createSynchronization()) {
            return initializationFailure();
        }

        state = RendererState::Ready;
        return true;
    }

    [[nodiscard]] bool initializationFailure() {
        const std::string error = lastError;
        shutdown();
        lastError = error;
        state = RendererState::Failed;
        return false;
    }

    [[nodiscard]] bool loadGlobalFunctions() {
        if (!vk.loadGlobal(vk.enumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties")
            || !vk.loadGlobal(vk.enumerateInstanceLayerProperties, "vkEnumerateInstanceLayerProperties")
            || !vk.loadGlobal(vk.createInstance, "vkCreateInstance")) {
            return fail("Vulkan loader is missing required global functions.");
        }

        static_cast<void>(vk.loadGlobal(vk.enumerateInstanceVersion, "vkEnumerateInstanceVersion"));
        return true;
    }

    [[nodiscard]] std::vector<VkExtensionProperties> enumerateInstanceExtensions() {
        std::uint32_t count = 0U;
        VkResult result = vk.enumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkEnumerateInstanceExtensionProperties failed", result));
            return {};
        }

        std::vector<VkExtensionProperties> extensions(count);
        if (count > 0U) {
            result = vk.enumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkEnumerateInstanceExtensionProperties failed", result));
                return {};
            }
            extensions.resize(count);
        }
        return extensions;
    }

    [[nodiscard]] std::vector<VkLayerProperties> enumerateInstanceLayers() {
        std::uint32_t count = 0U;
        VkResult result = vk.enumerateInstanceLayerProperties(&count, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkEnumerateInstanceLayerProperties failed", result));
            return {};
        }

        std::vector<VkLayerProperties> layers(count);
        if (count > 0U) {
            result = vk.enumerateInstanceLayerProperties(&count, layers.data());
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkEnumerateInstanceLayerProperties failed", result));
                return {};
            }
            layers.resize(count);
        }
        return layers;
    }

    [[nodiscard]] bool createVulkanInstance() {
        const std::vector<VkExtensionProperties> availableExtensions = enumerateInstanceExtensions();
        if (state == RendererState::Failed) {
            return false;
        }

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
        const std::uint32_t requestedApiVersion = std::min(runtimeVersion, VK_API_VERSION_1_2);

        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = config.applicationName.c_str();
        applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0U, 0U, 3U, 0U);
        applicationInfo.pEngineName = "TENEBRIS Engine";
        applicationInfo.engineVersion = VK_MAKE_API_VERSION(0U, 0U, 3U, 0U);
        applicationInfo.apiVersion = requestedApiVersion;

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
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateInstance failed", result));
        }
        return true;
    }

    [[nodiscard]] bool loadInstanceFunctions() {
        const bool loaded = vk.loadInstance(vk.destroyInstance, instance, "vkDestroyInstance")
            && vk.loadInstance(vk.enumeratePhysicalDevices, instance, "vkEnumeratePhysicalDevices")
            && vk.loadInstance(vk.getPhysicalDeviceProperties, instance, "vkGetPhysicalDeviceProperties")
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

        if (!loaded) {
            return fail("Vulkan loader is missing required instance functions.");
        }

        static_cast<void>(
            vk.loadInstance(vk.createDebugUtilsMessenger, instance, "vkCreateDebugUtilsMessengerEXT")
        );
        static_cast<void>(
            vk.loadInstance(vk.destroyDebugUtilsMessenger, instance, "vkDestroyDebugUtilsMessengerEXT")
        );
        return true;
    }

    [[nodiscard]] bool createDebugMessenger() {
        if (!validationEnabled || vk.createDebugUtilsMessenger == nullptr) {
            return true;
        }

        const VkDebugUtilsMessengerCreateInfoEXT createInfo = makeDebugMessengerCreateInfo();
        const VkResult result = vk.createDebugUtilsMessenger(instance, &createInfo, nullptr, &debugMessenger);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateDebugUtilsMessengerEXT failed", result));
        }
        return true;
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
            result = vk.getPhysicalDeviceSurfaceFormats(
                candidate,
                surface,
                &formatCount,
                support.formats.data()
            );
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkGetPhysicalDeviceSurfaceFormatsKHR failed", result));
                return {};
            }
            support.formats.resize(formatCount);
        }

        std::uint32_t presentModeCount = 0U;
        result = vk.getPhysicalDeviceSurfacePresentModes(candidate, surface, &presentModeCount, nullptr);
        if (result != VK_SUCCESS) {
            fail(makeVulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed", result));
            return {};
        }
        support.presentModes.resize(presentModeCount);
        if (presentModeCount > 0U) {
            result = vk.getPhysicalDeviceSurfacePresentModes(
                candidate,
                surface,
                &presentModeCount,
                support.presentModes.data()
            );
            if (result != VK_SUCCESS) {
                fail(makeVulkanError("vkGetPhysicalDeviceSurfacePresentModesKHR failed", result));
                return {};
            }
            support.presentModes.resize(presentModeCount);
        }
        return support;
    }

    [[nodiscard]] std::optional<DeviceCandidate> evaluateDevice(VkPhysicalDevice candidate) {
        if (!deviceSupportsSwapchain(candidate)) {
            return std::nullopt;
        }

        std::uint32_t queueFamilyCount = 0U;
        vk.getPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0U) {
            return std::nullopt;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vk.getPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());

        DeviceCandidate evaluated{};
        evaluated.device = candidate;
        for (std::uint32_t index = 0U; index < queueFamilyCount; ++index) {
            if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U
                && evaluated.graphicsQueueFamily == invalidQueueFamily) {
                evaluated.graphicsQueueFamily = index;
            }

            VkBool32 supportsPresent = VK_FALSE;
            const VkResult result = vk.getPhysicalDeviceSurfaceSupport(
                candidate,
                index,
                surface,
                &supportsPresent
            );
            if (result != VK_SUCCESS) {
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

        SwapchainSupport support = querySwapchainSupport(candidate);
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
        std::uint32_t deviceCount = 0U;
        VkResult result = vk.enumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkEnumeratePhysicalDevices failed", result));
        }
        if (deviceCount == 0U) {
            return fail("No Vulkan physical device is available.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        result = vk.enumeratePhysicalDevices(instance, &deviceCount, devices.data());
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkEnumeratePhysicalDevices failed", result));
        }
        devices.resize(deviceCount);

        std::optional<DeviceCandidate> best;
        for (VkPhysicalDevice candidate : devices) {
            std::optional<DeviceCandidate> evaluated = evaluateDevice(candidate);
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
        constexpr float queuePriority = 1.0F;
        std::array<std::uint32_t, 2U> queueFamilies{graphicsQueueFamily, presentQueueFamily};
        const std::size_t uniqueQueueFamilyCount = graphicsQueueFamily == presentQueueFamily ? 1U : 2U;

        std::array<VkDeviceQueueCreateInfo, 2U> queueCreateInfos{};
        for (std::size_t index = 0U; index < uniqueQueueFamilyCount; ++index) {
            queueCreateInfos[index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[index].queueFamilyIndex = queueFamilies[index];
            queueCreateInfos[index].queueCount = 1U;
            queueCreateInfos[index].pQueuePriorities = &queuePriority;
        }

        constexpr std::array<const char*, 1U> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkPhysicalDeviceFeatures enabledFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(uniqueQueueFamilyCount);
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.pEnabledFeatures = &enabledFeatures;

        const VkResult result = vk.createDevice(physicalDevice, &createInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateDevice failed", result));
        }
        return true;
    }

    [[nodiscard]] bool loadDeviceFunctions() {
        const bool loaded = vk.loadDevice(vk.destroyDevice, device, "vkDestroyDevice")
            && vk.loadDevice(vk.getDeviceQueue, device, "vkGetDeviceQueue")
            && vk.loadDevice(vk.createSwapchain, device, "vkCreateSwapchainKHR")
            && vk.loadDevice(vk.destroySwapchain, device, "vkDestroySwapchainKHR")
            && vk.loadDevice(vk.getSwapchainImages, device, "vkGetSwapchainImagesKHR")
            && vk.loadDevice(vk.createImageView, device, "vkCreateImageView")
            && vk.loadDevice(vk.destroyImageView, device, "vkDestroyImageView")
            && vk.loadDevice(vk.createRenderPass, device, "vkCreateRenderPass")
            && vk.loadDevice(vk.destroyRenderPass, device, "vkDestroyRenderPass")
            && vk.loadDevice(vk.createFramebuffer, device, "vkCreateFramebuffer")
            && vk.loadDevice(vk.destroyFramebuffer, device, "vkDestroyFramebuffer")
            && vk.loadDevice(vk.createCommandPool, device, "vkCreateCommandPool")
            && vk.loadDevice(vk.destroyCommandPool, device, "vkDestroyCommandPool")
            && vk.loadDevice(vk.allocateCommandBuffers, device, "vkAllocateCommandBuffers")
            && vk.loadDevice(vk.resetCommandBuffer, device, "vkResetCommandBuffer")
            && vk.loadDevice(vk.beginCommandBuffer, device, "vkBeginCommandBuffer")
            && vk.loadDevice(vk.endCommandBuffer, device, "vkEndCommandBuffer")
            && vk.loadDevice(vk.cmdBeginRenderPass, device, "vkCmdBeginRenderPass")
            && vk.loadDevice(vk.cmdEndRenderPass, device, "vkCmdEndRenderPass")
            && vk.loadDevice(vk.createSemaphore, device, "vkCreateSemaphore")
            && vk.loadDevice(vk.destroySemaphore, device, "vkDestroySemaphore")
            && vk.loadDevice(vk.createFence, device, "vkCreateFence")
            && vk.loadDevice(vk.destroyFence, device, "vkDestroyFence")
            && vk.loadDevice(vk.waitForFences, device, "vkWaitForFences")
            && vk.loadDevice(vk.resetFences, device, "vkResetFences")
            && vk.loadDevice(vk.acquireNextImage, device, "vkAcquireNextImageKHR")
            && vk.loadDevice(vk.queueSubmit, device, "vkQueueSubmit")
            && vk.loadDevice(vk.queuePresent, device, "vkQueuePresentKHR")
            && vk.loadDevice(vk.deviceWaitIdle, device, "vkDeviceWaitIdle");

        if (!loaded) {
            return fail("Vulkan loader is missing required device functions.");
        }
        return true;
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
        const std::vector<VkPresentModeKHR>& presentModes
    ) const {
        const auto mailbox = std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR);
        return mailbox != presentModes.end() ? *mailbox : VK_PRESENT_MODE_FIFO_KHR;
    }

    [[nodiscard]] std::optional<VkExtent2D> chooseExtent(
        const VkSurfaceCapabilitiesKHR& capabilities
    ) {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        std::optional<VkExtent2D> drawableExtent = windowExtent();
        if (!drawableExtent.has_value()) {
            return std::nullopt;
        }
        drawableExtent->width = std::clamp(
            drawableExtent->width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width
        );
        drawableExtent->height = std::clamp(
            drawableExtent->height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height
        );
        return drawableExtent;
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

    [[nodiscard]] bool createSwapchainResources() {
        SwapchainSupport support = querySwapchainSupport(physicalDevice);
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
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vk.createSwapchain(device, &createInfo, nullptr, &swapchain);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateSwapchainKHR failed", result));
        }

        swapchainFormat = surfaceFormat.format;
        swapchainExtent = *extent;

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

        if (!createImageViews() || !createRenderPass() || !createFramebuffers()) {
            return false;
        }

        imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);
        state = RendererState::Ready;
        return true;
    }

    [[nodiscard]] bool createImageViews() {
        swapchainImageViews.reserve(swapchainImages.size());
        for (VkImage image : swapchainImages) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = image;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchainFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0U;
            createInfo.subresourceRange.levelCount = 1U;
            createInfo.subresourceRange.baseArrayLayer = 0U;
            createInfo.subresourceRange.layerCount = 1U;

            VkImageView view = VK_NULL_HANDLE;
            const VkResult result = vk.createImageView(device, &createInfo, nullptr, &view);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateImageView failed", result));
            }
            swapchainImageViews.push_back(view);
        }
        return true;
    }

    [[nodiscard]] bool createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentReference{};
        colorAttachmentReference.attachment = 0U;
        colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1U;
        subpass.pColorAttachments = &colorAttachmentReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0U;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1U;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1U;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1U;
        createInfo.pDependencies = &dependency;

        const VkResult result = vk.createRenderPass(device, &createInfo, nullptr, &renderPass);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkCreateRenderPass failed", result));
        }
        return true;
    }

    [[nodiscard]] bool createFramebuffers() {
        framebuffers.reserve(swapchainImageViews.size());
        for (VkImageView imageView : swapchainImageViews) {
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass;
            createInfo.attachmentCount = 1U;
            createInfo.pAttachments = &imageView;
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
        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = graphicsQueueFamily;

        VkResult result = vk.createCommandPool(device, &poolCreateInfo, nullptr, &commandPool);
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
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkAllocateCommandBuffers failed", result));
        }
        return true;
    }

    [[nodiscard]] bool createSynchronization() {
        frameSync.resize(config.framesInFlight);

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (FrameSync& frame : frameSync) {
            VkResult result = vk.createSemaphore(
                device,
                &semaphoreCreateInfo,
                nullptr,
                &frame.imageAvailable
            );
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateSemaphore failed", result));
            }

            result = vk.createSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.renderFinished);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateSemaphore failed", result));
            }

            result = vk.createFence(device, &fenceCreateInfo, nullptr, &frame.inFlight);
            if (result != VK_SUCCESS) {
                return fail(makeVulkanError("vkCreateFence failed", result));
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

        VkClearValue clearValue{};
        clearValue.color.float32[0] = 0.008F;
        clearValue.color.float32[1] = 0.006F;
        clearValue.color.float32[2] = 0.009F;
        clearValue.color.float32[3] = 1.0F;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;
        renderPassInfo.clearValueCount = 1U;
        renderPassInfo.pClearValues = &clearValue;

        vk.cmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vk.cmdEndRenderPass(commandBuffer);

        result = vk.endCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkEndCommandBuffer failed", result));
        }
        return true;
    }

    [[nodiscard]] bool recreateSwapchain() {
        std::optional<VkExtent2D> drawableExtent = windowExtent();
        if (!drawableExtent.has_value()) {
            return false;
        }
        if (drawableExtent->width == 0U || drawableExtent->height == 0U) {
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
        state = RendererState::Ready;
        return true;
    }

    [[nodiscard]] bool drawFrame() {
        if (state == RendererState::Headless) {
            return true;
        }
        if (state == RendererState::Suspended) {
            return recreateSwapchain();
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
        if (result != VK_SUCCESS) {
            return fail(makeVulkanError("vkResetCommandBuffer failed", result));
        }
        if (!recordCommandBuffer(commandBuffer, imageIndex)) {
            return false;
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
            || result == VK_SUBOPTIMAL_KHR
            || acquiredSuboptimal
            || framebufferResized;
        if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR
            && result != VK_SUBOPTIMAL_KHR) {
            return fail(makeVulkanError("vkQueuePresentKHR failed", result));
        }

        ++stats.submittedFrames;
        currentFrame = (currentFrame + 1U) % config.framesInFlight;

        if (requiresRebuild) {
            return recreateSwapchain();
        }
        return true;
    }

    void destroySynchronization() noexcept {
        if (device == VK_NULL_HANDLE) {
            frameSync.clear();
            return;
        }
        for (FrameSync& frame : frameSync) {
            if (frame.inFlight != VK_NULL_HANDLE && vk.destroyFence != nullptr) {
                vk.destroyFence(device, frame.inFlight, nullptr);
            }
            if (frame.renderFinished != VK_NULL_HANDLE && vk.destroySemaphore != nullptr) {
                vk.destroySemaphore(device, frame.renderFinished, nullptr);
            }
            if (frame.imageAvailable != VK_NULL_HANDLE && vk.destroySemaphore != nullptr) {
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

        if (vk.destroyFramebuffer != nullptr) {
            for (VkFramebuffer framebuffer : framebuffers) {
                if (framebuffer != VK_NULL_HANDLE) {
                    vk.destroyFramebuffer(device, framebuffer, nullptr);
                }
            }
        }
        framebuffers.clear();

        if (renderPass != VK_NULL_HANDLE && vk.destroyRenderPass != nullptr) {
            vk.destroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        if (vk.destroyImageView != nullptr) {
            for (VkImageView imageView : swapchainImageViews) {
                if (imageView != VK_NULL_HANDLE) {
                    vk.destroyImageView(device, imageView, nullptr);
                }
            }
        }
        swapchainImageViews.clear();
        swapchainImages.clear();
        imagesInFlight.clear();

        if (swapchain != VK_NULL_HANDLE && vk.destroySwapchain != nullptr) {
            vk.destroySwapchain(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        swapchainFormat = VK_FORMAT_UNDEFINED;
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

        if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE && vk.destroyCommandPool != nullptr) {
            vk.destroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        commandBuffers.clear();

        destroySwapchainResources();

        if (device != VK_NULL_HANDLE && vk.destroyDevice != nullptr) {
            vk.destroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        graphicsQueue = VK_NULL_HANDLE;
        presentQueue = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
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
