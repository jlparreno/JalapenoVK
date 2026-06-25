#include "ResourceManager.h"

void ResourceManager::UnloadAllResources()
{
	for (auto& kv : m_resources)
	{
		auto& val = kv.second;

		for (auto& innerKv : val)
		{
			auto& loadedResource = innerKv.second;
			loadedResource->Unload();
		}
		val.clear();
	}
	m_resources.clear();
}