
#include "backend.h"

// concrete subclasses
#include "sdl3_backend.h"

std::unique_ptr<Backend> Backend::create(std::string_view)
{
    // only SDL3 atm
    return std::make_unique<SDL3Backend>();
}
