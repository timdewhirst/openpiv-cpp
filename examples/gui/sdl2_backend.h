
#pragma once

// SDL
#include <SDL.h>

// local
#include "backend.h"

class SDL2Backend : public Backend
{
public:
    bool init(std::string_view) override;
    void run(const std::function<bool()>& draw_callback) override;
    void begin_frame() override;
    void end_frame() override;
    bool should_close() const override;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool done_ = false;
};
