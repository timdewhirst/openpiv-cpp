#pragma once

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

// openpiv
#include "core/image.h"

// local
#include "backend.h"

class SDL3Backend : public Backend
{
public:
    bool init(std::string_view) override;
    void run(const std::function<bool()>& draw_callback) override;
    void begin_frame() override;
    void end_frame() override;
    bool should_close() const override;

    /// Load an image as a texture for display; the identifier is
    /// for use with ImGui and must be released with
    /// unload_image
    ImTextureID load_image(const openpiv::core::image& im) override;

    /// Unload a texture previously loaded with load_image
    void unload_image(ImTextureID texture) override;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool done_ = false;
};
