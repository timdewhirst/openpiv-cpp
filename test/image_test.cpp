#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>

// std
#include <cstdint>
#include <format>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// to be tested
#include "core/image.h"

using namespace openpiv;
using namespace openpiv::core;

namespace {

    /// allocator that records allocations into a pool;
    /// note that std::function copies the callable, so the pool is
    /// referenced through a shared_ptr rather than held directly
    struct pool_allocator_t
    {
        std::shared_ptr<std::vector<std::shared_ptr<std::vector<uint8_t>>>> pool =
            std::make_shared<std::vector<std::shared_ptr<std::vector<uint8_t>>>>();

        image::allocator_t operator()()
        {
            const auto& p = pool;
            return [ p ]( size_t bytes ) -> std::shared_ptr<uint8_t>
            {
                auto storage = std::make_shared<std::vector<uint8_t>>( bytes, uint8_t{0} );
                p->push_back( storage );
                return { storage, storage->data() };
            };
        }
    };

}

TEST_CASE("image-default-construction")
{
    image im;
    CHECK( im.format() == pixel_format::UNKNOWN );
    CHECK( im.width() == 0 );
    CHECK( im.height() == 0 );
    CHECK( im.pixel_count() == 0 );
    CHECK( im.data() == nullptr );
    CHECK( im.pitch() == 0 );

    // copy of empty image shares nothing but is equal
    const image im2{ im };
    CHECK( im2 == im );
    CHECK( im == image{} );

    // empty images throw on access
    REQUIRE_THROWS_AS( im.at<g_8>( 0 ), std::runtime_error );
    REQUIRE_THROWS_AS( im.line( 0 ), std::runtime_error );
    REQUIRE_THROWS_AS( im.sub_image( core::rect::from_size( {4, 4}) ), std::runtime_error );

    // unknown format is rejected
    REQUIRE_THROWS_AS( image( 4, 4, pixel_format::UNKNOWN ), std::invalid_argument );
}

TEST_CASE("image-construction-with-format")
{
    const image im{ 7, 5, pixel_format::GREY_U8 };
    CHECK( im.width() == 7 );
    CHECK( im.height() == 5 );
    CHECK( im.size() == core::size( 7, 5 ) );
    CHECK( im.pixel_count() == 35 );
    CHECK( im.format() == pixel_format::GREY_U8 );
    CHECK( im.rect().bottomLeft() == core::rect::point_t( 0, 0 ) );
    CHECK( im.pitch() == 7 );

    const auto [bpp, pitch] = im.stride();
    CHECK( bpp == 1 );
    CHECK( pitch == 7 );

    // storage is zero-initialized
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        CHECK( im.at<g_8>( i ) == g_8( 0 ) );

    // width drives the pitch
    const image wide{ 3, 4, pixel_format::RGBA_U8 };
    CHECK( wide.pitch() == 12 );
    CHECK( wide.info().bytes_per_pixel == 4 );

    // size-based/rect-based construction
    const image a{ core::size{ 4, 4 }, pixel_format::GREY_U16 };
    CHECK( a.pitch() == 8 );
    const image b{ core::rect::from_size( { 4, 6 } ), pixel_format::RGB_U8 };
    CHECK( b.pitch() == 12 );
}

TEST_CASE("image-external-allocation")
{
    pool_allocator_t pool;
    const image::allocator_t allocator{ pool() };

    const image im{ 5, 5, pixel_format::GREY_U16, allocator };
    REQUIRE( pool.pool->size() == 1 );
    CHECK( (*pool.pool)[0]->size() == 50 ); // 5*5*2

    // shallow copy: no new allocation
    const image alias{ im };
    CHECK( pool.pool->size() == 1 );

    // extract allocates anew
    const image sub{ im.extract( core::rect::from_size( {2, 2} ), allocator ) };
    CHECK( pool.pool->size() == 2 );

    // default allocator is independent
    const image plain{ 2, 2, pixel_format::GREY_U8 };
    CHECK( pool.pool->size() == 2 );
}

