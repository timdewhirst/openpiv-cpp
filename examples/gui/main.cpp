// std
#include <iostream>

// file
#include "ImGuiFD/ImGuiFD.h"

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// SDL2
#include <SDL2/SDL.h>

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set up SDL window
    const int width = 1280;
    const int height = 720;
    SDL_Window* window = SDL_CreateWindow(
        "OpenPIV GUI",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create SDL renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Set up Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // Note: Docking and Viewports require defining IMGUI_ENABLE_DOCKING and IMGUI_ENABLE_VIEWPORTS
    // For now, we'll keep it simple without these features
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Set up Dear ImGui style
    ImGui::StyleColorsDark();

    // Initialize platform/renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Main loop
    bool done = false;
    SDL_Event event;
    std::string filename;
    bool file_dialog_open = false;

    while (!done) {
        // Poll SDL events
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

        // main menu
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    std::cout << "opening file dialog...\n";
                    ImGuiFD::OpenDialog("Choose File", ImGuiFDMode_LoadFile, ".", ".*");
                    file_dialog_open = true;
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) { /* save action */ }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Alt+F4")) { done = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Cut", "Ctrl+X")) { /* ... */ }
                if (ImGui::MenuItem("Copy", "Ctrl+C")) { /* ... */ }
                if (ImGui::MenuItem("Paste", "Ctrl+V")) { /* ... */ }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (file_dialog_open) {
            if (ImGuiFD::BeginDialog("Choose File")) {
                if (ImGuiFD::ActionDone()) {
                    if (ImGuiFD::SelectionMade()) {
                        filename = ImGuiFD::GetSelectionPathString(0);
                    }
                    file_dialog_open = false;
                    ImGuiFD::CloseCurrentDialog();
                }
                ImGuiFD::EndDialog();
            }
        }

        // Main window
        ImGui::Begin("OpenPIV GUI");

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

        ImGui::Text("Filename: %s", filename.c_str());

        // FPS display
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // Demo window option
        static bool show_demo = false;
        if (ImGui::Button("Show ImGui Demo Window")) {
            show_demo = !show_demo;
        }

        ImGui::End();

        // Optional: Show ImGui demo window
        if (show_demo) {
            ImGui::ShowDemoWindow(&show_demo);
        }

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
