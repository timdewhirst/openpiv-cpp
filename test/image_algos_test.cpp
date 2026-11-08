#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

// doctest
#include <doctest.h>
#include "doctest_utils.h"

// std
#include <cmath>
#include <thread>

// local
#include "test_utils.h"

// to be tested
#include "algos/fft.h"
#include "algos/stats.h"
#include "core/image_utils.h"
#include "loaders/image_loader.h"

using namespace std::string_literals;
using namespace openpiv;
using namespace openpiv::core;
using namespace openpiv::algos;

TEST_CASE("image_algos_test - FFTTest")
{
    // generate sinusoidal pattern
    image im{ 256, 256, pixel_format::GREY_F64 };
    fill( im, []( uint32_t w, uint32_t h ){ return 128 * std::sin( 2*M_PI*w/8 + 2*M_PI*h/8 ); } );

    // save
    REQUIRE( save_to_file( "fft-input.pgm", im ) );

    // transform
    FFT fft( im.size() );
    image output{ fft.transform( im, direction::FORWARD ).clone() };

    REQUIRE( save_to_file( "fft-output.pgm", convert( output, pixel_format::GREY_F64 ) ) );

    // two peaks; relies on quadrants being unswapped i.e. zero
    // frequency is at edges
    for ( uint32_t y=0; y<output.height(); ++y )
        for ( uint32_t x=0; x<output.width(); ++x )
        {
            auto p = point2<uint32_t>{ x, y };
            if ( x == 32 && y == 32 )
            {
                REQUIRE( (output.at<c_f>( p ) != c_f{}) );
            }
            else if ( x == 224 && y == 224 )
            {
                REQUIRE( (output.at<c_f>( p ) != c_f{}) );
            }
            else
            {
                REQUIRE( doctest::matchers::WithinAbs(output.at<c_f>( p ).abs_sqr(), 0, 1e-9) );
            }
        }

    // inverse transform
    image g{ real( fft.transform( output, direction::REVERSE ) ) };

    REQUIRE( save_to_file( "fft-reverse.pgm", g) );
}

TEST_CASE("image_algos_test - FFT two real images")
{
    // generate sinusoidal patterns
    image a{ 256, 256, pixel_format::GREY_F64 };
    fill( a, []( uint32_t w, uint32_t h ){ return 128 * std::sin( 2*M_PI*w/8 + 2*M_PI*h/8 ); } );

    image b{ 256, 256, pixel_format::GREY_F64 };
    fill( b,
          [width=b.width()]( uint32_t w, uint32_t h ) {
              return 128 * std::sin( 2*M_PI*(width-w)/8 + 2*M_PI*h/8 );
          } );

    // save
    REQUIRE( save_to_file( "fft-real-input-a.pgm", a ) );
    REQUIRE( save_to_file( "fft-real-input-b.pgm", b ) );

    // transform
    FFT fft( a.size() );
    auto [a_fft, b_fft] = fft.transform_real( a, b, direction::FORWARD );

    REQUIRE( save_to_file( "fft-real-output-a.pgm", convert( a_fft, pixel_format::GREY_F64 ) ) );
    REQUIRE( save_to_file( "fft-real-output-b.pgm", convert( b_fft, pixel_format::GREY_F64 ) ) );

    // two peaks; relies on quadrants being unswapped i.e. zero
    // frequency is at edges
    auto checker =
        [](const image& im, point2<uint32_t> a, point2<uint32_t> b)
        {
            for ( uint32_t y=0; y<im.height(); ++y )
                for ( uint32_t x=0; x<im.width(); ++x )
                {
                    auto p = point2<uint32_t>{ x, y };
                    if ( p == a || p == b )
                    {
                        REQUIRE( (im.at<c_f>( p ) != c_f{}) );
                    }
                    else
                    {
                        REQUIRE( doctest::matchers::WithinAbs(im.at<c_f>( p ).abs_sqr(), 0, 1e-9) );
                    }
                }
        };

    checker(a_fft, {32, 32}, {224, 224});
    checker(b_fft, {32, 224}, {224, 32});
}

TEST_CASE("image_algos_test - FFT Different Size")
{
    image im{ 256, 256, pixel_format::GREY_F64 };
    fill( im, []( uint32_t w, uint32_t ){ return (w/2)%2 ? 1.0 : 0.0; } );

    FFT fft( { 512, 512 } );
    _REQUIRE_THROWS_MATCHES( fft.transform( im ),
                             std::runtime_error,
                             ContainsSubstring( "image size is different" , CaseSensitive::No ) );
}

