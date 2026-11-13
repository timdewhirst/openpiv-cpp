// std
#include <algorithm>
#include <any>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>
#include <variant>

// utils
#include <cxxopts.hpp>

// file
#include "ImGuiFD/ImGuiFD.h"

// Dear ImGui
#include "imgui.h"

// openpiv
#include "core/format_utils.h"
#include "core/log.h"
#include "loaders/image_loader.h"

using namespace openpiv;
namespace logger = openpiv::core::logger;

// local
#include "backend.h"
#include "property.h"


struct RenderNode;
using RenderNodeList = std::list<RenderNode>;
class RenderNodeManager;

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

    void set_state(const Properties& ps)
    {
        for (const auto& [k, v] : ps)
            _properties[k] = v;
    }

    void set_state(std::string k, PropertyValue v)
    {
        _properties[k] = std::move(v);
    }

    const Properties& state() const
    {
        return _properties;
    }

    template <typename T, class Self>
    decltype(auto) state(this Self&& self, const std::string& key)
    {
        static T static_t {};
        auto I = self._properties.find(key);
        if (I == self._properties.end())
            return (static_t);

        T* value = std::get_if<T>(&I->second);
        return value ? (*value) : (static_t);
    }

private:
    RenderNodeList _active;
    Properties _properties;
};

void setup_demo(RenderNodeManager& m)
{
    m.set_state("show_demo", true);
    m.insert("show demo",
             [](RenderNodeManager& mgr, RenderNodeList::iterator) -> RenderNode::result_t {
                 // Optional: Show ImGui demo window
                 bool& show_demo = mgr.state<bool>("show_demo");
                 if (!show_demo)
                     return RenderNode::RESULT_DISPOSE;

                 ImGui::ShowDemoWindow(&show_demo);
                 return RenderNode::RESULT_OK;
             });
}

void setup_dialog(RenderNodeManager& m)
{
    m.set_state("dialog_active", true);
    m.insert("show dialog",
             [](RenderNodeManager& mgr, RenderNodeList::iterator) -> RenderNode::result_t {
                 bool& dialog_active = mgr.state<bool>("dialog_active");

                 if (!dialog_active)
                     return RenderNode::RESULT_DISPOSE;

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
                     setup_demo(mgr);
                 }

                 ImGui::End();
                 return RenderNode::RESULT_OK;
             });
}

/// Load an image from \a path and open a new window displaying its
/// content; the window owns the display texture for its lifetime,
/// releasing it via unload_image when closed
void setup_image_window(RenderNodeManager& m, Backend* backend, const std::string& path)
{
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        logger::error("failed to open {}", path);
        return;
    }

    auto loader{ openpiv::core::image_loader_registry::instance().find(is) };
    if (!loader) {
        logger::error("failed to find image loader for {}", path);
        return;
    }

    auto im{ loader->load(is) };
    if (!im) {
        logger::error("failed to load image from {}", path);
        return;
    }

    if (!backend) {
        logger::error("no backend available to display {}", path);
        return;
    }

    const ImTextureID texture{ backend->load_image(*im) };
    if (texture == ImTextureID_Invalid) {
        logger::error("failed to display image: {}", path);
        return;
    }

    // window title from the file name; the ### suffix keeps the
    // imgui identity unique so the same file can be opened into
    // more than one window
    static unsigned int window_count{ 0 };
    const std::string filename{ path.substr(path.find_last_of("/\\") + 1) };
    const std::string title{ std::format("{}###image-{}", filename, window_count++) };

    m.insert("show image: " + filename,
             [backend, texture, title,
              w = static_cast<float>(im->width()),
              h = static_cast<float>(im->height()),
              open = true]
             (RenderNodeManager&, RenderNodeList::iterator) mutable -> RenderNode::result_t
             {
                 // open at the natural image size; the window is
                 // resizable
                 ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
                 if (!ImGui::Begin(title.c_str(), &open)) {
                     ImGui::End();
                     if (!open) {
                         backend->unload_image(texture);
                         return RenderNode::RESULT_DISPOSE;
                     }
                     return RenderNode::RESULT_OK;
                 }

                 // fit the image within the window, preserving the
                 // aspect ratio
                 const ImVec2 avail{ ImGui::GetContentRegionAvail() };
                 const float scale{ std::min(avail.x / w, avail.y / h) };
                 ImGui::Image(ImTextureRef(texture), ImVec2(w * scale, h * scale));
                 ImGui::End();
                 return RenderNode::RESULT_OK;
             });
}

void setup_app_menu(RenderNodeManager& m, Backend* backend)
{
    m.insert("build app menu",
             [backend](RenderNodeManager& mgr, RenderNodeList::iterator) -> RenderNode::result_t {
                 if (ImGui::BeginMainMenuBar()) {
                     if (ImGui::BeginMenu("File")) {
                         if (ImGui::MenuItem("Open", "Ctrl+O")) {
                             std::cout << "opening file dialog...\n";
                             mgr.insert("file open",
                                        [backend, done=false, opened=false](RenderNodeManager& mgr, RenderNodeList::iterator self) mutable -> RenderNode::result_t {
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
                                                        const std::string path{ ImGuiFD::GetSelectionPathString(0) };
                                                        mgr.set_state("filename", path);

                                                        // load the chosen file and open a
                                                        // window displaying its content
                                                        setup_image_window(mgr, backend, path);
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
                         if (ImGui::MenuItem("Demo", "Ctrl+D")) setup_demo(mgr);
                         if (ImGui::MenuItem("Main", "Ctrl+M")) setup_dialog(mgr);
                         ImGui::Separator();
                         if (ImGui::MenuItem("Quit", "Alt+F4")) mgr.set_state("app_done", true);
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

int main(int argc, char* argv[])
{
    cxxopts::Options options(argv[0]);
    auto log_level = logger::Level::INFO;

    try {
        options
            .add_options()
            ("h, help", "help", cxxopts::value<bool>())
            ("loglevel", "log level", cxxopts::value<logger::Level>(log_level)->default_value("INFO"));

        auto result = options.parse(argc, argv);

        // setup logger
        logger::Logger::instance().add_sink(
            [log_level](logger::Level l, const std::string& m) -> bool
            {
                if ( l > log_level )
                    return true;

                std::cerr << m << "\n";
                return true;
            });

        if (result.count("help"))
        {
            std::cout << options.help({""}) << "\n";
            return 0;
        }
    }
    catch (const std::exception& e) {
        logger::error("error parsing options: {}", e.what());
        return 1;
    }

    auto backend = Backend::create("sdl2");
    if (!backend) {
        logger::error("failed to create backend");
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

    // main gui
    RenderNodeManager mgr;
    mgr.set_state("dialog_active", true);

    // setup
    setup_app_menu(mgr, backend.get());
    setup_demo(mgr);
    setup_dialog(mgr);

    backend->init("openpiv");

    // run
    backend->run([&mgr](){
        if (mgr.state<bool>("app_done"))
            return true;

        mgr.poll();
        return false;
    });

    return 0;
}
