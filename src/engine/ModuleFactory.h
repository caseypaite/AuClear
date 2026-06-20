#pragma once
#include "RackModule.h"
#include <memory>

/** Creates a RackModule of the requested type. Returns nullptr for unknown types. */
std::unique_ptr<RackModule> makeModule (ModuleType type);
