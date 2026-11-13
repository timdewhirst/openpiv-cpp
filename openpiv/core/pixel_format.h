#pragma once

// std
#include <array>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

// local
#include "core/exception_builder.h"
#include "core/format_utils.h"
#include "core/pixel_types.h"

namespace openpiv::core {

/// runtime pixel format: the cross product of pixel families
/// (grey, rgb, rgba, yuv, yuva, complex) with pixel depths
/// (u8, u16, u32, f32, f64)
enum class pixel_format : uint8_t
{
    UNKNOWN = 0,

    GREY_U8,   GREY_U16,  GREY_U32,  GREY_F32,  GREY_F64,
    RGB_U8,    RGB_U16,   RGB_U32,    RGB_F32,   RGB_F64,
    RGBA_U8,   RGBA_U16,  RGBA_U32,   RGBA_F32,  RGBA_F64,
    YUV_U8,    YUV_U16,   YUV_U32,    YUV_F32,   YUV_F64,
    YUVA_U8,   YUVA_U16,  YUVA_U32,   YUVA_F32,  YUVA_F64,
    COMPLEX_U8, COMPLEX_U16, COMPLEX_U32, COMPLEX_F32, COMPLEX_F64,
};

/// layout family of a pixel
enum class pixel_family : uint8_t
{
    UNKNOWN = 0,
    GREY,
    RGB,
    RGBA,
    YUV,
    YUVA,
    COMPLEX,
};

/// fundamental type of the components of a pixel
enum class pixel_depth : uint8_t
{
    UNKNOWN = 0,
    U8,
    U16,
    U32,
    F32,
    F64,
};

/// descriptive information about a pixel format
struct pixel_format_info
{
    std::string_view name;
    pixel_family family;
    pixel_depth depth;
    uint8_t channels;        // components per pixel
    uint8_t bytes_per_pixel;  // channels * depth bytes
    bool is_mono;            // greyscale
    bool is_complex;         // complex value pair
};

/// size of a depth's fundamental type, in bytes
constexpr uint8_t depth_bytes( pixel_depth d )
{
    switch ( d )
    {
        case pixel_depth::U8: return 1;
        case pixel_depth::U16: return 2;
        case pixel_depth::U32: return 4;
        case pixel_depth::F32: return 4;
        case pixel_depth::F64: return 8;
        default: return 0;
    }
}

/// number of components in a family's pixel
constexpr uint8_t family_channels( pixel_family f )
{
    switch ( f )
    {
        case pixel_family::GREY: return 1;
        case pixel_family::RGB: return 3;
        case pixel_family::RGBA: return 4;
        case pixel_family::YUV: return 3;
        case pixel_family::YUVA: return 4;
        case pixel_family::COMPLEX: return 2;
        default: return 0;
    }
}

/// greyscale format of a given depth
constexpr pixel_format grey_format_for_depth( pixel_depth d )
{
    switch ( d )
    {
        case pixel_depth::U8: return pixel_format::GREY_U8;
        case pixel_depth::U16: return pixel_format::GREY_U16;
        case pixel_depth::U32: return pixel_format::GREY_U32;
        case pixel_depth::F32: return pixel_format::GREY_F32;
        case pixel_depth::F64: return pixel_format::GREY_F64;
        default: return pixel_format::UNKNOWN;
    }
}

/// complex format of a given depth; note that integral complex
/// pixel types are signed (complex<int8_t> etc.) while the
/// corresponding greyscale types are unsigned - use this mapping
/// when converting between complex and grey depths
constexpr pixel_format complex_format_for_depth( pixel_depth d )
{
    switch ( d )
    {
        case pixel_depth::U8: return pixel_format::COMPLEX_U8;
        case pixel_depth::U16: return pixel_format::COMPLEX_U16;
        case pixel_depth::U32: return pixel_format::COMPLEX_U32;
        case pixel_depth::F32: return pixel_format::COMPLEX_F32;
        case pixel_depth::F64: return pixel_format::COMPLEX_F64;
        default: return pixel_format::UNKNOWN;
    }
}

/// descriptive information about \a f; the formats are laid out so
/// that a format is ( family x depth ) i.e. family = (f-1)/5 and
/// depth = (f-1)%5
constexpr pixel_format_info format_info( pixel_format f )
{
    constexpr std::array<std::string_view, 30> names{ {
        "grey_u8",     "grey_u16",     "grey_u32",     "grey_f32",     "grey_f64",
        "rgb_u8",      "rgb_u16",      "rgb_u32",      "rgb_f32",      "rgb_f64",
        "rgba_u8",     "rgba_u16",     "rgba_u32",     "rgba_f32",     "rgba_f64",
        "yuv_u8",      "yuv_u16",      "yuv_u32",      "yuv_f32",      "yuv_f64",
        "yuva_u8",     "yuva_u16",     "yuva_u32",     "yuva_f32",     "yuva_f64",
        "complex_u8",  "complex_u16",  "complex_u32",  "complex_f32",  "complex_f64",
    } };
    constexpr std::array<pixel_family, 6> families{ {
        pixel_family::GREY,
        pixel_family::RGB,
        pixel_family::RGBA,
        pixel_family::YUV,
        pixel_family::YUVA,
        pixel_family::COMPLEX,
    } };
    constexpr std::array<pixel_depth, 5> depths{ {
        pixel_depth::U8,
        pixel_depth::U16,
        pixel_depth::U32,
        pixel_depth::F32,
        pixel_depth::F64,
    } };

    if ( f == pixel_format::UNKNOWN ||
         static_cast<uint8_t>(f) > static_cast<uint8_t>(pixel_format::COMPLEX_F64) )
        return { "unknown", pixel_family::UNKNOWN, pixel_depth::UNKNOWN, 0, 0, false, false };

    const uint8_t index{ static_cast<uint8_t>( static_cast<uint8_t>(f) - 1 ) };
    const pixel_family family{ families[ index / 5 ] };
    const pixel_depth depth{ depths[ index % 5 ] };
    const uint8_t channels{ family_channels( family ) };

    return {
        names[ index ],
        family,
        depth,
        channels,
        static_cast<uint8_t>( channels * depth_bytes( depth ) ),
        family == pixel_family::GREY,
        family == pixel_family::COMPLEX
    };
}

/// map a compile-time pixel type to its runtime format
template < typename T >
struct format_for : std::integral_constant< pixel_format, pixel_format::UNKNOWN > {};

#define OPENPIV_FORMAT_FOR( TYPE, FMT )                           \
    template <>                                                   \
    struct format_for< TYPE >                                     \
        : std::integral_constant< pixel_format, pixel_format::FMT > {};

OPENPIV_FORMAT_FOR( g_8,   GREY_U8 )
OPENPIV_FORMAT_FOR( g_16,  GREY_U16 )
OPENPIV_FORMAT_FOR( g_32,  GREY_U32 )
OPENPIV_FORMAT_FOR( g_f32, GREY_F32 )
OPENPIV_FORMAT_FOR( g_f,   GREY_F64 )

OPENPIV_FORMAT_FOR( rgb_8,   RGB_U8 )
OPENPIV_FORMAT_FOR( rgb_16,  RGB_U16 )
OPENPIV_FORMAT_FOR( rgb_32,  RGB_U32 )
OPENPIV_FORMAT_FOR( rgb_f32, RGB_F32 )
OPENPIV_FORMAT_FOR( rgb_f,   RGB_F64 )

OPENPIV_FORMAT_FOR( rgba_8,   RGBA_U8 )
OPENPIV_FORMAT_FOR( rgba_16,  RGBA_U16 )
OPENPIV_FORMAT_FOR( rgba_32,  RGBA_U32 )
OPENPIV_FORMAT_FOR( rgba_f32, RGBA_F32 )
OPENPIV_FORMAT_FOR( rgba_f,   RGBA_F64 )

OPENPIV_FORMAT_FOR( yuv_8,   YUV_U8 )
OPENPIV_FORMAT_FOR( yuv_16,  YUV_U16 )
OPENPIV_FORMAT_FOR( yuv_32,  YUV_U32 )
OPENPIV_FORMAT_FOR( yuv_f32, YUV_F32 )
OPENPIV_FORMAT_FOR( yuv_f,   YUV_F64 )

OPENPIV_FORMAT_FOR( yuva_8,   YUVA_U8 )
OPENPIV_FORMAT_FOR( yuva_16,  YUVA_U16 )
OPENPIV_FORMAT_FOR( yuva_32,  YUVA_U32 )
OPENPIV_FORMAT_FOR( yuva_f32, YUVA_F32 )
OPENPIV_FORMAT_FOR( yuva_f,   YUVA_F64 )

OPENPIV_FORMAT_FOR( c_8,   COMPLEX_U8 )
OPENPIV_FORMAT_FOR( c_16,  COMPLEX_U16 )
OPENPIV_FORMAT_FOR( c_32,  COMPLEX_U32 )
OPENPIV_FORMAT_FOR( c_f32, COMPLEX_F32 )
OPENPIV_FORMAT_FOR( c_f,   COMPLEX_F64 )

#undef OPENPIV_FORMAT_FOR

template < typename T >
inline constexpr pixel_format format_for_v = format_for<T>::value;

/// map a runtime format to its compile-time pixel type
template < pixel_format F >
struct pixel_type_for
{
    using type = void;
};

#define OPENPIV_PIXEL_TYPE_FOR( FMT, TYPE ) \
    template <> struct pixel_type_for< pixel_format::FMT > { using type = TYPE; };

OPENPIV_PIXEL_TYPE_FOR( GREY_U8,   g_8 )
OPENPIV_PIXEL_TYPE_FOR( GREY_U16,  g_16 )
OPENPIV_PIXEL_TYPE_FOR( GREY_U32,  g_32 )
OPENPIV_PIXEL_TYPE_FOR( GREY_F32,  g_f32 )
OPENPIV_PIXEL_TYPE_FOR( GREY_F64,  g_f )

OPENPIV_PIXEL_TYPE_FOR( RGB_U8,   rgb_8 )
OPENPIV_PIXEL_TYPE_FOR( RGB_U16,  rgb_16 )
OPENPIV_PIXEL_TYPE_FOR( RGB_U32,  rgb_32 )
OPENPIV_PIXEL_TYPE_FOR( RGB_F32,  rgb_f32 )
OPENPIV_PIXEL_TYPE_FOR( RGB_F64,  rgb_f )

OPENPIV_PIXEL_TYPE_FOR( RGBA_U8,   rgba_8 )
OPENPIV_PIXEL_TYPE_FOR( RGBA_U16,  rgba_16 )
OPENPIV_PIXEL_TYPE_FOR( RGBA_U32,  rgba_32 )
OPENPIV_PIXEL_TYPE_FOR( RGBA_F32,  rgba_f32 )
OPENPIV_PIXEL_TYPE_FOR( RGBA_F64,  rgba_f )

OPENPIV_PIXEL_TYPE_FOR( YUV_U8,   yuv_8 )
OPENPIV_PIXEL_TYPE_FOR( YUV_U16,  yuv_16 )
OPENPIV_PIXEL_TYPE_FOR( YUV_U32,  yuv_32 )
OPENPIV_PIXEL_TYPE_FOR( YUV_F32,  yuv_f32 )
OPENPIV_PIXEL_TYPE_FOR( YUV_F64,  yuv_f )

OPENPIV_PIXEL_TYPE_FOR( YUVA_U8,   yuva_8 )
OPENPIV_PIXEL_TYPE_FOR( YUVA_U16,  yuva_16 )
OPENPIV_PIXEL_TYPE_FOR( YUVA_U32,  yuva_32 )
OPENPIV_PIXEL_TYPE_FOR( YUVA_F32,  yuva_f32 )
OPENPIV_PIXEL_TYPE_FOR( YUVA_F64,  yuva_f )

OPENPIV_PIXEL_TYPE_FOR( COMPLEX_U8,   c_8 )
OPENPIV_PIXEL_TYPE_FOR( COMPLEX_U16,  c_16 )
OPENPIV_PIXEL_TYPE_FOR( COMPLEX_U32,  c_32 )
OPENPIV_PIXEL_TYPE_FOR( COMPLEX_F32,  c_f32 )
OPENPIV_PIXEL_TYPE_FOR( COMPLEX_F64,  c_f )

#undef OPENPIV_PIXEL_TYPE_FOR

template < pixel_format F >
using pixel_type_for_t = typename pixel_type_for<F>::type;

/// ostream operator
inline std::ostream& operator<<( std::ostream& os, pixel_format f )
{
    return os << format_info( f ).name;
}

namespace detail {

