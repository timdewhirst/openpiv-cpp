#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>
#include "doctest_utils.h"

// std
#include <cstdint>
#include <numeric>
#include <sstream>

// to be tested
#include "core/image_utils.h"

using namespace openpiv;
using namespace openpiv::core;

TEST_CASE("image_utils_test - fill")
{
    image im{ 10, 10, pixel_format::GREY_U8 };
    fill( im, 42.0 );
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        REQUIRE( im.at<g_8>( i ) == g_8( 42 ) );

    // floating point fill truncates through the pixel type
    image imf{ 4, 4, pixel_format::GREY_F32 };
    fill( imf, 1.5 );
    for ( image::index_t i = 0; i < imf.pixel_count(); ++i )
        REQUIRE( imf.at<g_f32>( i ) == g_f32( 1.5f ) );

    // colour images are filled with a "grey" value of the family
    image rgba{ 4, 4, pixel_format::RGBA_U8 };
    fill( rgba, 77.0 );
    for ( image::index_t i = 0; i < rgba.pixel_count(); ++i )
    {
        auto& p = rgba.at<rgba_8>( i );
        REQUIRE( p.r == 77 );
        REQUIRE( p.g == 77 );
        REQUIRE( p.b == 77 );
        REQUIRE( p.a == 255 );
    }
}

TEST_CASE("image_utils_test - fill_generator")
{
    image im{ 8, 6, pixel_format::GREY_F64 };
    fill( im, []( uint32_t w, uint32_t h ) { return w + 10.0*h; } );

    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
    {
        const uint32_t w = static_cast<uint32_t>( i ) % im.width();
        const uint32_t h = static_cast<uint32_t>( i ) / im.width();
        REQUIRE( im.at<g_f>( i ) == g_f( w + 10.0*h ) );
    }

    // generator fill on a sub-image
    image sub = im.sub_image( rect{ { 2, 2 }, size{ 4, 2 } } );
    fill( sub, []( uint32_t, uint32_t ){ return -1.0; } );
    for ( image::index_t i = 0; i < sub.pixel_count(); ++i )
        REQUIRE( sub.at<g_f>( i ) == g_f( -1.0 ) );
}

TEST_CASE("image_utils_test - apply")
{
    image im{ 4, 4, pixel_format::GREY_U8 };

    // apply must be generic over pixel types; the lambda compiles
    // for every format the image could be
    apply( im, []( image::index_t i, auto& v ){
            using P = std::decay_t<decltype(v)>;
            using V = typename P::value_t;
            return P( static_cast<V>( i ) );
        } );

    // check result through the grey accessor
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        REQUIRE( im.at<g_8>( i ) == g_8( static_cast<uint8_t>( i ) ) );
}

TEST_CASE("image_utils_test - pixel_sum")
{
    image im{ 10, 10, pixel_format::GREY_U8 };
    fill( im, []( uint32_t w, uint32_t h ) { return w + h; } );
    // trivially: sum_{0..9}(w+h) per row = 45*rows + 10*sum(h)
    REQUIRE( pixel_sum( im ) == 45*10 + 10*45 );

    image imf{ 4, 4, pixel_format::GREY_F64 };
    fill( imf, 0.5 );
    REQUIRE( doctest::matchers::WithinAbs( pixel_sum( imf ), 8.0, 1e-12 ) );

    // non-grey images are rejected
    REQUIRE_THROWS_AS( pixel_sum( image{ 4, 4, pixel_format::RGBA_U8 } ), std::runtime_error );
}

