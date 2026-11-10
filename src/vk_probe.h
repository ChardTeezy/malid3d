#pragma once

namespace malid3d {

// Loads the Vulkan loader, enumerates every GPU and writes its properties,
// features, limits, formats and extensions to the log. Runs at most once.
// Returns true if at least one Vulkan device was found.
bool run_vulkan_probe();

}  // namespace malid3d