TEST_CASE("image_algos_test - FFT non-power-of-two size")
{
    _REQUIRE_THROWS_MATCHES( FFT( { 512, 400 } ),
                             std::runtime_error,
                             ContainsSubstring( "power of 2" , CaseSensitive::No ) );
    _REQUIRE_THROWS_MATCHES( FFT( { 400, 512 } ),
                             std::runtime_error,
                             ContainsSubstring( "power of 2" , CaseSensitive::No ) );
}

TEST_CASE("image_algos_test - cross_correlation_test")
{
    // load images
    auto im = load_from_file( "corr_a.tiff" );
    im = convert( im, pixel_format::GREY_F64 );

    // extract a couple of small windows
    size s{ 128, 128 };
    auto view_a = im.sub_image( rect{ {20, 20}, s } );
    auto view_b = im.sub_image( rect{ {20, 25}, s } );

    REQUIRE( save_to_file( "cross-correlate-a-input.pgm", view_a ) );
    REQUIRE( save_to_file( "cross-correlate-b-input.pgm", view_b ) );

    // prepare & correlate
    FFT fft( view_a.size() );
    image output{ fft.cross_correlate( view_a, view_b ) };

    // output format matches the input depth
    REQUIRE( output.format() == pixel_format::GREY_F64 );

    // write output
    REQUIRE( save_to_file( "cross-correlate-output.pgm", output ) );

    // correlation of a window with itself peaks at the centre
    image self_output{ fft.cross_correlate( view_a, view_a ) };
    auto [min, max] = algos::find_image_range( self_output );
    REQUIRE( max > min );
    const auto centre_value = self_output.at<g_f>( point2<uint32_t>( 64, 64 ) );
    REQUIRE( doctest::matchers::WithinAbs( centre_value, max, 1e-6 ) );
}

TEST_CASE("image_algos_test - auto_correlation_test")
{
    // load images
    auto im = load_from_file( "corr_a.tiff" );
    im = convert( im, pixel_format::GREY_F64 );

    // extract a couple of small windows
    size s{ 128, 128 };
    auto view_a = im.sub_image( rect{ {20, 20}, s } );
    auto view_b = im.sub_image( rect{ {20, 25}, s } );

    // combine
    image data{ 128, 128, pixel_format::GREY_F64 };
    for ( image::index_t i = 0; i < data.pixel_count(); ++i )
        data.at<g_f>( i ) = view_a.at<g_f>( i ) + view_b.at<g_f>( i );
    REQUIRE( save_to_file( "auto-correlate-input.pgm", data) );

    // prepare & correlate; output is complex with zero imaginary part
    FFT fft( data.size() );
    image output{ fft.auto_correlate( data ) };
    REQUIRE( output.format() == pixel_format::COMPLEX_F64 );
    REQUIRE( save_to_file( "auto-correlate-output.pgm", convert( output, pixel_format::GREY_F64 ) ) );
}

TEST_CASE("image_algos_test - fft_real_test")
{
    // load images
    auto im_a = load_from_file( "corr_a.tiff" );
    im_a = convert( im_a, pixel_format::GREY_F64 );

    // extract a couple of small windows
    size s{ 128, 128 };
    auto view_a = im_a.sub_image( rect{ {20, 20}, s } );
    auto view_b = im_a.sub_image( rect{ {20, 25}, s } );

    // combine
    image data{ s, pixel_format::GREY_F64 };
    for ( image::index_t i = 0; i < data.pixel_count(); ++i )
        data.at<g_f>( i ) = view_a.at<g_f>( i ) + view_b.at<g_f>( i );

    REQUIRE( save_to_file( "fft_corr_a_input.pgm", data ) );

    // transform
    FFT fft( data.size() );
    image output{ fft.transform( data, direction::FORWARD ).clone() };
    REQUIRE( output.format() == pixel_format::COMPLEX_F64 );

    // remove DC
    output.at<c_f>( {0, 0} ) = c_f{};

    // transformed
    swap_quadrants( output );
    REQUIRE( save_to_file( "fft_corr_a_fourier.pgm", convert( output, pixel_format::GREY_F64 ) ) );

    // complex conjugate
    for ( uint32_t h=0; h<output.height(); ++h )
        for ( uint32_t w=0; w<output.width(); ++w )
        {
            auto p = point2<uint32_t>{ w, h };
            output.at<c_f>( p ) = output.at<c_f>( p ) * output.at<c_f>( p ).conj();
        }

    REQUIRE( save_to_file( "fft_corr_a_conj.pgm", convert( output, pixel_format::GREY_F64 ) ) );

    // inverse
    output = fft.transform( convert( output, pixel_format::GREY_F64 ), direction::REVERSE ).clone();
    swap_quadrants( output );

    // write output
    REQUIRE( save_to_file( "fft_corr_a_output.pgm", convert( output, pixel_format::GREY_F64 ) ) );
}
