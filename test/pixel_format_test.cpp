#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>

// std
#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

// to be tested
#include "core/pixel_format.h"

using namespace openpiv;
using namespace openpiv::core;

namespace {

    constexpr std::array<pixel_format, 30> all_formats{
        {
            pixel_format::GREY_U8, pixel_format::GREY_U16, pixel_format::GREY_U32, pixel_format::GREY_F32, pixel_format::GREY_F64,
            pixel_format::RGB_U8, pixel_format::RGB_U16, pixel_format::RGB_U32, pixel_format::RGB_F32, pixel_format::RGB_F64,
            pixel_format::RGBA_U8, pixel_format::RGBA_U16, pixel_format::RGBA_U32, pixel_format::RGBA_F32, pixel_format::RGBA_F64,
            pixel_format::YUV_U8, pixel_format::YUV_U16, pixel_format::YUV_U32, pixel_format::YUV_F32, pixel_format::YUV_F64,
            pixel_format::YUVA_U8, pixel_format::YUVA_U16, pixel_format::YUVA_U32, pixel_format::YUVA_F32, pixel_format::YUVA_F64,
            pixel_format::COMPLEX_U8, pixel_format::COMPLEX_U16, pixel_format::COMPLEX_U32, pixel_format::COMPLEX_F32, pixel_format::COMPLEX_F64,
        }
    };

#define OPENPIV_CHECK_FORMAT( FMT )                                                      \
    do {                                                                                 \
        constexpr pixel_format f{ pixel_format::FMT };                                    \
        using T = pixel_type_for_t< f >;                                                 \
        const pixel_format_info& fi{ format_info( f ) };                                 \
        CHECK( fi.bytes_per_pixel == sizeof( T ) );                                      \
        CHECK( fi.family != pixel_family::UNKNOWN );                                     \
        CHECK( fi.depth != pixel_depth::UNKNOWN );                                      \
        CHECK( fi.channels * depth_bytes( fi.depth ) == fi.bytes_per_pixel );            \
        CHECK( fi.name != "unknown" );                                                  \
        CHECK( format_for_v<T> == f );                                                   \
    } while ( false )

}

TEST_CASE("pixel_format-info-is-consistent-with-pixel-types")
{
    OPENPIV_CHECK_FORMAT( GREY_U8 );
    OPENPIV_CHECK_FORMAT( GREY_U16 );
    OPENPIV_CHECK_FORMAT( GREY_U32 );
    OPENPIV_CHECK_FORMAT( GREY_F32 );
    OPENPIV_CHECK_FORMAT( GREY_F64 );
    OPENPIV_CHECK_FORMAT( RGB_U8 );
    OPENPIV_CHECK_FORMAT( RGB_U16 );
    OPENPIV_CHECK_FORMAT( RGB_U32 );
    OPENPIV_CHECK_FORMAT( RGB_F32 );
    OPENPIV_CHECK_FORMAT( RGB_F64 );
    OPENPIV_CHECK_FORMAT( RGBA_U8 );
    OPENPIV_CHECK_FORMAT( RGBA_U16 );
    OPENPIV_CHECK_FORMAT( RGBA_U32 );
    OPENPIV_CHECK_FORMAT( RGBA_F32 );
    OPENPIV_CHECK_FORMAT( RGBA_F64 );
    OPENPIV_CHECK_FORMAT( YUV_U8 );
    OPENPIV_CHECK_FORMAT( YUV_U16 );
    OPENPIV_CHECK_FORMAT( YUV_U32 );
    OPENPIV_CHECK_FORMAT( YUV_F32 );
    OPENPIV_CHECK_FORMAT( YUV_F64 );
    OPENPIV_CHECK_FORMAT( YUVA_U8 );
    OPENPIV_CHECK_FORMAT( YUVA_U16 );
    OPENPIV_CHECK_FORMAT( YUVA_U32 );
    OPENPIV_CHECK_FORMAT( YUVA_F32 );
    OPENPIV_CHECK_FORMAT( YUVA_F64 );
    OPENPIV_CHECK_FORMAT( COMPLEX_U8 );
    OPENPIV_CHECK_FORMAT( COMPLEX_U16 );
    OPENPIV_CHECK_FORMAT( COMPLEX_U32 );
    OPENPIV_CHECK_FORMAT( COMPLEX_F32 );
    OPENPIV_CHECK_FORMAT( COMPLEX_F64 );
}

