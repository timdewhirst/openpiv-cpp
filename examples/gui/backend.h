#pragma once

// std
#include <functional>
#include <string_view>
#include <memory>

// imgui
#include "imgui.h"

// openpiv
#include "core/image.h"

/// abstract backend
class Backend
{
public:
    virtual ~Backend() = default;

    virtual bool init(std::string_view) = 0;
    virtual void run(const std::function<bool()>& draw_callback) = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual bool should_close() const = 0;

    /// Load an image as a texture for display; the identifier is
    /// for use with ImGui and must be released with
    /// unload_image()
    virtual ImTextureID load_image(const openpiv::core::image& im) = 0;

    /// Unload a texture previously loaded with load_image
    virtual void unload_image(ImTextureID texture) = 0;

    /// factory method
    static std::unique_ptr<Backend> create(std::string_view);
};