TEST_CASE("image_utils_test - transpose")
{
    image im{ 7, 3, pixel_format::GREY_U8 };
    fill( im, []( uint32_t w, uint32_t h ) { return w * 100 + h; } );

    image t{ transpose( im ) };
    REQUIRE( t.width() == 3 );
    REQUIRE( t.height() == 7 );
    REQUIRE( t.format() == im.format() );

    for ( image::index_t i = 0; i < t.pixel_count(); ++i )
    {
        const uint32_t w = static_cast<uint32_t>( i ) % t.width();
        const uint32_t h = static_cast<uint32_t>( i ) / t.width();
        REQUIRE( t.at<g_8>( i ) == im.at<g_8>( point2<uint32_t>( h, w ) ) );
    }

    // two-stage transpose into a provided output
    image out{ 3, 7, pixel_format::GREY_U8 };
    copy_transpose( im, out );
    REQUIRE( out == t );

    // mismatched formats/dimensions are errors
    image wrong_format{ 7, 3, pixel_format::GREY_U16 };
    image wrong_dims{ 7, 3, pixel_format::GREY_U8 };
    REQUIRE_THROWS_AS( copy_transpose( im, wrong_format ), std::runtime_error );
    REQUIRE_THROWS_AS( copy_transpose( im, wrong_dims ), std::runtime_error );

    // transpose of a sub-image
    image sub = im.sub_image( rect{ { 2, 1 }, size{ 3, 2 } } );
    image strt{ transpose( sub ) };
    REQUIRE( strt.width() == 2 );
    REQUIRE( strt.height() == 3 );
    for ( image::index_t i = 0; i < strt.pixel_count(); ++i )
    {
        const uint32_t w = static_cast<uint32_t>( i ) % strt.width();
        const uint32_t h = static_cast<uint32_t>( i ) / strt.width();
        REQUIRE( strt.at<g_8>( i ) == sub.at<g_8>( point2<uint32_t>( h, w ) ) );
    }
}

TEST_CASE("image_utils_test - swap_quadrants")
{
    image im{ 8, 8, pixel_format::GREY_U8 };
    fill( im, []( uint32_t, uint32_t h ) { return h; } );

    // fill each quadrant with a distinct value
    auto value_at = []( uint32_t w, uint32_t h, uint32_t dim )
        {
            const bool left{ w < dim/2 };
            const bool bottom{ h < dim/2 };
            return left ? ( bottom ? 1 : 2 ) : ( bottom ? 3 : 4 );
        };
    fill( im, [&value_at]( uint32_t w, uint32_t h ) { return value_at( w, h, 8 ); } );

    image original{ im.clone() };
    swap_quadrants( im );

    // after swapping: q1 <-> q3, q2 <-> q4 (using (0,0) as the
    // bottom-left corner)
    for ( uint32_t h = 0; h < 8; ++h )
        for ( uint32_t w = 0; w < 8; ++w )
        {
            REQUIRE( im.at<g_8>( point2<uint32_t>( w, h ) ) ==
                     original.at<g_8>( point2<uint32_t>( (w + 4) % 8, (h + 4) % 8 ) ) );
        }

    // double swap is identity
    swap_quadrants( im );
    REQUIRE( im == original );

    // odd dimensions are rejected
    image odd{ 3, 3, pixel_format::GREY_U8 };
    REQUIRE_THROWS_AS( swap_quadrants( odd ), std::runtime_error );
}

TEST_CASE("image_utils_test - find_peaks")
{
    image im{ 32, 32, pixel_format::GREY_F64 };
    fill( im, 0.0 );

    // craft a couple of peaks
    const point2<uint32_t> peak_a{ 5, 10 };
    const point2<uint32_t> peak_b{ 20, 25 };
    im.at<g_f>( peak_a ) = g_f( 10.0 );
    im.at<g_f>( peak_b ) = g_f( 100.0 );

    auto peaks = find_peaks( im, 5, 1 );
    REQUIRE( peaks.size() == 2 );

    // peaks are sorted by value, and share the source data
    REQUIRE( peaks[0].rect().bottomLeft() == rect::point_t( 19, 24 ) );
    REQUIRE( peaks[1].rect().bottomLeft() == rect::point_t( 4, 9 ) );

    im.at<g_f>( peak_b ) = g_f( 0.0 );
    REQUIRE( peaks[0].at<g_f>( point2<uint32_t>( 1, 1 ) ) == g_f( 0.0 ) );

    // num_peaks caps the result
    im.at<g_f>( peak_b ) = g_f( 100.0 );
    auto peaks_capped = find_peaks( im, 1, 1 );
    REQUIRE( peaks_capped.size() == 1 );
    REQUIRE( peaks_capped[0].rect().bottomLeft() == rect::point_t( 19, 24 ) );

    // non-grey images are rejected
    REQUIRE_THROWS_AS( find_peaks( image{ 32, 32, pixel_format::RGBA_U8 }, 2, 1 ),
                       std::runtime_error );
    // too small images are rejected
    REQUIRE_THROWS_AS( find_peaks( image{ 3, 3, pixel_format::GREY_U8 }, 2, 2 ),
                       std::runtime_error );
}

