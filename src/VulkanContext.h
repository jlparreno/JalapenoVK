#pragma once

#include "VulkanIncludes.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

/**
 * @brief Central Vulkan context responsible for owning all global GPU state.
 *
 * VulkanContext encapsulates the entire lifetime of the Vulkan backend that is
 * NOT tied to individual resources. It owns the instance, physical device,
 * logical device, queues, swapchain surface, command infrastructure, and debug
 * utilities.
 *
 * Resource-level objects (textures, buffers, meshes, etc.) do not own Vulkan
 * global state directly; instead, they depend on this context to allocate GPU
 * memory, create images/buffers, and submit commands.
 *
 * The context must outlive all GPU resources that reference it.
 */
class VulkanContext
{

public:

    /**
     * @brief Creates and initializes the Vulkan context for a given window.
     *
     * This constructor sets up the full Vulkan pipeline bootstrap, including:
     * instance creation, surface setup, physical device selection, logical device
     * creation, queue retrieval, and command pool initialization.
     *
     * @param window GLFW window used for surface creation and presentation.
     */
    VulkanContext(GLFWwindow* window);

    /**
     * @brief Destroys the Vulkan context and all owned GPU state.
     */
    ~VulkanContext() = default;

    // Non-copyable, non-movable (raii handles own the resources)
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    // ----------------------------------------------
    // ACCESSORS (READ-ONLY REFERENCES)
    // ----------------------------------------------

   /**
     * @brief Returns the logical Vulkan device.
     *
     * Used by resource systems to create and destroy GPU objects.
     */
    const vk::raii::Device&         GetDevice() const { return m_device; }

    /**
     * @brief Returns the selected physical GPU.
     *
     * Used for querying memory types and device capabilities.
     */
    const vk::raii::PhysicalDevice& GetPhysicalDevice() const { return m_physicalDevice; }

    /**
     * @brief Returns the surface from the device to draw to.
     */
    const vk::raii::SurfaceKHR&     GetSurface() const { return m_surface; }

    /**
     * @brief Returns the graphics queue used for command submission.
     */
    const vk::raii::Queue&          GetGraphicsQueue() const { return m_graphicsQueue; }

    /**
     * @brief Returns the command pool used for transient command buffers.
     */
    const vk::raii::CommandPool&    GetCommandPool()  const { return m_commandPool; }

    /**
     * @brief Returns the queue family index used for graphics/present operations.
     */
    const uint32_t&                 GetQueueFamilyIndex() const { return m_queueIndex; }


    // ----------------------------------------------
    // BUFFER HELPERS
    // ----------------------------------------------

    /**
     * @brief Creates a GPU buffer and allocates its backing memory.
     *
     * The buffer is created with the specified usage flags and allocated from a
     * memory type matching the requested properties (e.g. host-visible, device-local).
     *
     * @param size        Size of the buffer in bytes.
     * @param usage       Vulkan buffer usage flags.
     * @param properties  Memory property flags for allocation.
     *
     * @return Pair containing the created buffer and its allocated memory.
     */
    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

    /**
     * @brief Copies data between two GPU buffers using a staging command.
     *
     * This operation is executed on the graphics queue using a transient command
     * buffer and is typically used for CPU -> GPU transfers.
     *
     * @param srcBuffer Source buffer.
     * @param dstBuffer Destination buffer.
     * @param size      Number of bytes to copy.
     */
    void CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);


    // ----------------------------------------------
    // IMAGE HELPERS
    // ----------------------------------------------
    
    /**
     * @brief Creates a GPU image and allocates its backing memory.
     *
     * Used for textures, render targets, and other image-based GPU resources.
     *
     * @return Pair containing the created image and its allocated memory.
     */
    std::pair<vk::raii::Image, vk::raii::DeviceMemory>  CreateImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        vk::SampleCountFlagBits numSamples,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties);

    /**
     * @brief Transitions an image between Vulkan layouts.
     *
     * Inserts the required pipeline barrier to move an image between layouts such
     * as undefined -> transfer destination -> shader read.
     *
     * @param image       Image to transition.
     * @param oldLayout   Current image layout.
     * @param newLayout   Target image layout.
     * @param mipLevels   Number of mip levels affected.
     */
    void TransitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);

    /**
     * @brief Copies a buffer into a 2D image.
     *
     * Typically used for uploading texture data from a staging buffer.
     */
    void CopyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);

    /**
     * @brief Creates an image view for a GPU image.
     *
     * Image views define how shaders access image data.
     */
    vk::raii::ImageView CreateImageView(vk::Image const& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);


    // ----------------------------------------------
    // MEMORY HELPERS
    // ----------------------------------------------

    /**
     * @brief Finds a compatible memory type index for GPU allocations.
     *
     * Vulkan exposes multiple memory heaps and types per device. This function
     * selects a memory type that satisfies both the requested type filter and
     * required property flags (e.g. host-visible, device-local).
     *
     * @param typeFilter Bitmask of allowed memory types.
     * @param properties Desired memory property flags.
     *
     * @return Index of a compatible memory type.
     */
    uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;


    vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);


    // ----------------------------------------------
    // COMMANDS HELPERS
    // ----------------------------------------------

    /**
     * @brief Begins recording a one-time command buffer for short-lived GPU operations.
     *
     * Allocates a transient command buffer from the command pool, begins recording,
     * and returns it for immediate use. Typically used for staging operations.
     *
     * @return Pointer to an active command buffer ready for recording.
     */
    std::unique_ptr<vk::raii::CommandBuffer> BeginSingleTimeCommands();

    /**
     * @brief Ends recording and submits a one-time command buffer to the GPU.
     *
     * Submits the command buffer to the graphics queue, waits for execution to
     * complete, and frees the temporary buffer resources.
     *
     * @param commandBuffer Command buffer previously started with BeginSingleTimeCommands().
     */
    void EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer) const;


