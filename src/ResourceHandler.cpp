#include "ResourceHandler.h"

template <typename T>
T* ResourceHandler<T>::Get() const
{
	if (!m_resourceManager)
		return nullptr;

	return m_resourceManager->GetResource<T>(m_resourceId);
}

template <typename T>
bool ResourceHandler<T>::IsValid() const
{
	if (!m_resourceManager)
		return false;

	return m_resourceManager->HasResource<T>(m_resourceId);
}