TEST_CASE("image_utils_test - fit_simple_gaussian")
{
    // a gaussian-like peak with centre at (1, 1); the fit should
    // recover the (sub-pixel) centre
    image im{ 3, 3, pixel_format::GREY_F64 };
    const double values[] = { 0.5, 0.75, 0.5,
                              0.75, 1.0, 0.75,
                              0.5, 0.75, 0.5 };
    for ( uint32_t h = 0; h < 3; ++h )
        for ( uint32_t w = 0; w < 3; ++w )
            im.at<g_f>( point2<uint32_t>( w, h ) ) = g_f( values[ h*3 + w ] );

    const auto result = fit_simple_gaussian( im );
    // symmetric peak: centre is unchanged
    REQUIRE( doctest::matchers::WithinAbs( result[0], 1.0, 1e-9 ) );
    REQUIRE( doctest::matchers::WithinAbs( result[1], 1.0, 1e-9 ) );

    // an asymmetric peak shifts the centre
    image im2{ 3, 3, pixel_format::GREY_F64 };
    const double values2[] = { 0.25, 0.5, 0.75,
                               0.5, 0.75, 1.0,
                               0.25, 0.5, 0.75 };
    for ( uint32_t h = 0; h < 3; ++h )
        for ( uint32_t w = 0; w < 3; ++w )
            im2.at<g_f>( point2<uint32_t>( w, h ) ) = g_f( values2[ h*3 + w ] );

    const auto result2 = fit_simple_gaussian( im2 );
    REQUIRE( result2[0] > 1.0 );
    REQUIRE( doctest::matchers::WithinAbs( result2[1], 1.0, 1e-9 ) );

    REQUIRE_THROWS_AS( fit_simple_gaussian( image{ 4, 4, pixel_format::GREY_F64 } ),
                       std::runtime_error );
    REQUIRE_THROWS_AS( fit_simple_gaussian( image{ 3, 3, pixel_format::RGBA_U8 } ),
                       std::runtime_error );
}

TEST_CASE("image_utils_test - split_join_channels")
{
    image im{ 8, 6, pixel_format::RGBA_U16 };
    fill( im, []( uint32_t w, uint32_t h ) { return w + h; } );

    auto [r, g, b, a] = split_to_channels( im );
    REQUIRE( r.format() == pixel_format::GREY_U16 );
    REQUIRE( r.width() == 8 );
    REQUIRE( r.height() == 6 );
    for ( image::index_t i = 0; i < r.pixel_count(); ++i )
    {
        const auto& p = im.at<rgba_16>( i );
        REQUIRE( r.at<g_16>( i ) == g_16( p.r ) );
        REQUIRE( g.at<g_16>( i ) == g_16( p.g ) );
        REQUIRE( b.at<g_16>( i ) == g_16( p.b ) );
        REQUIRE( a.at<g_16>( i ) == g_16( p.a ) );
    }

    // joins round-trip
    image joined = join_from_channels( r, g, b, a );
    REQUIRE( joined.format() == pixel_format::RGBA_U16 );
    REQUIRE( joined == im );

    // mismatches are errors
    REQUIRE_THROWS_AS( join_from_channels( r, g, b, image{ 8, 6, pixel_format::GREY_U8 } ),
                       std::runtime_error );
    REQUIRE_THROWS_AS( join_from_channels( r, g, b, image{ 6, 8, pixel_format::GREY_U16 } ),
                       std::runtime_error );
    REQUIRE_THROWS_AS( split_to_channels( image{ 4, 4, pixel_format::GREY_U8 } ),
                       std::runtime_error );
}

TEST_CASE("image_utils_test - split_join_complex_channels")
{
    image im{ 8, 6, pixel_format::COMPLEX_F32 };
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        im.at<c_f32>( i ) = c_f32( static_cast<float>( i ), static_cast<float>( -i ) );

    auto [r, i] = split_complex_to_channels( im );
    REQUIRE( r.format() == pixel_format::GREY_F32 );
    for ( image::index_t j = 0; j < r.pixel_count(); ++j )
    {
        REQUIRE( r.at<g_f32>( j ) == g_f32( static_cast<float>( j ) ) );
        REQUIRE( i.at<g_f32>( j ) == g_f32( static_cast<float>( -j ) ) );
    }

    image joined = join_complex_from_channels( r, i );
    REQUIRE( joined.format() == pixel_format::COMPLEX_F32 );
    REQUIRE( joined == im );
}