TEST_CASE("pixel_format-stats")
{
    // only grey formats are mono; only complex formats are complex
    for ( pixel_format f : all_formats )
    {
        const pixel_format_info& fi{ format_info( f ) };
        CHECK( fi.is_mono == ( fi.family == pixel_family::GREY ) );
        CHECK( fi.is_complex == ( fi.family == pixel_family::COMPLEX ) );
    }

    // selected names
    CHECK( format_info( pixel_format::GREY_U8 ).name == "grey_u8" );
    CHECK( format_info( pixel_format::RGB_U16 ).name == "rgb_u16" );
    CHECK( format_info( pixel_format::RGBA_F32 ).name == "rgba_f32" );
    CHECK( format_info( pixel_format::YUV_U8 ).name == "yuv_u8" );
    CHECK( format_info( pixel_format::YUVA_F64 ).name == "yuva_f64" );
    CHECK( format_info( pixel_format::COMPLEX_F32 ).name == "complex_f32" );

    // unknown format
    const pixel_format_info& fi{ format_info( pixel_format::UNKNOWN ) };
    CHECK( fi.name == "unknown" );
    CHECK( fi.family == pixel_family::UNKNOWN );
    CHECK( fi.depth == pixel_depth::UNKNOWN );
    CHECK( fi.channels == 0 );
    CHECK( fi.bytes_per_pixel == 0 );
    CHECK( !fi.is_mono );
    CHECK( !fi.is_complex );

    // out of range values
    const pixel_format bogus{ static_cast<pixel_format>( 200 ) };
    CHECK( format_info( bogus ).name == "unknown" );

    // channel counts
    CHECK( format_info( pixel_format::GREY_U8 ).channels == 1 );
    CHECK( format_info( pixel_format::RGB_U8 ).channels == 3 );
    CHECK( format_info( pixel_format::RGBA_U8 ).channels == 4 );
    CHECK( format_info( pixel_format::YUV_U8 ).channels == 3 );
    CHECK( format_info( pixel_format::YUVA_U8 ).channels == 4 );
    CHECK( format_info( pixel_format::COMPLEX_F32 ).channels == 2 );

    // bytes per pixel for a spread of formats; note that u32 and
    // f32 share the same depth size - identity of format is carried
    // by the enum, not the size
    CHECK( format_info( pixel_format::GREY_U8 ).bytes_per_pixel == 1 );
    CHECK( format_info( pixel_format::GREY_F32 ).bytes_per_pixel == 4 );
    CHECK( format_info( pixel_format::GREY_U32 ).bytes_per_pixel == 4 );
    CHECK( format_info( pixel_format::RGB_U8 ).bytes_per_pixel == 3 );
    CHECK( format_info( pixel_format::RGB_U16 ).bytes_per_pixel == 6 );
    CHECK( format_info( pixel_format::RGBA_U8 ).bytes_per_pixel == 4 );
    CHECK( format_info( pixel_format::RGBA_F64 ).bytes_per_pixel == 32 );
    CHECK( format_info( pixel_format::COMPLEX_U8 ).bytes_per_pixel == 2 );
    CHECK( format_info( pixel_format::COMPLEX_F64 ).bytes_per_pixel == 16 );

    // all unknown types map to unknown format
    CHECK( format_for_v<int> == pixel_format::UNKNOWN );
    CHECK( format_for_v<double> == pixel_format::UNKNOWN );
    CHECK( format_for_v<void> == pixel_format::UNKNOWN );
}

TEST_CASE("pixel_format-to-ostream")
{
    std::ostringstream oss;
    oss << pixel_format::GREY_U8;
    CHECK( oss.str() == "grey_u8" );
    CHECK( oss.str() == std::string( format_info( pixel_format::GREY_U8 ).name ) );
}
