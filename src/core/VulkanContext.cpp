#include "core/VulkanContext.h"
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VulkanContext::VulkanContext(GLFWwindow* window) :
	m_window{ window },
	m_context{},
	m_instance{ nullptr },
	m_surface{ nullptr },
	m_physicalDevice{ nullptr },
	m_device{ nullptr },
	m_graphicsQueue{ nullptr },
	m_commandPool{ nullptr },
	m_debugMessenger{ nullptr }
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateCommandPool();
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> VulkanContext::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	// Buffer description
	vk::BufferCreateInfo bufferInfo
	{
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive		// Only one queue family will use this buffer
	};

	// Buffer creation. No memory is allocated here, it is only the resource
	vk::raii::Buffer buffer = vk::raii::Buffer(m_device, bufferInfo);

	// Ask GPU for memory requirements. We should know alignment, size and compatible types of memory.
	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo
	{
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties)
	};

	// Memory reserved here, on DeviceMemory creation
	vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(m_device, allocInfo);

	// Link Buffer object to actual memory
	buffer.bindMemory(*bufferMemory, 0);

	// Vulkan doesn't allow copies of buffers, so we need to return them moved
	return { std::move(buffer), std::move(bufferMemory) };
}

void VulkanContext::CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
{
	std::unique_ptr<vk::raii::CommandBuffer> commandCopyBuffer = BeginSingleTimeCommands();
	commandCopyBuffer->copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{ .size = size });
	EndSingleTimeCommands(*commandCopyBuffer);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> VulkanContext::CreateImage(const ImageDescription& imageDesc)
{
	// Image creation info
	vk::ImageCreateInfo imageInfo
	{
		.flags		 = imageDesc.flags,
		.imageType	 = imageDesc.imageType,
		.format		 = imageDesc.format,
		.extent		 = {imageDesc.width, imageDesc.height, imageDesc.depth},
		.mipLevels	 = imageDesc.mipLevels,
		.arrayLayers = imageDesc.arrayLayers,
		.samples	 = imageDesc.samples,
		.tiling		 = imageDesc.tiling,
		.usage		 = imageDesc.usage,
		.sharingMode = imageDesc.sharingMode
	};

	// Create the Vulkan Image object. No memory is allocated here, it is only the resource
	vk::raii::Image image = vk::raii::Image(m_device, imageInfo);

	// Ask GPU for memory requirements. We should know alignment, size and compatible types of memory.
	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo
	{
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, imageDesc.memory)
	};

	// Memory reserved here, on DeviceMemory creation
	vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(m_device, allocInfo);

	// Link Image object to actual memory
	image.bindMemory(imageMemory, 0);

	// Vulkan doesn't allow copies of buffers, so we need to return them moved
	return { std::move(image), std::move(imageMemory) };
}

void VulkanContext::TransitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
{
	const auto commandBuffer = BeginSingleTimeCommands();

	// !! --> A barrier tells Vulkan: this image was being used like this, now it will be used like that
	vk::ImageMemoryBarrier2 barrier;
	barrier.setOldLayout(oldLayout)
	       .setNewLayout(newLayout)
	       .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
	       .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
	       .setImage(image)
	       .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 });

	// Image just created, we want it ready to receive a copy
	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
		       .setSrcAccessMask(vk::AccessFlagBits2::eNone)
		       .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
		       .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);
	}
	// We already copied the pixels, now this image should be used in shaders. Wait writes to end.
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
		       .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
		       .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
		       .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	// Record the transition
	vk::DependencyInfo depInfo;
	depInfo.setImageMemoryBarriers(barrier);
	commandBuffer->pipelineBarrier2(depInfo);

	EndSingleTimeCommands(*commandBuffer);
}

void VulkanContext::CopyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
{
	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = BeginSingleTimeCommands();

	vk::BufferImageCopy region
	{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
		.imageOffset = {0, 0, 0},
		.imageExtent = {width, height, 1}
	};

	commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

	EndSingleTimeCommands(*commandBuffer);
}

vk::raii::ImageView VulkanContext::CreateImageView(const ImageViewDescription& viewDesc)
{
	vk::ImageViewCreateInfo viewInfo
	{
		.image		= viewDesc.image,
		.viewType	= viewDesc.viewType,
		.format		= viewDesc.format,
		.subresourceRange = 
		{
			.aspectMask		= viewDesc.aspect, 
			.baseMipLevel	= viewDesc.baseMipLevel, 
			.levelCount		= viewDesc.levelCount, 
			.baseArrayLayer = viewDesc.baseArrayLayer, 
			.layerCount		= viewDesc.layerCount
		}
	};

	return vk::raii::ImageView(m_device, viewInfo);
}