TEST_CASE("image_utils_test - real")
{
    image im{ 4, 4, pixel_format::COMPLEX_F64 };
    for ( image::index_t i = 0; i < im.pixel_count(); ++i )
        im.at<c_f>( i ) = c_f( static_cast<double>( i*2 ), static_cast<double>( 5 - i ) );

    image re{ real( im ) };
    REQUIRE( re.format() == pixel_format::GREY_F64 );
    for ( image::index_t i = 0; i < re.pixel_count(); ++i )
        REQUIRE( re.at<g_f>( i ) == g_f( static_cast<double>( i*2 ) ) );

    // out-parameter form: extraction into any grey depth, with truncation
    image out_u8{ im.size(), pixel_format::GREY_U8 };
    copy_real( im, out_u8 );
    REQUIRE( out_u8.format() == pixel_format::GREY_U8 );
    for ( image::index_t i = 0; i < out_u8.pixel_count(); ++i )
        REQUIRE( out_u8.at<g_8>( i ) == g_8( static_cast<uint8_t>( i*2 ) ) );

    // out-parameter form matches the by-value form at equal depth
    image out_f64{ im.size(), pixel_format::GREY_F64 };
    copy_real( im, out_f64 );
    REQUIRE( out_f64 == re );

    // checks
    image non_complex{ 4, 4, pixel_format::GREY_U8 };
    image grey_out{ 4, 8, pixel_format::GREY_U8 };
    image rgba_out{ 4, 4, pixel_format::RGBA_U8 };
    REQUIRE_THROWS_AS( real( non_complex ), std::runtime_error );
    REQUIRE_THROWS_AS( copy_real( non_complex, grey_out ), std::runtime_error );
    REQUIRE_THROWS_AS( copy_real( im, rgba_out ), std::runtime_error );
    REQUIRE_THROWS_AS( copy_real( im, grey_out ), std::runtime_error );
}

TEST_CASE("image_utils_test - convert")
{
    // by-value form: a new image of the same size with the requested format
    image grey{ 8, 4, pixel_format::GREY_U8 };
    fill( grey, []( uint32_t w, uint32_t h ) { return (w + 10*h) % 251; } );

    image wide = convert( grey, pixel_format::GREY_F64 );
    REQUIRE( wide.format() == pixel_format::GREY_F64 );
    REQUIRE( wide.size() == grey.size() );
    for ( image::index_t i = 0; i < wide.pixel_count(); ++i )
        REQUIRE( doctest::matchers::WithinAbs( wide.at<g_f>( i ), grey.at<g_8>( i ), 1e-12 ) );

    // out-parameter form: the destination carries the requested format
    image dst{ grey.size(), pixel_format::GREY_F32 };
    copy_convert( grey, dst );
    for ( image::index_t i = 0; i < dst.pixel_count(); ++i )
        REQUIRE( dst.at<g_f32>( i ) == g_f32( grey.at<g_8>( i ) ) );

    // same-format copy_convert is a straight copy (sub-image sources
    // are honoured; the destination keeps its own rect and pitch, so
    // compare content rather than geometry)
    image sub = grey.sub_image( core::rect{ { 2, 1 }, core::size{ 4, 2 } } );
    image sub_copy{ sub.size(), pixel_format::GREY_U8 };
    copy_convert( sub, sub_copy );
    bool matched{ true };
    for ( image::index_t i = 0; i < sub.pixel_count(); ++i )
        if ( sub.at<g_8>( i ) != sub_copy.at<g_8>( i ) )
            matched = false;
    CHECK( matched );

    // round trip through two depths
    image back = convert( convert( grey, pixel_format::GREY_F64 ), pixel_format::GREY_U8 );
    REQUIRE( back == grey );

    // same format with the by-value form is a positioned deep copy
    image same = convert( grey, pixel_format::GREY_U8 );
    REQUIRE( same == grey );
    REQUIRE( same.rect() == grey.rect() );

    // checks: the destination's format is the requested format, so
    // only dimension and convertibility remain to validate
    image bad_size{ core::size{ 4, 4 }, pixel_format::GREY_F64 };
    REQUIRE_THROWS_AS( copy_convert( grey, bad_size ), std::runtime_error );
    // unsupported pixel pair: complex -> rgb has no conversion
    image cmplx{ 8, 4, pixel_format::COMPLEX_F64 };
    image rgb_dst{ core::size{ 8, 4 }, pixel_format::RGB_U8 };
    REQUIRE_THROWS_AS( copy_convert( cmplx, rgb_dst ), std::runtime_error );
    REQUIRE_THROWS_AS( convert( grey, pixel_format::UNKNOWN ), std::invalid_argument );
}
