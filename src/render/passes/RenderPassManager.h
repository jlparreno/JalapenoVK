#pragma once

#include "core/VulkanIncludes.h"
#include "render/passes/RenderPass.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class VulkanContext;

class RenderPassManager
{
public:

   // ----------------------------------------------
   // RENDER PASSES MANAGEMENT
   // ----------------------------------------------

    template<typename T, typename... Args>
    T* AddRenderPass(const std::string& name, Args&&... args)
    {
        static_assert(std::is_base_of<RenderPass, T>::value, "T must derive from RenderPass");

        auto it = m_renderPasses.find(name);
        if (it != m_renderPasses.end()) {
            return dynamic_cast<T*>(it->second.get());
        }

        auto pass = std::make_unique<T>(name, std::forward<Args>(args)...);
        T* passPtr = pass.get();
        m_renderPasses[name] = std::move(pass);
        m_dirty = true;

        return passPtr;
    }

    void RemoveRenderPass(const std::string& name);

    void Execute(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame);

   // ----------------------------------------------
   // GETTERS & SETTERS
   // ----------------------------------------------

    RenderPass* GetRenderPass(const std::string& name);

private:

   // ----------------------------------------------
   // INTERNAL HELPERS
   // ----------------------------------------------

    void SortPasses();

    void TopologicalSort(const std::string& name,
        const std::unordered_map<std::string, RenderPass*>& passMap,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& visiting);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    std::unordered_map<std::string, std::unique_ptr<RenderPass>>    m_renderPasses;
    std::vector<RenderPass*>                                        m_sortedPasses;
    bool                                                            m_dirty{ true };
};
