
#include "backend.h"

// concrete subclasses
#include "sdl2_backend.h"

std::unique_ptr<Backend> Backend::create(std::string_view)
{
    // only SDL2 atm
    return std::make_unique<SDL2Backend>();
}
