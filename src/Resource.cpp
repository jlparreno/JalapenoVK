#include "Resource.h"

bool Resource::Load()
{
	m_loaded = true;
	return true;
}

void Resource::Unload()
{
	m_loaded = false;
}