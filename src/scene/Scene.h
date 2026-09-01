#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class Entity;

/**
 * @brief Owns and coordinates every entity that makes up the current scene.
 *
 * Scene is the single source of truth for what exists in the world. It owns
 * all entities via std::unique_ptr, which keeps their addresses stable as the
 * vector grows so cached Entity* handles (e.g. the active camera) stay valid,
 * drives their per-frame lifecycle, and tracks which entity acts as the
 * active camera.
 */
class Scene
{

public:

	/**
	 * @brief Constructs the scene and populates it with its initial entities.
	 */
	Scene();

	/**
	 * @brief Default destructor.
	 */
	~Scene() = default;

	// ----------------------------------------------
	// LIFECYCLE
	// ----------------------------------------------

	/**
	 * @brief Ticks every entity in the scene by the given delta.
	 *
	 * @param deltaTime  Time elapsed since the previous frame, in seconds.
	 */
	void Update(std::chrono::duration<float> deltaTime);


	// ----------------------------------------------
	// ENTITIES MANAGEMENT
	// ----------------------------------------------

	/**
	 * @brief Create a new entity owned by the scene.
	 *
	 * @param name The name of the entity.
	 *
	 * @return A pointer to the newly created entity.
	 */
	Entity* AddEntity(const std::string& name);


	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Get an entity by name.
	 *
	 * @param name The name of the entity to look up.
	 *
	 * @return A pointer to the entity, or nullptr if not found.
	 */
	Entity* GetEntity(const std::string& name) const;

	/**
	 * @brief Returns the entity used as the active camera, or nullptr if none is bound.
	 */
	Entity* GetActiveCamera() const { return m_activeCamera; };

	/**
	 * @brief Set the entity used as the active camera.
	 *
	 * @param camera The entity to use as the active camera.
	 */
	void SetActiveCamera(Entity* camera) { m_activeCamera = camera; };

private:

	// ----------------------------------------------
	// INTERNAL HELPERS
	// ----------------------------------------------

	/**
	 * @brief Initialize the scene.
	 */
	void Init();

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	std::vector<std::unique_ptr<Entity>>	m_entities;					// Owned entity instances. unique_ptr keeps addresses stable as the vector grows.

	Entity*									m_activeCamera{ nullptr };	// Entity currently used as the active camera. Not owned by this class.
};

