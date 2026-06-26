#include "Shader.h"
#include "VulkanContext.h"

#include <fstream>

bool Shader::Load()
{
    // Load shader from file
    std::string filePath = "shaders/" + GetId() + ".spv";

    // Read shader code
    std::vector<char> shaderCode;
    if (!ReadFile(filePath, shaderCode)) 
    {
        return false;
    }

    // Create shader module
    CreateShaderModule(shaderCode);

    return Resource::Load();
}

void Shader::Unload()
{
    // Destroy Vulkan resources
    if (IsLoaded()) 
    {
        m_shaderModule = nullptr;

        Resource::Unload();
    }
}

bool Shader::ReadFile(const std::string& filePath, std::vector<char>& buffer)
{
    // Opening at "ate" positions the cursor at the end of the file
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::cout << "Failed to open shader file!" << std::endl;
        return false;
    }

    // The cursor is at the end, so its position is the file size
    buffer.resize(file.tellg());

    file.seekg(0, std::ios::beg);                                           // Return to beginning
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));  // Read all file into the vector

    file.close();

    return true;
}

void Shader::CreateShaderModule(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo
    {
        .codeSize   = code.size() * sizeof(char),
        .pCode      = reinterpret_cast<const uint32_t*>(code.data())
    };

    m_shaderModule = vk::raii::ShaderModule(m_context.GetDevice(), createInfo);
}
