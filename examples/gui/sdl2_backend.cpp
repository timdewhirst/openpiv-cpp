
#include "sdl2_backend.h"

// openpiv
#include "core/log.h"

// imgui
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

using namespace openpiv;
namespace logger = openpiv::core::logger;

bool SDL2Backend::init(std::string_view window_title)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logger::error("SDL_Init Error: {}", SDL_GetError());
        return false;
    }

    // Set up SDL window
    const int width = 1280;
    const int height = 720;
    window_ = SDL_CreateWindow(window_title.data(),
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               width,
                               height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
                               );

    if (!window_) {
        logger::error("SDL_CreateWindow error: {}", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Create SDL renderer
    renderer_ = SDL_CreateRenderer(window_,
                                   -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
                                   );

    if (!renderer_) {
        logger::error("SDL_CreateRenderer error: {}", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    // Initialize platform/renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer2_Init(renderer_);

    return true;
}

void SDL2Backend::run(const std::function<bool()>& draw_callback)
{
    // Main loop
    while (!done_) {
        begin_frame();
        done_ = draw_callback();
        end_frame();
    }

    // clean up - we're done
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

void SDL2Backend::begin_frame()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT) {
            done_ = true;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_)) {
            done_ = true;
        }
    }

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    ImGui::NewFrame();
}

void SDL2Backend::end_frame()
{
    // Rendering
    ImGui::Render();

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
}

bool SDL2Backend::should_close() const
{
    return done_;
}
