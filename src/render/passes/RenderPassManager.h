#pragma once

#include "core/VulkanIncludes.h"
#include "render/passes/RenderPass.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Owns a set of RenderPass instances and executes them in dependency order.
 *
 * RenderPassManager stores every registered pass by name, tracks the declared
 * dependencies between them, and produces an ordered execution list via
 * topological sort. The order is rebuilt lazily whenever the set of passes or
 * their dependencies change.
 *
 * The manager owns each pass by unique_ptr, so any pointer returned by
 * AddRenderPass() / GetRenderPass() is only valid while the pass remains
 * registered and while the manager itself is alive.
 */
class RenderPassManager
{
public:

    // ----------------------------------------------
    // RENDER PASSES MANAGEMENT
    // ----------------------------------------------

    /**
     * @brief Registers a new render pass of type T under the given name.
     *
     * If a pass with the same name is already registered, returns the existing
     * instance cast to T without creating a new one. Otherwise constructs T
     * in-place forwarding the provided arguments and marks the execution order
     * dirty so the next Execute() rebuilds it.
     *
     * @tparam T        Concrete pass type. Must derive from RenderPass.
     * @tparam Args     Argument types forwarded to T's constructor.
     * @param name      Unique identifier under which the pass is registered.
     * @param args      Arguments forwarded to T's constructor.
     *
     * @return Pointer to the registered pass. Non-owning; remains valid until the pass is removed or the manager is destroyed.
     */
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

    /**
     * @brief Removes the pass registered under the given name.
     *
     * Safe to call with a name that is not registered (no-op in that case).
     * Marks the execution order dirty so the next Execute() rebuilds it.
     *
     * @param name  Identifier of the pass to remove.
     */
    void RemoveRenderPass(const std::string& name);

    /**
     * @brief Executes every enabled pass in dependency order for the current frame.
     *
     * Recomputes the sorted execution list if any pass has been added or removed
     * since the last call, then dispatches each pass's Execute() with the shared
     * command buffer and per-frame info.
     *
     * @param commandBuffer  Command buffer being recorded for this frame.
     * @param frame          Per-frame indexing data forwarded to every pass.
     */
    void Execute(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame);

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the pass registered under the given name, or nullptr if none exists.
     *
     * @param name  Identifier of the pass to look up.
     *
     * @return Non-owning pointer to the pass, or nullptr if unregistered.
     */
    RenderPass* GetRenderPass(const std::string& name);

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Rebuilds the ordered execution list from the current pass set and their dependencies.
     *
     * Runs a topological sort over the dependency graph and repopulates m_sortedPasses.
     * Called automatically at the start of Execute() when m_dirty is true.
     */
    void SortPasses();

    /**
     * @brief Recursive depth-first search (DFS) helper used to build the topologically sorted pass order.
     *
     * Detects cycles via the visiting set and appends nodes to m_sortedPasses
     * in post-order once all their dependencies have been resolved.
     *
     * @param name      Name of the pass being visited in this call.
     * @param passMap   Flat name -> RenderPass* lookup used to walk dependencies.
     * @param visited   Set of pass names already finalised in the order.
     * @param visiting  Set of pass names currently on the recursion stack (cycle detection).
     */
    void TopologicalSort(const std::string& name,
        const std::unordered_map<std::string, RenderPass*>& passMap,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& visiting);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    std::unordered_map<std::string, std::unique_ptr<RenderPass>>    m_renderPasses;     // Owned pass instances keyed by name.
    std::vector<RenderPass*>                                        m_sortedPasses;     // Cached execution order, rebuilt from m_renderPasses when m_dirty is true.
    bool                                                            m_dirty{ true };    // Set when the pass set changes; triggers a re-sort on the next Execute().
};
