
#include "sdl2_backend.h"

// std
#include <cstdint>
#include <optional>

// openpiv
#include "core/image_utils.h"
#include "core/log.h"
#include "core/pixel_format.h"
#include "core/pixel_types.h"

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

namespace
{
    /// the display format is chosen so that packed rgba_8 pixels
    /// (bytes r, g, b, a) can be uploaded without swizzling: the SDL
    /// format's red mask must cover the first byte in memory, which
    /// is ABGR8888 on little-endian and RGBA8888 on big-endian.
    /// (SDL's *A8888 names refer to masks within a 32-bit word, not
    /// to memory order on this platform)
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    constexpr uint32_t display_format = SDL_PIXELFORMAT_ABGR8888;
#else
    constexpr uint32_t display_format = SDL_PIXELFORMAT_RGBA8888;
#endif
}

ImTextureID SDL2Backend::load_image(const openpiv::core::image& im)
{
    using namespace openpiv::core;

    if (im.width() == 0 || im.height() == 0) {
        logger::error("cannot create texture from an empty image");
        return ImTextureID_Invalid;
    }

    // the renderer displays rgba8: upload directly where possible,
    // otherwise convert. The pixel conversion machinery covers the
    // grey and rgba families at any depth; rgb has no rgba
    // conversion, so it is expanded per channel here
    const image* upload{&im};
    std::optional<image> converted;

    if (im.format() != pixel_format::RGBA_U8) {
        switch (im.info().family) {
        case pixel_family::GREY:
        case pixel_family::RGBA:
            converted = convert(im, pixel_format::RGBA_U8);
            break;

        case pixel_family::RGB:
            converted = image( im.size(), pixel_format::RGBA_U8 );
            dispatch_pixel_depth(
                im.info().depth,
                [&im, &converted]( auto tag )
                {
                    using V = typename decltype( tag )::type;
                    using PixelT = rgb<V>;

                    // im may be a sub-image, so advance rows by the pitch
                    const size_t in_stride{ im.pitch() / sizeof( PixelT ) };
                    const size_t out_stride{ converted->pitch() / sizeof( rgba_8 ) };

                    for ( uint32_t h = 0; h < im.height(); ++h )
                    {
                        const PixelT* in{ im.pixels<PixelT>() + h * in_stride };
                        rgba_8* out{ converted->pixels<rgba_8>() + h * out_stride };
                        for ( uint32_t w = 0; w < im.width(); ++w )
                            out[w] = rgba_8( static_cast<uint8_t>( in[w].r ),
                                             static_cast<uint8_t>( in[w].g ),
                                             static_cast<uint8_t>( in[w].b ),
                                             255 );
                    }
                } );
            break;

        default:
            logger::error("unsupported pixel format for display: {}", im.format());
            return ImTextureID_Invalid;
        }

        upload = &converted.value();
    }

    // packed rgba_8 uploads directly: the display format's masks
    // match the pixel memory layout (see display_format above)
    SDL_Texture* texture = SDL_CreateTexture(
        renderer_,
        display_format,
        SDL_TEXTUREACCESS_STATIC,
        static_cast<int>(upload->width()),
        static_cast<int>(upload->height()));

    if (!texture) {
        logger::error("failed to create SDL texture: {}", SDL_GetError());
        return ImTextureID_Invalid;
    }

    SDL_UpdateTexture(texture,
                      nullptr,
                      upload->pixels<rgba_8>(),
                      static_cast<int>(upload->pitch()));
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    return static_cast<ImTextureID>( reinterpret_cast<intptr_t>(texture) );
}

void SDL2Backend::unload_image(ImTextureID texture)
{
    if (texture != ImTextureID_Invalid) {
        SDL_DestroyTexture(
            reinterpret_cast<SDL_Texture*>( static_cast<intptr_t>(texture) ));
    }
}