TEST_CASE("image-typed-access")
{
    image im{ 6, 3, pixel_format::RGBA_U8 };

    // linear and point access agree
    im.at<rgba_8>( 0 ) = rgba_8{ 1, 2, 3, 4 };
    im.at<rgba_8>( core::point2<uint32_t>( 1, 0 ) ) = rgba_8{ 5, 6, 7, 8 };
    im.at<rgba_8>( core::point2<uint32_t>( 0, 1 ) ) = rgba_8{ 9, 10, 11, 12 };
    CHECK( im.at<rgba_8>( 0 ) == rgba_8( 1, 2, 3, 4 ) );
    CHECK( im.at<rgba_8>( 1 ) == rgba_8( 5, 6, 7, 8 ) );
    CHECK( im.at<rgba_8>( 6 ) == rgba_8( 9, 10, 11, 12 ) );

    // pixels() gives raw packed access
    rgba_8* pixels = im.pixels<rgba_8>();
    CHECK( pixels[6] == rgba_8( 9, 10, 11, 12 ) );

    // line() gives per-row byte access
    uint8_t* row = im.line( 1 );
    CHECK( row[0] == 9 );
    CHECK( row[1] == 10 );
    CHECK( im.line( 2 ) == row + im.pitch() );
    CHECK( const_cast<const image&>( im ).line( 1 ) == row );

    // bounds are checked
    REQUIRE_THROWS_AS( im.at<rgba_8>( 18 ), std::out_of_range );
    REQUIRE_THROWS_AS( im.at<rgba_8>( core::point2<uint32_t>( 6, 0 ) ), std::out_of_range );
    REQUIRE_THROWS_AS( im.at<rgba_8>( core::point2<uint32_t>( 0, 3 ) ), std::out_of_range );
    REQUIRE_THROWS_AS( im.line( 4 ), std::range_error );

    // pixel type is checked against the format: rgb<->rgba have the
    // same per-pixel size but are distinct formats
    REQUIRE_THROWS_AS( im.at<rgb_8>( 0 ), std::runtime_error );
    REQUIRE_THROWS_AS( im.pixels<yuv_16>(), std::runtime_error );

    // same size, different layout: yuva vs rgba (both 4 channels)
    image yuva_im{ 2, 2, pixel_format::YUVA_U8 };
    REQUIRE_THROWS_AS( yuva_im.at<rgba_8>( 0 ), std::runtime_error );

    // same size, different depth: u32 vs f32 (both 4 bytes)
    image u32_im{ 2, 2, pixel_format::GREY_U32 };
    REQUIRE_THROWS_AS( u32_im.at<g_f32>( 0 ), std::runtime_error );
}

TEST_CASE("image-sub-image")
{
    image im{ 10, 10, pixel_format::GREY_U8 };
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        im.at<g_8>( i ) = g_8( static_cast<uint8_t>( i ) );

    // sub-image at (3, 2) size (4, 5)
    const core::rect local{ { 3, 2 }, core::size{ 4, 5 } };
    image sub = im.sub_image( local );

    CHECK( sub.width() == 4 );
    CHECK( sub.height() == 5 );
    // rect is positioned in the parent's space
    CHECK( sub.rect().bottomLeft() == core::rect::point_t( 3, 2 ) );
    // pitch is inherited from the parent
    CHECK( sub.pitch() == 10 );
    CHECK( std::get<1>( sub.stride() ) == 10 );

    // mapping is correct
    for ( image::index_t i = 0; i < sub.pixel_count(); ++i )
    {
        const uint32_t x = static_cast<uint32_t>( i ) % sub.width();
        const uint32_t y = static_cast<uint32_t>( i ) / sub.width();
        CHECK( sub.at<g_8>( i ) == im.at<g_8>( core::point2<uint32_t>( 3 + x, 2 + y ) ) );
    }

    // data is shared: write through sub-image, see in parent
    sub.at<g_8>( 0 ) = g_8( 255 );
    CHECK( im.at<g_8>( core::point2<uint32_t>( 3, 2 ) ) == g_8( 255 ) );

    // sub-image of a sub-image
    image subsub = sub.sub_image( core::rect{ { 1, 1 }, core::size{ 2, 2 } } );
    CHECK( subsub.rect().bottomLeft() == core::rect::point_t( 4, 3 ) );
    CHECK( subsub.pitch() == 10 );
    CHECK( subsub.at<g_8>( 0 ) == im.at<g_8>( core::point2<uint32_t>( 4, 3 ) ) );
    subsub.at<g_8>( 0 ) = g_8( 128 );
    CHECK( im.at<g_8>( core::point2<uint32_t>( 4, 3 ) ) == g_8( 128 ) );

    // bounds are enforced
    REQUIRE_THROWS_AS( im.sub_image( core::rect{ { 7, 7 }, core::size{ 4, 4 } } ), std::out_of_range );
    REQUIRE_THROWS_AS( im.sub_image( core::rect{ { uint32_t(0) - 3, 0 }, core::size{ 4, 4 } } ), std::out_of_range );
}