    /// tag carrying a compile-time pixel type
    template < typename T >
    struct pixel_tag
    {
        using type = T;
    };

}

/// invoke \a fn with a pixel_tag for the compile-time fundamental
/// type matching \a d; useful for dispatching on runtime depths to
/// templated implementations
template < typename F >
decltype(auto) dispatch_pixel_depth( pixel_depth d, F&& fn )
{
    switch ( d )
    {
#define OPENPIV_DEPTH_TAG_CASE( DEPTH, TYPE )                                         \
    case pixel_depth::DEPTH:                                                          \
        return fn( detail::pixel_tag< TYPE >{} );

        OPENPIV_DEPTH_TAG_CASE( U8,  uint8_t )
        OPENPIV_DEPTH_TAG_CASE( U16, uint16_t )
        OPENPIV_DEPTH_TAG_CASE( U32, uint32_t )
        OPENPIV_DEPTH_TAG_CASE( F32, float )
        OPENPIV_DEPTH_TAG_CASE( F64, double )

#undef OPENPIV_DEPTH_TAG_CASE

        default:
            core::exception_builder<std::runtime_error>()
                << "unknown pixel depth: " << format_info( pixel_format::UNKNOWN ).name;
            std::unreachable();
    }
}

/// invoke \a fn with a pixel_tag for the compile-time pixel type
/// matching \a f; useful for dispatching on runtime formats to
/// templated implementations
template < typename F >
decltype(auto) dispatch_pixel_type( pixel_format f, F&& fn )
{
    switch ( f )
    {
#define OPENPIV_PIXEL_TAG_CASE( FMT, PIXELT )                                         \
    case pixel_format::FMT:                                                           \
        return fn( detail::pixel_tag< PIXELT >{} );

        OPENPIV_PIXEL_TAG_CASE( GREY_U8,   g_8 )
        OPENPIV_PIXEL_TAG_CASE( GREY_U16,  g_16 )
        OPENPIV_PIXEL_TAG_CASE( GREY_U32,  g_32 )
        OPENPIV_PIXEL_TAG_CASE( GREY_F32,  g_f32 )
        OPENPIV_PIXEL_TAG_CASE( GREY_F64,  g_f )
        OPENPIV_PIXEL_TAG_CASE( RGB_U8,    rgb_8 )
        OPENPIV_PIXEL_TAG_CASE( RGB_U16,   rgb_16 )
        OPENPIV_PIXEL_TAG_CASE( RGB_U32,   rgb_32 )
        OPENPIV_PIXEL_TAG_CASE( RGB_F32,   rgb_f32 )
        OPENPIV_PIXEL_TAG_CASE( RGB_F64,   rgb_f )
        OPENPIV_PIXEL_TAG_CASE( RGBA_U8,   rgba_8 )
        OPENPIV_PIXEL_TAG_CASE( RGBA_U16,  rgba_16 )
        OPENPIV_PIXEL_TAG_CASE( RGBA_U32,  rgba_32 )
        OPENPIV_PIXEL_TAG_CASE( RGBA_F32,  rgba_f32 )
        OPENPIV_PIXEL_TAG_CASE( RGBA_F64,  rgba_f )
        OPENPIV_PIXEL_TAG_CASE( YUV_U8,    yuv_8 )
        OPENPIV_PIXEL_TAG_CASE( YUV_U16,   yuv_16 )
        OPENPIV_PIXEL_TAG_CASE( YUV_U32,   yuv_32 )
        OPENPIV_PIXEL_TAG_CASE( YUV_F32,   yuv_f32 )
        OPENPIV_PIXEL_TAG_CASE( YUV_F64,   yuv_f )
        OPENPIV_PIXEL_TAG_CASE( YUVA_U8,   yuva_8 )
        OPENPIV_PIXEL_TAG_CASE( YUVA_U16,  yuva_16 )
        OPENPIV_PIXEL_TAG_CASE( YUVA_U32,  yuva_32 )
        OPENPIV_PIXEL_TAG_CASE( YUVA_F32,  yuva_f32 )
        OPENPIV_PIXEL_TAG_CASE( YUVA_F64,  yuva_f )
        OPENPIV_PIXEL_TAG_CASE( COMPLEX_U8,   c_8 )
        OPENPIV_PIXEL_TAG_CASE( COMPLEX_U16,  c_16 )
        OPENPIV_PIXEL_TAG_CASE( COMPLEX_U32,  c_32 )
        OPENPIV_PIXEL_TAG_CASE( COMPLEX_F32,  c_f32 )
        OPENPIV_PIXEL_TAG_CASE( COMPLEX_F64,  c_f )

#undef OPENPIV_PIXEL_TAG_CASE

        default:
            core::exception_builder<std::runtime_error>()
                << "unknown pixel format: " << f;
            std::unreachable();
    }
}

}

template <>
struct std::formatter<openpiv::core::pixel_format> : openpiv::core::StreamableFormatter<openpiv::core::pixel_format>
{};
