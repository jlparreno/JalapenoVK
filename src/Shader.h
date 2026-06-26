#pragma once

#include "VulkanIncludes.h"
#include "Resource.h"

#include <ktx.h>

// Forward declarations
class VulkanContext;

/**
 * @brief GPU shader resource loaded from a SPIR-V file.
 *
 * Manages the lifecycle of a vk::ShaderModule: reading a compiled SPIR-V
 * binary from disk and wrapping it in a Vulkan shader module ready to be
 * used in a pipeline stage.
 *
 * Inherits from Resource, so Load() / Unload() follow the standard resource
 * lifecycle. Requires a valid VulkanContext for all GPU operations.
 */
class Shader : public Resource
{

public:

    /**
     * @brief Constructs a Shader bound to a Vulkan context and a pipeline stage.
     *
     * @param context       The Vulkan context used for all GPU resource operations. Must outlive this shader.
     * @param id            Unique resource identifier, used to resolve the SPIR-V file path.
     * @param shaderStage   The pipeline stage this shader will be bound to (eVertex, eFragment, eCompute).
     */
    explicit Shader(VulkanContext& context, const std::string& id, vk::ShaderStageFlagBits shaderStage) : Resource(id), m_context(context), m_stage(shaderStage) {}

    /**
     * @brief Destructor. Ensures GPU resources are released via Unload().
     */
    ~Shader() { Unload(); }


    // ----------------------------------------------
    // RESOURCE OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Loads the SPIR-V binary from disk and creates the shader module.
     *
     * Reads the compiled SPIR-V file resolved from the resource ID and
     * wraps it in a vk::ShaderModule for use in pipeline creation.
     *
     * @return True if the shader module was created successfully, false otherwise.
     */
    bool Load() override;

    /**
     * @brief Releases the shader module.
     *
     * Safe to call if the shader was never loaded.
     */
    void Unload() override;


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the Vulkan shader module handle, valid only while the shader is loaded.
     */
    vk::ShaderModule GetShaderModule() const { return m_shaderModule; }

    /**
     * @brief Returns the pipeline stage this shader is bound to (e.g. eVertex, eFragment).
     */
    vk::ShaderStageFlagBits GetStage() const { return m_stage; }

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Reads a binary file into a byte buffer.
     *
     * Opens the file in binary mode and reads its full contents into @p buffer.
     *
     * @param filePath  Path to the SPIR-V binary file.
     * @param buffer    Output buffer populated with the raw file bytes.
     * 
     * @return True if the file was read successfully, false otherwise.
     */
    bool ReadFile(const std::string& filePath, std::vector<char>& buffer);

    /**
     * @brief Creates a vk::ShaderModule from a SPIR-V byte buffer.
     *
     * Wraps the raw SPIR-V binary in a Vulkan shader module object,
     * which is required by the pipeline stage create info at pipeline creation time.
     *
     * @param code  Raw SPIR-V binary data as read from disk.
     */
    void CreateShaderModule(const std::vector<char>& code);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Vulkan context used for all GPU operations. Not owned by this class.
    VulkanContext&            m_context;

    vk::ShaderStageFlagBits   m_stage;                    // Pipeline stage this shader is bound to.
    vk::raii::ShaderModule    m_shaderModule{ nullptr };  // Compiled SPIR-V wrapped in a Vulkan shader module.
};