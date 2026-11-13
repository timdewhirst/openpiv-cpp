#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>
#include "doctest_utils.h"

// std
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

// local
#include "test_utils.h"

// to be tested
#include "core/image_utils.h"

using namespace std::string_literals;
using namespace openpiv;
using namespace openpiv::core;

TEST_CASE("image_loader_test - tiff_loader_mono")
{
    std::ifstream is("test-mono.tiff", std::ios::binary);
    REQUIRE(is.is_open());

    std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(is) };
    REQUIRE(!!loader);
    REQUIRE(loader->name() == std::string("image/tiff") );

    auto maybe{ loader->load( is ) };
    REQUIRE( !!maybe );
    image im{ std::move( *maybe ) };

    REQUIRE(im.width() == 511);
    REQUIRE(im.height() == 369);
    // the image format is determined by the file; this is an
    // 8-bit greyscale tiff
    REQUIRE(im.format() == pixel_format::GREY_U8);
}

TEST_CASE("image_loader_test - tiff_loader_rgb")
{
    std::ifstream is("test-rgb.tiff", std::ios::binary);
    REQUIRE(is.is_open());

    std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(is) };
    REQUIRE(!!loader);
    REQUIRE(loader->name() == std::string("image/tiff"));

    auto maybe{ loader->load( is ) };
    REQUIRE( !!maybe );
    image im{ std::move( *maybe ) };

    REQUIRE(im.width() == 511);
    REQUIRE(im.height() == 369);
    REQUIRE(im.format() == pixel_format::RGB_U8);
}

TEST_CASE("image_loader_test - pnm_loader_load_p5")
{
    std::stringstream ss;
    ss << "P5\n"
       << "# test\n"
       << "# second comment line\n"
       << "300 200 255\n";
    // write 300x200 bytes
    std::vector<int8_t> data( 300 * 200 );
    std::generate(data.begin(), data.end(), [](){ return rand() % 256; });
    ss.write(reinterpret_cast<const char*>(&data[0]), data.size());

    std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(ss) };
    REQUIRE(!!loader);

    auto maybe{ loader->load( ss ) };
    REQUIRE( !!maybe );
    image im{ std::move( *maybe ) };

    REQUIRE(im.width() == 300);
    REQUIRE(im.height() == 200);
    REQUIRE(im.format() == pixel_format::GREY_U8);

    // check content
    bool matched{ true };
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        if ( im.at<g_8>( i ) != g_8( static_cast<uint8_t>( data[i] ) ) )
            matched = false;
    CHECK( matched );
}

TEST_CASE("image_loader_test - pnm_loader_load_p6")
{
    std::stringstream ss;
    ss << "P6\n"
       << "# test\n"
       << "100 50 255\n";
    std::vector<uint8_t> data( 100 * 50 * 3 );
    std::generate(data.begin(), data.end(), [](){ return rand() % 256; });
    ss.write(reinterpret_cast<const char*>( data.data() ), data.size());

    std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(ss) };
    REQUIRE(!!loader);

    auto maybe{ loader->load( ss ) };
    REQUIRE( !!maybe );
    image im{ std::move( *maybe ) };

    REQUIRE(im.width() == 100);
    REQUIRE(im.height() == 50);
    REQUIRE(im.format() == pixel_format::RGB_U8);

    bool matched{ true };
    for ( uint32_t h = 0; h < im.height(); ++h )
    {
        const rgb_8* p{ reinterpret_cast<const rgb_8*>( im.line( h ) ) };
        for ( uint32_t w = 0; w < im.width(); ++w, ++p )
        {
            const size_t index{ 3 * ( h * im.width() + w ) };
            if ( p->r != data[index] || p->g != data[index+1] || p->b != data[index+2] )
                matched = false;
        }
    }
    CHECK( matched );
}

TEST_CASE("image_loader_test - pnm_save_unsupported_format")
{
    std::shared_ptr<image_loader> writer{ image_loader_registry::instance().find("image/x-portable-anymap") };
    REQUIRE(!!writer);

    // unsupported format is a runtime error
    image im{ 4, 4, pixel_format::YUV_U8 };
    std::stringstream ss;
    REQUIRE_THROWS_AS( writer->save( ss, im ), image_loader_exception );
}

TEST_CASE("image_loader_test - pnm_save_reload_p5")
{
    image im{ 64, 32, pixel_format::GREY_U8 };
    fill( im, []( uint32_t w, uint32_t h ) { return (w * 8 + h) % 256; } );

    std::shared_ptr<image_loader> writer{ image_loader_registry::instance().find("image/x-portable-anymap") };
    REQUIRE(!!writer);

    std::stringstream ss( std::ios_base::in | std::ios_base::out | std::ios_base::binary );
    writer->save( ss, im );

    // read back; PNM stores 16-bit data so comes back as GREY_U16
    std::shared_ptr<image_loader> reader{ image_loader_registry::instance().find(ss) };
    REQUIRE(!!reader);
    auto maybe{ reader->load( ss ) };
    REQUIRE( !!maybe );
    image rt{ std::move( *maybe ) };
    REQUIRE(rt.format() == pixel_format::GREY_U16);
    CHECK( rt == convert( im, pixel_format::GREY_U16 ) );
}

TEST_CASE("image_loader_test - pnm_save_reload_p6")
{
    image im{ 48, 16, pixel_format::RGBA_U8 };
    fill( im, []( uint32_t w, uint32_t h ) { return (w + h) % 128; } );

    std::shared_ptr<image_loader> writer{ image_loader_registry::instance().find("image/x-portable-anymap") };
    REQUIRE(!!writer);

    std::stringstream ss( std::ios_base::in | std::ios_base::out | std::ios_base::binary );
    writer->save( ss, im );

    // read back; alpha is dropped by the PNM file format
    std::shared_ptr<image_loader> reader{ image_loader_registry::instance().find(ss) };
    REQUIRE(!!reader);
    auto maybe{ reader->load( ss ) };
    REQUIRE( !!maybe );
    image rt{ std::move( *maybe ) };
    REQUIRE(rt.format() == pixel_format::RGB_U16);

    bool matched{ true };
    for ( uint32_t h = 0; h < im.height(); ++h )
    {
        const rgba_8* p{ reinterpret_cast<const rgba_8*>( im.line( h ) ) };
        const rgb_16* q{ reinterpret_cast<const rgb_16*>( rt.line( h ) ) };
        for ( uint32_t w = 0; w < im.width(); ++w, ++p, ++q )
            if ( p->r != q->r || p->g != q->g || p->b != q->b )
                matched = false;
    }
    CHECK( matched );
}

TEST_CASE("image_loader_test - pnm_loader_save_files")
{
    // load a tiff and save as pnm files
    {
        std::ifstream is("test-mono.tiff", std::ios::binary);
        REQUIRE(is.is_open());
        std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(is) };
        REQUIRE(!!loader);
        auto maybe{ loader->load( is ) };
        REQUIRE( !!maybe );
        image im{ std::move( *maybe ) };
        REQUIRE( save_to_file( "test-mono.pgm", im ) );
    }

    {
        std::ifstream is("test-rgb.tiff", std::ios::binary);
        REQUIRE(is.is_open());
        std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(is) };
        REQUIRE(!!loader);
        auto maybe{ loader->load( is ) };
        REQUIRE( !!maybe );
        image im{ std::move( *maybe ) };
        REQUIRE( save_to_file( "test-rgb.ppm", im ) );
    }
}
