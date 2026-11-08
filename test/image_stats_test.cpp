#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>
#include "doctest_utils.h"

// local
#include "test_utils.h"

// to be tested
#include "core/image_utils.h"
#include "algos/stats.h"

using namespace openpiv;
using namespace openpiv::core;
using namespace openpiv::algos;

TEST_CASE("image_stats_test - find_image_range_uniform_test")
{
    image im{ 200, 200, pixel_format::GREY_U8 };
    fill( im, 0.0 );

    auto [min, max] = find_image_range( im );
    REQUIRE(min == 0);
    REQUIRE(max == 0);
}

TEST_CASE("image_stats_test - find_image_range_max_test")
{
    image im{ 200, 200, pixel_format::GREY_U8 };
    fill( im, 0.0 );

    im.at<g_8>( point2<uint32_t>( 50, 50 ) ) = g_8( 255 );
    auto [min, max] = find_image_range( im );
    REQUIRE(min == 0);
    REQUIRE(max == 255);
}

TEST_CASE("image_stats_test - find_image_range_min_test")
{
    image im{ 200, 200, pixel_format::GREY_U8 };
    fill( im, 255.0 );

    im.at<g_8>( point2<uint32_t>( 50, 50 ) ) = g_8( 0 );
    auto [min, max] = find_image_range( im );
    REQUIRE(min == 0);
    REQUIRE(max == 255);
}

TEST_CASE("image_stats_test - find_image_range_min_max_test")
{
    image im{ 200, 200, pixel_format::GREY_U8 };
    fill( im, 128.0 );

    im.at<g_8>( point2<uint32_t>( 50, 50 ) ) = g_8( 0 );
    im.at<g_8>( point2<uint32_t>( 150, 150 ) ) = g_8( 255 );
    auto [min, max] = find_image_range( im );
    REQUIRE(min == 0);
    REQUIRE(max == 255);
}

TEST_CASE("image_stats_test - find_image_range_sub_image")
{
    image im{ 200, 200, pixel_format::GREY_F64 };
    fill( im, 10.0 );

    image sub = im.sub_image( rect{ { 10, 10 }, size{ 20, 20 } } );
    fill( sub, 20.0 );

    auto [min, max] = find_image_range( sub );
    REQUIRE(min == 20);
    REQUIRE(max == 20);
}

TEST_CASE("image_stats_test - find_image_range_requires_grey")
{
    image im{ 8, 8, pixel_format::RGBA_U8 };
    REQUIRE_THROWS_AS( find_image_range( im ), std::runtime_error );
}
