#pragma once

#include <string>

// Forward declarations
class ResourceManager;

/**
 * @brief Lightweight typed handle used to access resources through the ResourceManager.
 *
 * A ResourceHandler does not own the resource instance directly. Instead, it stores
 * the resource identifier together with a pointer to the ResourceManager responsible
 * for resolving that identifier into a live resource object.
 *
 * This allows engine systems and gameplay code to keep stable references to resources
 * without owning them explicitly. The handle provides typed access helpers and
 * convenience operators for retrieving the resolved resource when needed.
 *
 * @tparam T Concrete resource type referenced by this handle.
 */
template <typename T>
class ResourceHandler
{

public:

	/**
	 * @brief Default constructor.
	 */
	ResourceHandler() = default;

	/**
	 * @brief Constructor with a resource ID and resource manager.
	 * 
	 * @param id The resource ID.
	 * @param manager The resource manager.
	 */
	ResourceHandler(const std::string& id, ResourceManager* manager) : m_resourceId(id), m_resourceManager(manager) {}


	// ----------------------------------------------
	// RESOURCE ACCESS
	// ----------------------------------------------

	/**
	 * @brief Get the resource.
	 * 
	 * @return A pointer to the resource, or nullptr if not found.
	 */
	T* Get() const 
	{
		if (!m_resourceManager)
			return nullptr;

		return m_resourceManager->template GetResource<T>(m_resourceId);
	}

	/**
	 * @brief Check if the handle is valid.
	 * 
	 * @return True if the handle is valid, false otherwise.
	 */
	bool IsValid() const
	{
		if (!m_resourceManager)
			return false;

		return m_resourceManager->template HasResource<T>(m_resourceId);
	}


	// ----------------------------------------------
	// GETTERS & SETTERS & OPERATORS
	// ----------------------------------------------

	/**
	 * @brief Get the resource ID.
	 * 
	 * @return The resource ID.
	 */
	const std::string& GetId() const { return m_resourceId; }

	/**
	 * @brief Convenience operator for accessing the resource.
	 * 
	 * @return A pointer to the resource.
	 */
	T* operator->() const { return Get(); }

	/**
	 * @brief Convenience operator for dereferencing the resource.
	 * 
	 * @return A reference to the resource.
	 */
	T& operator*() const { return *Get(); }

	/**
	 * @brief Convenience operator for checking if the handle is valid.
	 * 
	 * @return True if the handle is valid, false otherwise.
	 */
	operator bool() const { return IsValid(); }

private:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	std::string         m_resourceId;					// Unique identifier of the resource referenced by this handle.
	ResourceManager*	m_resourceManager{ nullptr };	// Resource manager used to resolve the resource ID into a live resource instance.Not owned by this handle.
};

