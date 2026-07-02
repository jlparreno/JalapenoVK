#pragma once

/** Selects between traditional Vulkan-Hpp include style and C++20 module import.
*
* When USE_CPP20_MODULES is enabled and supported by the compiler, Vulkan is
* consumed via the official vulkan_hpp module interface. Otherwise, the engine
* falls back to the header-based RAII Vulkan-Hpp include for compatibility.
*
* This logic is isolated in a dedicated header to centralize Vulkan backend
* configuration in a single include point. It avoids scattering conditional
* compilation across the codebase and ensures all Vulkan-dependent modules
* share the same API entry path.
*
* The Intellisense guard is required because many IDE language servers do not
* fully support C++20 modules yet, which would otherwise break code navigation
* and autocomplete.
*/

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif