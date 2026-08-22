
#include "backend.h"

// imgui
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// SDL2
#include <SDL2/SDL.h>


bool Backend::Init()
{}

void Backend::Run(const std::function<void()>& drawCallback)
{}

void Backend::BeginFrame()
{}

void Backend::EndFrame()
{}

bool Backend::ShouldClose() const
{}
