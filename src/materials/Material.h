#pragma once

#include "core/VulkanIncludes.h"

/**
 * @brief Abstract base for all shading models.
 *
 * A Material only needs to identify its shading model and expose the descriptor set that goes with it. 
 * Different shading models need different per-material data and different fragment shaders.
 *
 * Each Mesh primitive owns its Material as a std::unique_ptr<Material> - see Mesh::m_materials. 
 * GeometryPass currently only knows about PBRMaterial (one pipeline, m_pbrPipeline); 
 * The day a second concrete Material exists, it will need a small pipeline[ShadingModel] registry 
 * and pick one per primitive via GetShadingModel().
 */
class Material
{

public:

	/**
	 * @brief Identifies which concrete Material subclass an instance belongs to (and which GeometryPass pipeline / descriptor set layout)
	 */
	enum ShadingModel
	{
		PBR
	};

	// ----------------------------------------------
	// VIRTUAL METHODS FOR ALL MATERIALS
	// ----------------------------------------------

	/**
	 * @brief Virtual destructor - Material is always held as a base-class pointer.
	 */
	virtual ~Material() = default;

	/**
	 * @brief Returns which shading model this material implements.
	 *
	 * @return The concrete subclass's ShadingModel.
	 */
	virtual ShadingModel GetShadingModel() const = 0;

	/**
	 * @brief Returns the material's descriptor set.
	 *
	 * Bound as set 1 alongside the pass's own set 0 (view/proj/light) when
	 * drawing a primitive that uses this material.
	 *
	 * @return Raw vk::DescriptorSet handle, valid for the lifetime of this Material.
	 */
	virtual vk::DescriptorSet GetDescriptorSet() const = 0;
};