void VulkanContext::CreateInstance()
{
	// Description of the app
	constexpr vk::ApplicationInfo appInfo
	{
		.pApplicationName	= "JalapenoVK",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName		= "JalapenoVK",
		.engineVersion		= VK_MAKE_VERSION(1, 0, 0),
		.apiVersion			= vk::ApiVersion14
	};

	// EXTENSIONS
	// Get the required Vulkan instance extensions
	auto requiredExtensions = GetRequiredInstanceExtensions();

	// Check if the required extensions are supported by the Vulkan implementation
	// If any of required extensions is not found in the context, then it will be unsupported and we should return accordly
	auto extensionProperties = m_context.enumerateInstanceExtensionProperties();
	auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions,
		[&extensionProperties](auto const& requiredExtension) {
			return std::ranges::none_of(extensionProperties,
				[requiredExtension](auto const& extensionProperty) {
					return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
				});
		});

	if (unsupportedPropertyIt != requiredExtensions.end())
	{
		throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
	}

	// VALIDATION LAYERS
	// Get the required validation layers
	std::vector<const char*> requiredLayers;
#ifndef NDEBUG
	requiredLayers.assign(k_validationLayers.begin(), k_validationLayers.end());
#endif

	// Check if the required layers are supported by the Vulkan implementation.
	// If any of required validation layer is not found in the context, then it will be unsupported and we should return accordly
	auto layerProperties = m_context.enumerateInstanceLayerProperties();
	auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		[&layerProperties](auto const& requiredLayer) {
			return std::ranges::none_of(layerProperties,
				[requiredLayer](auto const& layerProperty) {
					return strcmp(layerProperty.layerName, requiredLayer) == 0;
				});
		});

	if (unsupportedLayerIt != requiredLayers.end())
	{
		throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
	}

	// Build InstanceCreateInfo with app, extensions and validation layers data
	vk::InstanceCreateInfo createInfo
	{
		.pApplicationInfo		 = &appInfo,
		.enabledLayerCount		 = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames	 = requiredLayers.data(),
		.enabledExtensionCount	 = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data()
	};

	// Create the instance
	m_instance = vk::raii::Instance(m_context, createInfo);
}

void VulkanContext::SetupDebugMessenger()
{
	// Enable messenger only in DEBUG mode
#ifdef NDEBUG
	return;
#endif

	// Only show Warning and Errors
	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

	// Show messages of type:Generic, Performance: things that work but are slow, Validation: API usage errors
	vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

	// Configure callback to call when we have any of the configured messages
	// debugCallback is a simple function to print the specific message in terminal
	vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT
	{
		.messageSeverity = severityFlags,
		.messageType	 = messageTypeFlags,
		.pfnUserCallback = &DebugCallback
	};

	// Register debug system in the instance
	m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void VulkanContext::CreateSurface()
{
	// Temporal C-style Vulkan (GLFW needs this)
	VkSurfaceKHR _surface;

	// Create the surface where Vulkan can present images
	if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &_surface) != 0)
	{
		throw std::runtime_error("failed to create window surface!");
	}

	// Convert to RAII
	m_surface = vk::raii::SurfaceKHR(m_instance, _surface);
}

void VulkanContext::PickPhysicalDevice()
{
	// Ask Vulkan for available GPUs in the system
	// Each element represents a physical GPU (AMD, NVIDIA, Intel...)
	std::vector<vk::raii::PhysicalDevice> physicalDevices = m_instance.enumeratePhysicalDevices();

	// Search for the first GPU that suits our requirements (checked in isDeviceSuitable)
	// Here we choose the first one for simplicity, but we could assign a score for each GPU and choose the best, for example
	auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const& physicalDevice) { return IsDeviceSuitable(physicalDevice); });

	// If there is no valid GPU, we cannot continue
	if (devIter == physicalDevices.end())
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	// Save the selected GPU
	m_physicalDevice = *devIter;

	// Print selected GPU model
	auto properties = m_physicalDevice.getProperties();
	std::cout << "[VulkanContext] Selected GPU: " << properties.deviceName << '\n';

	//Get max usable samples for this device (for MSAA)
	//m_msaaSamples = getMaxUsableSampleCount();
}