TEST_CASE("image-shallow-copy-semantics")
{
    image im{ 3, 3, pixel_format::GREY_U8 };
    image alias{ im };
    alias.at<g_8>( 4 ) = g_8( 42 );
    CHECK( im.at<g_8>( 4 ) == g_8( 42 ) );
    CHECK( im == alias ); // same geometry, format and content
}

TEST_CASE("image-extract-and-clone")
{
    image im{ 10, 10, pixel_format::GREY_U8 };
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        im.at<g_8>( i ) = g_8( static_cast<uint8_t>( i ) );

    const core::rect local{ { 4, 4 }, core::size{ 3, 2 } };
    image sub = im.extract( local );

    // fresh storage with the same content
    CHECK( sub != image{ 3, 2, pixel_format::GREY_U8 } ); // content differs from zero-image
    CHECK( sub.rect().bottomLeft() == core::rect::point_t( 4, 4 ) );
    CHECK( sub.pitch() == 3 ); // fresh image has its own tight pitch
    for ( image::index_t i = 0; i < sub.pixel_count(); ++i )
    {
        const uint32_t x = static_cast<uint32_t>( i ) % sub.width();
        const uint32_t y = static_cast<uint32_t>( i ) / sub.width();
        CHECK( sub.at<g_8>( i ) == im.at<g_8>( core::point2<uint32_t>( 4 + x, 4 + y ) ) );
    }

    // modifying the source afterwards does not affect the extract
    im.at<g_8>( core::point2<uint32_t>( 4, 4 ) ) = g_8( 255 );
    // the parent image was filled with its linear index; the first
    // pixel of the extract is linear index 4*10+4 in the parent
    CHECK( sub.at<g_8>( 0 ) == g_8( static_cast<uint8_t>( 4 * 10 + 4 ) ) );

    // clone is a positioned deep copy
    image c = im.clone();
    CHECK( c.rect() == im.rect() );
    CHECK( c.pitch() == im.pitch() );
    CHECK( c == im );
    im.at<g_8>( 0 ) = g_8( 1 );
    CHECK( c.at<g_8>( 0 ) != g_8( 1 ) );
    CHECK( !( c == im ) );
    CHECK( c != im );

    // extracted rect of a sub-image keeps its global position
    image subview = im.sub_image( core::rect{ { 2, 2 }, core::size{ 5, 5 } } );
    image extracted = subview.extract( core::rect{ { 1, 1 }, core::size{ 2, 2 } } );
    CHECK( extracted.rect().bottomLeft() == core::rect::point_t( 3, 3 ) );
    CHECK( extracted.at<g_8>( 0 ) == im.at<g_8>( core::point2<uint32_t>( 3, 3 ) ) );

    // bounds are enforced
    REQUIRE_THROWS_AS( im.extract( core::rect{ { 8, 8 }, core::size{ 4, 4 } } ), std::out_of_range );
}

TEST_CASE("image-equality")
{
    image a{ 4, 4, pixel_format::GREY_U8 };
    image b{ 4, 4, pixel_format::GREY_U8 };

    CHECK( a == b ); // independent allocations, equal content

    b.at<g_8>( 5 ) = g_8( 1 );
    CHECK( a != b );
    a.at<g_8>( 5 ) = g_8( 1 );
    CHECK( a == b );

    // different geometry
    CHECK( a != image( 5, 4, pixel_format::GREY_U8 ) );
    CHECK( a != image( 4, 5, pixel_format::GREY_U8 ) );
    CHECK( a != image( 4, 4, pixel_format::GREY_U16 ) );

    // a sub-image of a uniform image is equal to a tight image of
    // the same content (pitch differs, content matches)
    image wide{ 8, 8, pixel_format::GREY_U8 };
    image tight{ 4, 8, pixel_format::GREY_U8 };
    image wide_sub = wide.sub_image( core::rect{ { 0, 0 }, core::size{ 4, 8 } } );
    CHECK( wide_sub.pitch() != tight.pitch() );
    CHECK( wide_sub == tight );
}

