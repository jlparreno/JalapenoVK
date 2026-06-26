#pragma once

#include <unordered_map>
#include <stdexcept>
#include <typeindex>
#include <memory>

#include "ResourceHandler.h"
#include "Resource.h"

// Forward declarations
class VulkanContext;

/**
 * @brief Central registry responsible for loading, storing, and unloading engine resources.
 *
 * ResourceManager owns every loaded resource instance and provides typed access
 * to them through a two-level lookup table indexed first by concrete resource
 * type and then by resource identifier.
 *
 * Resources are created lazily on first request via LoadResource<T>(), cached
 * internally, and returned to callers through lightweight ResourceHandler<T>
 * objects or direct typed pointers. The manager is also responsible for invoking
 * the resource lifecycle methods and destroying resource instances when they are
 * explicitly unloaded or when the manager itself is torn down.
 */
class ResourceManager final
{

public:

	/**
	 * @brief Default constructor.
	 */
	ResourceManager() = default;

	/**
	 * @brief Virtual destructor for proper cleanup.
	 */
	virtual ~ResourceManager() = default;


	// ----------------------------------------------
	// RESOURCE LOADING & ACCESS
	// ----------------------------------------------

	/**
	 * @brief Load a resource.
	 * 
	 * @tparam T The type of resource.
	 * @tparam Args The types of arguments to pass to the resource constructor.
	 * @param id The resource ID.
	 * @param args The arguments to pass to the resource constructor.
	 * 
	 * @return A handle to the resource.
	 */
	template <typename T, typename... Args>
	ResourceHandler<T> LoadResource(VulkanContext& context, const std::string& id, Args &&...args)
	{
		static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

		// Check if the resource already exists
		auto& typeResources = m_resources[std::type_index(typeid(T))];
		auto it = typeResources.find(id);
		if (it != typeResources.end())
		{
			return ResourceHandler<T>(id, this);
		}

		// Create and load the resource
		auto resource = std::make_unique<T>(context, id, std::forward<Args>(args)...);
		if (!resource->Load())
		{
			// Loading failed - return invalid handle
			return ResourceHandler<T>();
		}

		// Store the resource
		typeResources[id] = std::move(resource);
		return ResourceHandler<T>(id, this);
	}

	/**
	 * @brief Get a resource.
	 * 
	 * @tparam T The type of resource.
	 * @param id The resource ID.
	 * 
	 * @return A pointer to the resource, or nullptr if not found.
	 */
	template <typename T>
	T* GetResource(const std::string& id)
	{
		static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

		auto typeIt = m_resources.find(std::type_index(typeid(T)));
		if (typeIt == m_resources.end())
		{
			// Type not found - return null for safe handling by caller
			return nullptr;
		}

		auto& typeResources = typeIt->second;
		auto  resourceIt = typeResources.find(id);
		if (resourceIt == typeResources.end())
		{
			// Resource not found - return null for safe handling by caller
			return nullptr;
		}

		// Resource found - perform safe downcast and return typed pointer
		return static_cast<T*>(resourceIt->second.get());
	}

	/**
	 * @brief Check if a resource exists.
	 * 
	 * @tparam T The type of resource.
	 * @param id The resource ID.
	 * 
	 * @return True if the resource exists, false otherwise.
	 */
	template <typename T>
	bool HasResource(const std::string& id)
	{
		static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

		auto typeIt = m_resources.find(std::type_index(typeid(T)));
		if (typeIt == m_resources.end())
		{
			// Type not found
			return false;
		}

		// Type found - check if resource exists
		auto& typeResources = typeIt->second;
		return typeResources.contains(id);
	}

	/**
	 * @brief Unload a resource.
	 * 
	 * @tparam T The type of resource.
	 * @param id The resource ID.
	 * 
	 * @return True if the resource was unloaded, false otherwise.
	 */
	template <typename T>
	bool UnloadResource(const std::string& id)
	{
		static_assert(std::is_base_of<Resource, T>::value, "T must derive from Resource");

		auto typeIt = m_resources.find(std::type_index(typeid(T)));
		if (typeIt == m_resources.end())
		{
			return false;
		}

		auto& typeResources = typeIt->second;
		auto  resourceIt = typeResources.find(id);
		if (resourceIt == typeResources.end())
		{
			return false;
		}

		resourceIt->second->Unload();
		typeResources.erase(resourceIt);
		return true;
	}

	/**
	 * @brief Unload all resources.
	 */
	void UnloadAllResources();

private:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	// Two-level resource storage:
	//   1. First key  -> concrete resource type
	//   2. Second key -> unique resource identifier within that type
	//
	// This layout keeps resource ownership centralized while allowing efficient,
	// type-safe lookups without requiring separate containers per resource class.
	std::unordered_map<std::type_index, std::unordered_map<std::string, std::unique_ptr<Resource>>> m_resources;
};