private:

    // ----------------------------------------------
    // INITIALIZATION PIPELINE
    // ----------------------------------------------
    
    /**
     * @brief Creates the Vulkan instance and establishes the connection with the Vulkan loader.
     *
     * This step initializes the global Vulkan context, specifying application info,
     * required instance extensions, and enabled validation layers. It is the root
     * object required before any GPU interaction can occur.
     */
    void CreateInstance();

    /**
     * @brief Configures and installs the Vulkan debug messenger (debug builds only).
     *
     * Sets up validation layer callbacks and defines severity/type filters for
     * runtime debugging. This allows Vulkan to report warnings, errors, and performance
     * issues during execution.
     */
    void SetupDebugMessenger();

    /**
     * @brief Creates the window surface used for presenting rendered images.
     *
     * Establishes the platform-specific swapchain surface (GLFW integration) which
     * acts as the link between Vulkan and the operating system windowing system.
     */
    void CreateSurface();

    /**
     * @brief Selects a physical GPU that satisfies engine requirements.
     *
     * Iterates over available physical devices and evaluates their capabilities,
     * checking for required queue families, extensions, and feature support.
     * The first suitable device is selected and stored.
     */
    void PickPhysicalDevice();

    /**
     * @brief Creates the logical device and retrieves required queues.
     *
     * Opens a logical connection to the selected physical device, enabling the
     * required features and extensions, and retrieves the graphics/present queue
     * used for command submission.
     */
    void CreateLogicalDevice();

    /**
     * @brief Creates the command pool used for allocating transient command buffers.
     *
     * The command pool is associated with the graphics queue family and is used
     * for short-lived command buffers such as staging operations and resource uploads.
     */
    void CreateCommandPool();


    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Returns the list of required Vulkan instance extensions.
     *
     * Includes platform-specific extensions (GLFW surface support) and optional
     * debugging extensions when validation layers are enabled.
     *
     * @return Array of required instance extension names.
     */
    std::vector<const char*> GetRequiredInstanceExtensions();

    /**
     * @brief Evaluates whether a physical device is suitable for engine usage.
     *
     * Checks support for required queue families, Vulkan extensions, swapchain
     * compatibility, and minimum feature requirements.
     *
     * @param physicalDevice GPU candidate to evaluate.
     *
     * @return True if the device satisfies all requirements, false otherwise.
     */
    bool IsDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice);


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    GLFWwindow*                m_window{ nullptr };     // Native GLFW window used for surface creation and presentation.

    vk::raii::Context          m_context;               // Loads the Vulkan loader
    vk::raii::Instance         m_instance;              // Vulkan instance
    vk::raii::SurfaceKHR       m_surface;               // Window surface (WSI)
    vk::raii::PhysicalDevice   m_physicalDevice;        // GPU selected at startup
    vk::raii::Device           m_device;                // Logical device
    vk::raii::Queue            m_graphicsQueue;         // Graphics + present queue
    vk::raii::CommandPool      m_commandPool;           // For transient commands

    uint32_t                   m_queueIndex = ~0;	    // Index of the queue family that supports graphics + present (~0 = uninitialized sentinel)

    vk::raii::DebugUtilsMessengerEXT m_debugMessenger;  // Validation layer callback; active only in debug builds

    
    // ----------------------------------------------
    // CONSTANTS
    // ----------------------------------------------

    // Vulkan validation layers enabled in debug builds to catch API misuse, incorrect synchronization, and undefined behavior at runtime.
    static constexpr std::array<const char*, 1> k_validationLayers{ "VK_LAYER_KHRONOS_validation" };

    // Required device extensions for engine functionality. Includes swapchain support for presentation and additional shader capabilities.
    static constexpr std::array<const char*, 2> k_deviceExtensions{ vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName };


    // ----------------------------------------------
    // DEBUG CALLBACK
    // ----------------------------------------------

    /**
     * @brief Vulkan validation layer callback used for debug output.
     *
     * This function is registered with the Vulkan debug utils extension and is
     * invoked whenever the validation layers emit messages (warnings, errors,
     * performance hints, or informational logs).
     *
     * It forwards the message to standard error output for developer visibility.
     *
     * @param severity      Severity level of the validation message.
     * @param type          Type/category of the validation message.
     * @param pCallbackData Detailed callback payload containing the message text.
     * @param pUserData     User-defined pointer passed during registration (unused).
     *
     * @return vk::False to indicate that Vulkan should not abort the call.
     */
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, 
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }
};