TEST_CASE("image-convert")
{
    // same-format conversion is a positioned deep copy
    image grey{ 4, 2, pixel_format::GREY_U8 };
    for ( image::index_t i = 0; i < grey.pixel_count(); ++i )
        grey.at<g_8>( i ) = g_8( static_cast<uint8_t>( i * 10 ) );

    image same = convert( grey, pixel_format::GREY_U8 );
    CHECK( same.format() == pixel_format::GREY_U8 );
    CHECK( same == grey );
    same.at<g_8>( 0 ) = g_8( 99 );
    CHECK( grey.at<g_8>( 0 ) == g_8( 0 ) );

    // depth conversion
    image wider = convert( grey, pixel_format::GREY_F64 );
    CHECK( wider.format() == pixel_format::GREY_F64 );
    CHECK( wider.at<g_f>( 1 ) == g_f( 10.0 ) );

    // rgb -> grey: uniform colour maps to its own intensity
    image rgb{ 3, 1, pixel_format::RGB_U8 };
    rgb.at<rgb_8>( 0 ) = rgb_8( 100, 100, 100 );
    rgb.at<rgb_8>( 1 ) = rgb_8( 0, 0, 0 );
    rgb.at<rgb_8>( 2 ) = rgb_8( 255, 255, 255 );
    image grey2 = convert( rgb, pixel_format::GREY_U8 );
    CHECK( grey2.at<g_8>( 0 ) == g_8( 100 ) );
    CHECK( grey2.at<g_8>( 1 ) == g_8( 0 ) );
    CHECK( grey2.at<g_8>( 2 ) == g_8( 255 ) );

    // yuv -> grey keeps y
    image yuv_im{ 2, 1, pixel_format::YUV_U8 };
    yuv_im.at<yuv_8>( 0 ) = yuv_8( 33, 1, 2 );
    yuv_im.at<yuv_8>( 1 ) = yuv_8( 77, 0, 0 );
    image grey3 = convert( yuv_im, pixel_format::GREY_U8 );
    CHECK( grey3.at<g_8>( 0 ) == g_8( 33 ) );
    CHECK( grey3.at<g_8>( 1 ) == g_8( 77 ) );

    // grey -> rgba
    image rgba = convert( grey, pixel_format::RGBA_U8 );
    CHECK( rgba.at<rgba_8>( 0 ).r == 0 );
    CHECK( rgba.at<rgba_8>( 0 ).a == 255 );

    // complex -> grey uses magnitude
    image cmplx{ 2, 1, pixel_format::COMPLEX_F64 };
    cmplx.at<c_f>( 0 ) = c_f( 3.0, 4.0 );
    cmplx.at<c_f>( 1 ) = c_f( -5.0, 0.0 );
    image grey4 = convert( cmplx, pixel_format::GREY_F64 );
    CHECK( grey4.at<g_f>( 0 ) == g_f( 5.0 ) );
    CHECK( grey4.at<g_f>( 1 ) == g_f( 5.0 ) );

    // unsupported conversion is a runtime error
    REQUIRE_THROWS_AS( convert( cmplx, pixel_format::RGB_U8 ), std::runtime_error );

    // bad requests
    REQUIRE_THROWS_AS( convert( grey, pixel_format::UNKNOWN ), std::invalid_argument );
    REQUIRE_THROWS_AS( convert( image{}, pixel_format::GREY_U8 ), std::runtime_error );
}

TEST_CASE("image-swap-and-stream")
{
    image a{ 2, 2, pixel_format::GREY_U8 };
    image b{ 3, 3, pixel_format::GREY_U16 };
    a.at<g_8>( 0 ) = g_8( 7 );

    swap( a, b );
    CHECK( a.format() == pixel_format::GREY_U16 );
    CHECK( b.format() == pixel_format::GREY_U8 );
    CHECK( b.at<g_8>( 0 ) == g_8( 7 ) );

    std::ostringstream oss;
    oss << b;
    const std::string description{ oss.str() };
    CHECK( description.find( "image[grey_u8]" ) == 0 );
    CHECK( description.find( "pitch: 2" ) != std::string::npos );

    // std::format goes through the same path
    const std::string formatted{ std::format( "{}", b ) };
    CHECK( formatted == description );
}
