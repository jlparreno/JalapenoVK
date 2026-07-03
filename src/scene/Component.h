#pragma once

#include <chrono>
#include <string>

// Forward declarations
class Entity;

/**
 * @brief Base class for all components in the entity-component system.
 *
 * Components are the building blocks of the ECS architecture.
 * Each component encapsulates a specific behavior or property and is
 * attached to an Entity, which acts as its owner and lifecycle manager.
 */
class Component
{

public:

	/**
	 * @brief Constructor with optional name.
	 * 
	 * @param componentName The name of the component.
	 */
	explicit Component(const std::string& componentName = "Component") : m_name(componentName) {}

	/**
	 * @brief Virtual destructor for proper cleanup.
	 */
	virtual ~Component() = default;


	// ----------------------------------------------
	// VIRTUAL METHODS FOR ALL COMPONENTS
	// ----------------------------------------------

	/**
	 * @brief Initialize the component.
	 * 
	 * Called when the component is added to an entity.
	 */
	virtual void Init() {}

	/**
	 * @brief Update the component. Called every frame.
	 *
	 * @param deltaTime The time elapsed since the last frame, in seconds.
	 */
	virtual void Update(std::chrono::duration<float> deltaTime) {}

	/**
	 * @brief Render the component.
	 * 
	 * Called during the rendering phase.
	 */
	virtual void Render() {}


	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Set the owner entity of this component.
	 * 
	 * @param entity The entity that owns this component.
	 */
	void SetOwner(Entity* entity){ m_owner = entity; }

	/**
	 * @brief Get the owner entity of this component.
	 * 
	 * @return The entity that owns this component.
	 */
	Entity* GetOwner() const { return m_owner; }

	/**
	 * @brief Get the name of the component.
	 * 
	 * @return The name of the component.
	 */
	const std::string& GetName() const { return m_name; }

	/**
	 * @brief Check if the component is active.
	 * 
	 * @return True if the component is active, false otherwise.
	 */
	bool IsActive() const { return m_active; }

	/**
	 * @brief Set the active state of the component.
	 * 
	 * @param isActive The new active state.
	 */
	void SetActive(bool isActive) { m_active = isActive; }

protected:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	Entity*		m_owner{ nullptr }; // Owning entity. Not owned by this class.
	std::string m_name;             // Display name, used for debugging.
	bool        m_active{ true };   // Whether this component participates in update and render.
};

