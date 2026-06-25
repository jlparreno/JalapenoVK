#pragma once

#include <string>

/**
 * @brief Base interface for loadable engine resources.
 *
 * Resource provides the common lifecycle contract shared by all resource types
 * managed by the engine, such as textures, meshes, materials, shaders, or
 * other asset-backed objects.
 *
 * Each resource is identified by a unique string ID and exposes a standard
 * Load() / Unload() workflow. Derived classes are expected to override these
 * methods to perform the actual acquisition and release of CPU- and/or GPU-side
 * data while keeping m_loaded synchronized with the real resource state.
 */
class Resource
{

public:

	/**
	 * @brief Constructor with a resource ID.
	 * 
	 * @param id The unique identifier for the resource.
	 */
	explicit Resource(const std::string& id) : m_id(id) {}

	/**
	 * @brief Virtual destructor for proper cleanup.
	 */
	virtual ~Resource() = default;


	// ----------------------------------------------
	// VIRTUAL METHODS FOR ALL RESOURCES
	// ----------------------------------------------

	/**
	 * @brief Load the resource.
	 * 
	 * @return True if the resource was loaded successfully, false otherwise.
	 */
	virtual bool Load();

	/**
	 * @brief Unload the resource.
	 */
	virtual void Unload();


	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Get the resource ID.
	 * 
	 * @return The resource ID.
	 */
	const std::string& GetId() const { return m_id; }

	/**
	 * @brief Check if the resource is loaded.
	 * 
	 * @return True if the resource is loaded, false otherwise.
	 */
	bool IsLoaded() const {	return m_loaded; }

protected:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	std::string		m_id;				// Unique resource identifier used by the resource system to resolve this asset.
	bool			m_loaded = false;	// Tracks whether the resource is currently in a valid loaded state.
};

