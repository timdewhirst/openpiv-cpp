
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
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return false;
  }

  // Set up SDL window
  const int width = 1280;
  const int height = 720;
  window_ = SDL_CreateWindow(
    window_title.data(),
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
  renderer_ = SDL_CreateRenderer(
    window_,
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

void Backend::run(const std::function<void()>& draw_callback)
{
  // Main loop
  SDL_Event event;
  bool done = false;
  while (!done) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      
      if (event.type == SDL_QUIT) {
	done = true;
      }
      if (event.type == SDL_WINDOWEVENT &&
	  event.window.event == SDL_WINDOWEVENT_CLOSE &&
	  event.window.windowID == SDL_GetWindowID(window)) {
	done = true;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();

    
        ImGui::NewFrame();

        mgr.poll();

        // Main window
        static bool dialog_active = true;
        if (dialog_active) {
            ImGui::Begin("OpenPIV GUI", &dialog_active);

            ImGui::Text("Welcome to OpenPIV GUI Example");
            ImGui::Text("This is a basic Dear ImGui + SDL2 application.");
            ImGui::Separator();

            // Simple demo content
            static int counter = 0;
            if (ImGui::Button("Increment")) {
                counter++;
            }
            ImGui::SameLine();
            if (ImGui::Button("Decrement")) {
                counter--;
            }
            ImGui::Text("Counter: %d", counter);

            ImGui::Text("Filename: %s", mgr.state<std::string>("filename").c_str());

            // FPS display
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            // Demo window option
            if (ImGui::Button("Show ImGui Demo Window")) {
                show_demo = !show_demo;
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();


	///
	
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

}

void Backend::begin_frame()
{}

void Backend::end_frame()
{}

bool Backend::should_close() const
{}