void VulkanContext::CreateLogicalDevice()
{
	// Get physical device queues properties. Which type of queues do you have and how many?
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();

	// Get the first queue from queueFamilyProperties which supports both graphics and present (can draw and present in window)
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
	{
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface))
		{
			m_queueIndex = qfpIndex;
			break;
		}
	}
	if (m_queueIndex == ~0)
	{
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// Query for features. We are telling which features we want to use (same than the previous checked that they are available from physical device)
	vk::StructureChain<vk::PhysicalDeviceFeatures2,
					   vk::PhysicalDeviceVulkan11Features,
					   vk::PhysicalDeviceVulkan13Features,
					   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain =
	{
		{.features = {.sampleRateShading = true, .samplerAnisotropy = true}},   // vk::PhysicalDeviceFeatures2
		{.shaderDrawParameters = true},											// advanced instancing, improved indirect drawing
		{.synchronization2 = true, .dynamicRendering = true},					// Removes RenderPass requirement, improvements in synchronization  
		{.extendedDynamicState = true}											// Enable extended dynamic state from the extension
	};

	// Create a Device. We should assign a priority for the queue. 
	// For now we only have one so 0.5 is ok. We can assign any from 0.0 to 1.0
	float queuePriority = 0.5f;

	// Inside this GPU I want: 1 graphic queue of the selected family with medium priority
	// The GPU could have more queues, we are only requesting one
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo
	{
		.queueFamilyIndex	= m_queueIndex,
		.queueCount			= 1,
		.pQueuePriorities	= &queuePriority
	};

	// Here we define all that we need in the logical device: features (beginning of the chain), queue and required extensions
	vk::DeviceCreateInfo deviceCreateInfo
	{
		.pNext					 = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount	 = 1,
		.pQueueCreateInfos		 = &deviceQueueCreateInfo,
		.enabledExtensionCount	 = static_cast<uint32_t>(k_deviceExtensions.size()),
		.ppEnabledExtensionNames = k_deviceExtensions.data()
	};

	// Device creation and access to the queue
	m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
	m_graphicsQueue = vk::raii::Queue(m_device, m_queueIndex, 0);
}

void VulkanContext::CreateCommandPool()
{
	// eResetCommandBuffer allows individual command buffers to be re-recorded without resetting the entire pool.
	// Suitable for per-frame recording.
	vk::CommandPoolCreateInfo poolInfo
	{
		.flags				= vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex	= m_queueIndex // Commands will be submitted to this queue family (graphics + present)
	};

	m_commandPool = vk::raii::CommandPool(m_device, poolInfo);
}

std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions()
{
	// Ask GLFW for required Vulkan extensions
	uint32_t glfwExtensionCount = 0;
	auto     glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Initialize required extensions list
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	// If DEBUG mode, then add the the extension for validation layers to the list
#ifndef NDEBUG
	extensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

	return extensions;
}

bool VulkanContext::IsDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice)
{
	// Get physical device properties
	auto deviceProperties = physicalDevice.getProperties();
	auto queueFamilies = physicalDevice.getQueueFamilyProperties();

	// Check if the physicalDevice supports the Vulkan 1.3 API version
	bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;

	// Check if any of the queue families support graphics operations (capable to perform draw calls)
	bool supportsGraphics = std::ranges::any_of(queueFamilies,
		[](auto const& qfp) {
			return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
		});

	// Check if all required physicalDevice extensions are available
	auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties(); // Which extensions does this GPU support?
	bool supportsAllRequiredExtensions = std::ranges::all_of(k_deviceExtensions,
		[&availableDeviceExtensions](auto const& requiredDeviceExtension) {
			return std::ranges::any_of(availableDeviceExtensions,
				[requiredDeviceExtension](auto const& availableDeviceExtension) {
					return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
				});
		});


	// Check if the physicalDevice supports the required features (shader draw parameters, dynamic rendering and extended dynamic state)
	auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
														 vk::PhysicalDeviceVulkan11Features,
														 vk::PhysicalDeviceVulkan13Features,
														 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

	bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.sampleRateShading &&      // sample rate shading
		features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&									// anisotropic filtering for textures
		features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&									// advanced instancing, improved indirect drawing
		features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&										// Removes RenderPass requirement
		features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&										// Improvement in synchronization
		features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;					// Allows to change state without recreating pipeline (viewport, scissor)

	// Return true if the physicalDevice meets all the criteria
	return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

uint32_t VulkanContext::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
	vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		// typeFilter is a bitmask where bit i means memory type i is acceptable to the driver.
		// We additionally check that the type includes all the property flags we need.
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format VulkanContext::FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
	for (const auto format : candidates)
	{
		vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);

		if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
			((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

std::unique_ptr<vk::raii::CommandBuffer> VulkanContext::BeginSingleTimeCommands()
{
	// Create temporal command buffer
	vk::CommandBufferAllocateInfo allocInfo
	{
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(m_device, allocInfo).front()));

	// Start command recording, telling that you are going to record only one time
	vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
	commandBuffer->begin(beginInfo);

	return commandBuffer;
}

void VulkanContext::EndSingleTimeCommands(const vk::raii::CommandBuffer& commandBuffer) const
{
	// End command recording
	commandBuffer.end();

	// Submit command buffer to the queue, here we are telling the GPU to execute this copy command
	vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *commandBuffer };

	vk::SubmitInfo2 submitInfo;
	submitInfo.setCommandBufferInfos(commandBufferInfo);

	m_graphicsQueue.submit2(submitInfo, nullptr);

	// We wait until this queue is empty, so GPU has finished
	m_graphicsQueue.waitIdle();
}
