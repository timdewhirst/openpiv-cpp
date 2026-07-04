// std
#include <functional>
#include <iostream>
#include <list>
#include <string>

// file
#include "ImGuiFD/ImGuiFD.h"

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// SDL2
#include <SDL2/SDL.h>

struct RenderNode;
using RenderNodeList = std::list<RenderNode>;
class RenderNodeManager;

/// an intrusive list of nodes that can remove themselves once finished
struct RenderNode {
    enum result_t {
        RESULT_OK,
        RESULT_ERROR,
        RESULT_DISPOSE
    };
    using render_t = std::function<result_t(RenderNodeManager&, RenderNodeList::iterator)>;

    std::string id;
    render_t render = {};
};

class RenderNodeManager
{
public:
    void insert(std::string id, RenderNode::render_t&& fn)
    {
        _active.push_front(RenderNode{.id = id, .render = fn});
    }

    void poll()
    {
        for (auto I=_active.begin(); I!=_active.end(); ++I) {
            auto result = I->render(*this, I);
            switch (result) {
            case RenderNode::RESULT_ERROR:
                // \todo: handle this better
                std::cerr << "error from node: " << I->id << "\n";
                break;

            case RenderNode::RESULT_DISPOSE:
                I = _active.erase(I);
                break;

            case RenderNode::RESULT_OK:
            default:
                break;
            }
        }
    }

private:
    RenderNodeList _active;
};



static bool show_demo = false;
static bool app_done = false;
static std::string filename;


void setup_app_menu(RenderNodeManager& m)
{
    m.insert("build app menu",
             [](RenderNodeManager& mgr, RenderNodeList::iterator) -> RenderNode::result_t {
                 if (ImGui::BeginMainMenuBar()) {
                     if (ImGui::BeginMenu("File")) {
                         if (ImGui::MenuItem("Open", "Ctrl+O")) {
                             std::cout << "opening file dialog...\n";
                             mgr.insert("file open",
                                        [done=false, opened=false](RenderNodeManager& mgr, RenderNodeList::iterator self) mutable -> RenderNode::result_t {
                                            if (done) {
                                                return RenderNode::RESULT_DISPOSE;
                                            }

                                            if (!opened) {
                                                ImGuiFD::OpenDialog("Choose File", ImGuiFDMode_LoadFile, ".", ".*");
                                                opened = true;
                                            }

                                            if (ImGuiFD::BeginDialog("Choose File")) {
                                                if (ImGuiFD::ActionDone()) {
                                                    if (ImGuiFD::SelectionMade()) {
                                                        filename = ImGuiFD::GetSelectionPathString(0);
                                                    }
                                                    ImGuiFD::CloseCurrentDialog();
                                                    done = true;
                                                }
                                                ImGuiFD::EndDialog();
                                            }
                                            return RenderNode::RESULT_OK;
                                        });
                         }
                         if (ImGui::MenuItem("Save", "Ctrl+S")) { /* save action */ }
                         ImGui::Separator();
                         if (ImGui::MenuItem("Quit", "Alt+F4")) { app_done = true; }
                         ImGui::EndMenu();
                     }
                     if (ImGui::BeginMenu("Edit")) {
                         if (ImGui::MenuItem("Cut", "Ctrl+X")) { /* ... */ }
                         if (ImGui::MenuItem("Copy", "Ctrl+C")) { /* ... */ }
                         if (ImGui::MenuItem("Paste", "Ctrl+V")) { /* ... */ }
                         ImGui::EndMenu();
                     }
                     ImGui::EndMainMenuBar();
                 }

                 return RenderNode::RESULT_OK;
             });
}

void setup_demo(RenderNodeManager& m)
{
    m.insert("show demo",
             [](RenderNodeManager&, RenderNodeList::iterator) -> RenderNode::result_t {
                 // Optional: Show ImGui demo window
                 if (!show_demo)
                     return RenderNode::RESULT_OK;

                 ImGui::ShowDemoWindow(&show_demo);
                 return RenderNode::RESULT_OK;
             });
}

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
    SDL_Event event;

    RenderNodeManager mgr;

    // setup
    setup_app_menu(mgr);
    setup_demo(mgr);


    while (!app_done) {
        // Poll SDL events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                app_done = true;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                app_done = true;
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

            ImGui::Text("Filename: %s", filename.c_str());

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
