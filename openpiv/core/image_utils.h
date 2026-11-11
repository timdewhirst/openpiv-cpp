#pragma once

// std
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <vector>

// local
#include "core/exception_builder.h"
#include "core/image.h"
#include "core/pixel_format.h"
#include "core/point.h"

namespace openpiv::core {

/// image utilities for the runtime-typed image

namespace detail {

    /// visit each pixel of an image as the compile-time pixel type;
    /// works for sub-images as pixels are packed within a row
    template <typename PixelT, typename F>
    void for_image_pixels( image& im, F&& f )
    {
        for ( uint32_t h = 0; h < im.height(); ++h )
        {
            PixelT* p{ reinterpret_cast<PixelT*>( im.line( h ) ) };
            for ( uint32_t w = 0; w < im.width(); ++w, ++p )
                f( *p, w, h );
        }
    }

    template <typename PixelT>
    void fill_impl( image& im, double v )
    {
        const typename PixelT::value_t value{ static_cast<typename PixelT::value_t>( v ) };
        for_image_pixels<PixelT>( im, [value]( PixelT& p, uint32_t, uint32_t )
            {
                p = PixelT( value );
            } );
    }

    template <typename PixelT, typename GeneratorT>
    void fill_generator_impl( image& im, GeneratorT& g )
    {
        for_image_pixels<PixelT>( im, [&g]( PixelT& p, uint32_t w, uint32_t h )
            {
                p = PixelT( static_cast<typename PixelT::value_t>( g( w, h ) ) );
            } );
    }

    template <typename PixelT, typename OpT>
    void apply_impl( image& im, OpT& op )
    {
        image::index_t index{ 0 };
        for_image_pixels<PixelT>( im, [&op, &index]( PixelT& p, uint32_t, uint32_t )
            {
                p = op( index++, p );
            } );
    }

}

/// fill an image with a constant; the value is converted to the
/// image's pixel format
inline image& fill( image& im, double v )
{

    dispatch_pixel_type( im.format(), [&im, v]( auto tag )
        {
            detail::fill_impl<typename decltype( tag )::type>( im, v );
        } );
    return im;
}

/// fill an image with values produced by a generator function of
/// the form g(w, h) -> double
template <typename GeneratorT>
image& fill( image& im, GeneratorT g )
{
    static_assert( std::is_invocable_v< GeneratorT, uint32_t, uint32_t >,
                   "fill generator must be invocable as g(uint32_t w, uint32_t h)" );


    dispatch_pixel_type( im.format(), [&im, &g]( auto tag )
        {
            detail::fill_generator_impl<typename decltype( tag )::type>( im, g );
        } );
    return im;
}

/// apply a function to each pixel; op is of the form
/// op(index, pixel) -> pixel
template <typename OpT>
image& apply( image& im, OpT op )
{

    dispatch_pixel_type( im.format(), [&im, &op]( auto tag )
        {
            detail::apply_impl<typename decltype( tag )::type>( im, op );
        } );
    return im;
}

/// find the sum of all pixels of a greyscale image; integral depths
/// are accumulated exactly
inline double pixel_sum( const image& im )
{

    if ( im.info().family != pixel_family::GREY )
        core::exception_builder<std::runtime_error>()
            << "pixel_sum requires a greyscale image: " << im.format();

    return dispatch_pixel_type( im.format(), [&im]( auto tag )
        {
            using value_t = typename decltype( tag )::type::value_t;
            if constexpr ( std::is_integral_v<value_t> )
            {
                int64_t sum{ 0 };
                for ( uint32_t h = 0; h < im.height(); ++h )
                {
                    const value_t* p{ reinterpret_cast<const value_t*>( im.line( h ) ) };
                    for ( uint32_t w = 0; w < im.width(); ++w )
                        sum += *p++;
                }
                return static_cast<double>( sum );
            }
            else
            {
                double sum{ 0 };
                for ( uint32_t h = 0; h < im.height(); ++h )
                {
                    const value_t* p{ reinterpret_cast<const value_t*>( im.line( h ) ) };
                    for ( uint32_t w = 0; w < im.width(); ++w )
                        sum += *p++;
                }
                return sum;
            }
        } );
}

/// transpose an image i.e. rows <-> columns; maps from \a in to \a
/// out; the dimensions of \a in and \a out must be transposed as a
/// pre-condition (and the formats equal).
///
/// \returns a reference to \a out.
inline image& transpose( const image& in, image& out )
{
    if ( in.format() != out.format() ||
         !(in.width() == out.height() && in.height() == out.width() ) )
        core::exception_builder<std::runtime_error>()
            << "input and output must be images of the same format with transposed "
               "dimensions: " << in.format() << " " << in.size() << ", "
            << out.format() << " " << out.size();


    const uint8_t bpp{ in.info().bytes_per_pixel };

    // pointers to the start of every output line
    std::vector<image::byte_t*> olines( out.height() );
    for ( uint32_t h = 0; h < out.height(); ++h )
        olines[h] = out.line( h );

    for ( uint32_t h = 0; h < in.height(); ++h )
    {
        const image::byte_t* p{ in.line( h ) };
        for ( uint32_t w = 0; w < in.width(); ++w )
        {
            std::memcpy( olines[w], p, bpp );
            olines[w] += bpp;
            p += bpp;
        }
    }

    return out;
}

/// transpose an image i.e. rows <-> columns; \returns a new transposed image
inline image transpose( const image& im )
{
    image out{ im.height(), im.width(), im.format() };
    transpose( im, out );
    return out;
}

/// swap quadrants of an even dimensioned image i.e.
/// - quadrant 1 <-> quadrant 3
/// - quadrant 2 <-> quadrant 4
inline image& swap_quadrants( image& im )
{
    const uint32_t width{ im.width() };
    const uint32_t height{ im.height() };
    if ( width % 2 || height % 2 )
        core::exception_builder<std::runtime_error>()
            << "swap_quadrants requires even dimensions: " << im.size();


    const uint8_t bpp{ im.info().bytes_per_pixel };

    for ( uint32_t h = 0; h < height; ++h )
    {
        image::byte_t* i{ im.line( h ) };
        image::byte_t* o{ im.line( (h + height/2) % height ) };

        for ( uint32_t w = 0; w < width/2; ++w )
        {
            const uint32_t ow{ (w + width/2) % width };
            std::swap_ranges( i + w*bpp, i + w*bpp + bpp, o + ow*bpp );
        }
    }

    return im;
}

/// find the highest \a num_peaks peaks in a greyscale image and
/// return a sorted vector of peaks.
/// The peaks are returned as sub-images sharing the input data and
/// the size of each peak can be adjusted by setting \a peak_radius
inline std::vector<image> find_peaks( const image& im, uint16_t num_peaks, uint32_t peak_radius )
{

    if ( im.info().family != pixel_family::GREY )
        core::exception_builder<std::runtime_error>()
            << "find_peaks requires a greyscale image: " << im.format();

    if ( !( im.width() >= 2*peak_radius + 1 && im.height() >= 2*peak_radius + 1 ) )
        core::exception_builder<std::runtime_error>()
            << "find_peaks: image is too small for peak_radius: " << im.size() << ", " << peak_radius;

    using candidate_t = std::pair<double, core::rect>;
    std::vector<candidate_t> candidates;

    const uint32_t width{ im.width() };
    const uint32_t height{ im.height() };
    const uint32_t result_w{ 2*peak_radius + 1 };

    dispatch_pixel_type( im.format(),
        [&]( auto tag )
        {
            using PixelT = typename decltype( tag )::type;
            using V = typename PixelT::value_t;

            for ( uint32_t h = peak_radius; h < height - 2*peak_radius; ++h )
            {
                const V* above{ reinterpret_cast<const V*>( im.line( h - 1 ) ) };
                const V* line{ reinterpret_cast<const V*>( im.line( h ) ) };
                const V* below{ reinterpret_cast<const V*>( im.line( h + 1 ) ) };

                for ( uint32_t w = peak_radius; w < width - peak_radius; ++w )
                {
                    // check we have peak on this line before checking above and below
                    if ( line[w-1] < line[w] && line[w+1] < line[w] &&
                         above[w] < line[w] && below[w] < line[w] )
                    {
                        candidates.emplace_back(
                            static_cast<double>( line[w] ),
                            core::rect{ { static_cast<int32_t>(w - peak_radius),
                                         static_cast<int32_t>(h - peak_radius) },
                                       core::size{ result_w, result_w } } );
                    }
                }
            }
        } );

    std::sort( std::begin( candidates ), std::end( candidates ),
               []( const candidate_t& a, const candidate_t& b ) -> bool
               {
                   return a.first > b.first;
               } );
    candidates.resize( std::min( candidates.size(), static_cast<size_t>( num_peaks ) ) );

    std::vector<image> result;
    result.reserve( candidates.size() );
    for ( const auto& [value, r] : candidates )
        result.push_back( im.sub_image( r ) );

    return result;
}

/// fit two one-dimensional Gaussian curves to a 3x3 peak; the
/// input must be a greyscale image
inline point2<double> fit_simple_gaussian( const image& im )
{
    if ( im.size() != core::size{ 3, 3 } )
        core::exception_builder<std::runtime_error>()
            << "fit_simple_gaussian: input must be 3x3: " << im.size();

    if ( im.info().family != pixel_family::GREY )
        core::exception_builder<std::runtime_error>()
            << "fit_simple_gaussian requires a greyscale image: " << im.format();

    point2<double> result{ im.rect().midpoint() };

    auto f = []( double l, double c, double r ) -> double
        {
            const double num = std::log( l ) - std::log( r );
            const double den = 2.0*( std::log( l ) + std::log( r ) - 2.0*std::log( c ) );

            if ( den == 0.0 )
                return 0.0;

            return num/den;
        };

    result[0] += dispatch_pixel_type(
        im.format(),
        [&im, &f]( auto tag ) -> double
        {
            using V = typename decltype( tag )::type::value_t;
            const V* line{ reinterpret_cast<const V*>( im.line( 1 ) ) };
            return f( line[0], line[1], line[2] );
        } );

    result[1] += dispatch_pixel_type(
        im.format(),
        [&im, &f]( auto tag ) -> double
        {
            using V = typename decltype( tag )::type::value_t;
            const V* row0{ reinterpret_cast<const V*>( im.line( 0 ) ) };
            const V* row1{ reinterpret_cast<const V*>( im.line( 1 ) ) };
            const V* row2{ reinterpret_cast<const V*>( im.line( 2 ) ) };
            return f( row0[1], row1[1], row2[1] );
        } );

    return result;
}

/// split an RGBA image into (r, g, b, a) greyscale images of the
/// same depth
inline std::tuple<image, image, image, image> split_to_channels( const image& im )
{

    if ( im.info().family != pixel_family::RGBA )
        core::exception_builder<std::runtime_error>()
            << "split_to_channels requires an RGBA image: " << im.format();

    return dispatch_pixel_depth(
        im.info().depth,
        [&im]( auto tag ) -> std::tuple<image, image, image, image>
        {
            using V = typename decltype( tag )::type;
            using PixelT = rgba<V>;

            image r_im{ im.width(), im.height(), format_for_v< g<V> > };
            image g_im{ im.width(), im.height(), format_for_v< g<V> > };
            image b_im{ im.width(), im.height(), format_for_v< g<V> > };
            image a_im{ im.width(), im.height(), format_for_v< g<V> > };

            for ( uint32_t h = 0; h < im.height(); ++h )
            {
                const PixelT* p{ reinterpret_cast<const PixelT*>( im.line( h ) ) };
                V* r{ reinterpret_cast<V*>( r_im.line( h ) ) };
                V* g{ reinterpret_cast<V*>( g_im.line( h ) ) };
                V* b{ reinterpret_cast<V*>( b_im.line( h ) ) };
                V* a{ reinterpret_cast<V*>( a_im.line( h ) ) };
                for ( uint32_t w = 0; w < im.width(); ++w, ++p )
                {
                    *r++ = p->r;
                    *g++ = p->g;
                    *b++ = p->b;
                    *a++ = p->a;
                }
            }

            return { std::move( r_im ), std::move( g_im ), std::move( b_im ), std::move( a_im ) };
        } );
}

/// split a complex image into (real, imag) greyscale images of the
/// same depth
inline std::tuple<image, image> split_complex_to_channels( const image& im )
{

    if ( im.info().family != pixel_family::COMPLEX )
        core::exception_builder<std::runtime_error>()
            << "split_complex_to_channels requires a complex image: " << im.format();

    return dispatch_pixel_depth(
        im.info().depth,
        [&im]( auto tag ) -> std::tuple<image, image>
        {
            using V = typename decltype( tag )::type;
            using PixelT = complex<V>;

            // integral complex types are signed; grey types are
            // unsigned, so map by depth and copy components bitwise
            const pixel_format grey_format{ grey_format_for_depth( im.info().depth ) };
            image real_im{ im.width(), im.height(), grey_format };
            image imag_im{ im.width(), im.height(), grey_format };

            for ( uint32_t h = 0; h < im.height(); ++h )
            {
                const PixelT* p{ reinterpret_cast<const PixelT*>( im.line( h ) ) };
                V* r{ reinterpret_cast<V*>( real_im.line( h ) ) };
                V* i{ reinterpret_cast<V*>( imag_im.line( h ) ) };
                for ( uint32_t w = 0; w < im.width(); ++w, ++p )
                {
                    *r++ = p->real;
                    *i++ = p->imag;
                }
            }

            return { std::move( real_im ), std::move( imag_im ) };
        } );
}

/// join four greyscale images into a single RGBA image; the input
/// images must be of the same format and dimension
inline image join_from_channels( const image& r_im, const image& g_im,
                                 const image& b_im, const image& a_im )
{
    if ( r_im.format() != g_im.format() || r_im.format() != b_im.format() ||
         r_im.format() != a_im.format() )
        core::exception_builder<std::runtime_error>()
            << "join_from_channels: source images must have the same format";

    if ( r_im.size() != g_im.size() || r_im.size() != b_im.size() ||
         r_im.size() != a_im.size() )
        core::exception_builder<std::runtime_error>()
            << "join_from_channels: source images must have matching dimensions";

    return dispatch_pixel_depth(
        r_im.info().depth,
        [&r_im, &g_im, &b_im, &a_im]( auto tag ) -> image
        {
            using V = typename decltype( tag )::type;
            using PixelT = rgba<V>;

            image result{ r_im.width(), r_im.height(), format_for_v< rgba<V> > };

            for ( uint32_t h = 0; h < result.height(); ++h )
            {
                const V* r{ reinterpret_cast<const V*>( r_im.line( h ) ) };
                const V* g{ reinterpret_cast<const V*>( g_im.line( h ) ) };
                const V* b{ reinterpret_cast<const V*>( b_im.line( h ) ) };
                const V* a{ reinterpret_cast<const V*>( a_im.line( h ) ) };
                PixelT* p{ reinterpret_cast<PixelT*>( result.line( h ) ) };
                for ( uint32_t w = 0; w < result.width(); ++w, ++p )
                {
                    p->r = *r++;
                    p->g = *g++;
                    p->b = *b++;
                    p->a = *a++;
                }
            }

            return result;
        } );
}

/// join two greyscale images (real, imag) into a single complex
/// image of the same depth; the input images must be of the same
/// format and dimension
inline image join_complex_from_channels( const image& real_im, const image& imag_im )
{
    if ( real_im.format() != imag_im.format() )
        core::exception_builder<std::runtime_error>()
            << "join_complex_from_channels: source images must have the same format";

    if ( real_im.size() != imag_im.size() )
        core::exception_builder<std::runtime_error>()
            << "join_complex_from_channels: source images must have matching dimensions";

    return dispatch_pixel_depth(
        real_im.info().depth,
        [&real_im, &imag_im]( auto tag ) -> image
        {
            using V = typename decltype( tag )::type;
            using PixelT = complex<V>;

            // integral complex types are signed; grey types are
            // unsigned, so map by depth and copy components bitwise
            const pixel_format complex_format{ complex_format_for_depth( real_im.info().depth ) };
            image result{ real_im.width(), real_im.height(), complex_format };

            for ( uint32_t h = 0; h < result.height(); ++h )
            {
                const V* r{ reinterpret_cast<const V*>( real_im.line( h ) ) };
                const V* i{ reinterpret_cast<const V*>( imag_im.line( h ) ) };
                PixelT* p{ reinterpret_cast<PixelT*>( result.line( h ) ) };
                for ( uint32_t w = 0; w < result.width(); ++w, ++p )
                {
                    p->real = *r++;
                    p->imag = *i++;
                }
            }

            return result;
        } );
}

/// extract the real part of a complex image into \a out; \a out
/// must be a greyscale image of the same size and may have any
/// depth - values are truncated as required. Note: no abs, use
/// convert() for magnitude
inline image& real( const image& in, image& out )
{
    if ( in.info().family != pixel_family::COMPLEX )
        core::exception_builder<std::runtime_error>()
            << "real requires a complex image: " << in.format();

    if ( out.info().family != pixel_family::GREY )
        core::exception_builder<std::runtime_error>()
            << "real output must be greyscale: " << out.format();

    if ( in.size() != out.size() )
        core::exception_builder<std::runtime_error>()
            << "real: source images must have matching dimensions: "
            << in.size() << ", " << out.size();

    // note: \a in may be a sub-image, so advance rows by the pitch
    dispatch_pixel_depth(
        in.info().depth,
        [&in, &out]( auto tag )
        {
            using FromT = typename decltype( tag )::type;
            using PixelT = complex<FromT>;
            const size_t in_stride{ in.pitch() / sizeof( PixelT ) };

            dispatch_pixel_depth(
                out.info().depth,
                [&in, &out, &in_stride]( auto out_tag )
                {
                    using V = typename decltype( out_tag )::type;
                    for ( uint32_t h = 0; h < in.height(); ++h )
                    {
                        const PixelT* pi{ in.pixels<PixelT>() + h * in_stride };
                        V* po{ reinterpret_cast<V*>( out.line( h ) ) };
                        for ( uint32_t w = 0; w < in.width(); ++w, ++pi )
                            *po++ = static_cast<V>( pi->real );
                    }
                } );
        } );

    return out;
}

/// extract the real part of a complex image as a greyscale image
/// of the same depth (note: no abs, use convert() for magnitude)
inline image real( const image& im )
{
    if ( im.info().family != pixel_family::COMPLEX )
        core::exception_builder<std::runtime_error>()
            << "real requires a complex image: " << im.format();

    // integral complex types are signed; grey types are unsigned,
    // so map by depth
    const pixel_format grey_format{ grey_format_for_depth( im.info().depth ) };
    image result{ im.width(), im.height(), grey_format };
    real( im, result );
    return result;
}

namespace detail {

/// convert pixels from one compile-time pixel type to another,
/// row by row; pairs for which no conversion exists are runtime
/// errors; \a src may be a sub-image, \a dst must be a full image
template <typename FromT, typename ToT>
void convert_pixel_range( const image& src, image& dst )
{
    if constexpr ( pixeltype_is_convertible_v<FromT, ToT> )
    {
        // \a src and \a dst may be sub-images, so advance rows by
        // the pitch
        const size_t src_stride{ src.pitch() / sizeof( FromT ) };
        const size_t dst_stride{ dst.pitch() / sizeof( ToT ) };
        for ( uint32_t h = 0; h < src.height(); ++h )
        {
            const FromT* s{ src.pixels<FromT>() + h * src_stride };
            ToT* d{ dst.pixels<ToT>() + h * dst_stride };
            for ( uint32_t w = 0; w < src.width(); ++w )
                convert( *s++, *d++ );
        }
    }
    else
    {
        core::exception_builder<std::runtime_error>()
            << "unsupported pixel conversion: " << format_for_v<FromT>
            << " -> " << format_for_v<ToT>;
    }
}

/// dispatch on the source pixel type
template <typename ToT>
void convert_dispatch( const image& src, image& dst )
{
#define OPENPIV_FROM_FORMAT_CASE( FMT )                                                       \
    case pixel_format::FMT:                                                                   \
        convert_pixel_range< pixel_type_for_t< pixel_format::FMT >, ToT >( src, dst );         \
        break;

    switch ( src.format() )
    {
        OPENPIV_FROM_FORMAT_CASE( GREY_U8 )
        OPENPIV_FROM_FORMAT_CASE( GREY_U16 )
        OPENPIV_FROM_FORMAT_CASE( GREY_U32 )
        OPENPIV_FROM_FORMAT_CASE( GREY_F32 )
        OPENPIV_FROM_FORMAT_CASE( GREY_F64 )
        OPENPIV_FROM_FORMAT_CASE( RGB_U8 )
        OPENPIV_FROM_FORMAT_CASE( RGB_U16 )
        OPENPIV_FROM_FORMAT_CASE( RGB_U32 )
        OPENPIV_FROM_FORMAT_CASE( RGB_F32 )
        OPENPIV_FROM_FORMAT_CASE( RGB_F64 )
        OPENPIV_FROM_FORMAT_CASE( RGBA_U8 )
        OPENPIV_FROM_FORMAT_CASE( RGBA_U16 )
        OPENPIV_FROM_FORMAT_CASE( RGBA_U32 )
        OPENPIV_FROM_FORMAT_CASE( RGBA_F32 )
        OPENPIV_FROM_FORMAT_CASE( RGBA_F64 )
        OPENPIV_FROM_FORMAT_CASE( YUV_U8 )
        OPENPIV_FROM_FORMAT_CASE( YUV_U16 )
        OPENPIV_FROM_FORMAT_CASE( YUV_U32 )
        OPENPIV_FROM_FORMAT_CASE( YUV_F32 )
        OPENPIV_FROM_FORMAT_CASE( YUV_F64 )
        OPENPIV_FROM_FORMAT_CASE( YUVA_U8 )
        OPENPIV_FROM_FORMAT_CASE( YUVA_U16 )
        OPENPIV_FROM_FORMAT_CASE( YUVA_U32 )
        OPENPIV_FROM_FORMAT_CASE( YUVA_F32 )
        OPENPIV_FROM_FORMAT_CASE( YUVA_F64 )
        OPENPIV_FROM_FORMAT_CASE( COMPLEX_U8 )
        OPENPIV_FROM_FORMAT_CASE( COMPLEX_U16 )
        OPENPIV_FROM_FORMAT_CASE( COMPLEX_U32 )
        OPENPIV_FROM_FORMAT_CASE( COMPLEX_F32 )
        OPENPIV_FROM_FORMAT_CASE( COMPLEX_F64 )
        default:
            core::exception_builder<std::runtime_error>()
                << "unsupported source format: " << src.format();
    }

#undef OPENPIV_FROM_FORMAT_CASE
}

} // namespace detail

/// convert the pixels of \a src into \a dst; \a dst must be of the
/// same size and its format carries the requested image type; \a
/// src and \a dst may be sub-images. Same-format conversions are
/// straight copies; pixel pairs for which no conversion exists
/// are runtime errors
inline image& copy_convert( const image& src, image& dst )
{
    if ( src.size() != dst.size() )
        core::exception_builder<std::runtime_error>()
            << "copy_convert: source and destination must have matching dimensions: "
            << src.size() << ", " << dst.size();

    if ( src.format() == dst.format() )
    {
        // same format: straight copy
        const uint8_t bpp{ src.info().bytes_per_pixel };
        for ( uint32_t h = 0; h < src.height(); ++h )
            std::memcpy( dst.line( h ), src.line( h ),
                         static_cast<size_t>( src.width() ) * bpp );
        return dst;
    }

#define OPENPIV_TO_FORMAT_CASE( FMT )                                                         \
    case pixel_format::FMT:                                                                   \
        detail::convert_dispatch< pixel_type_for_t< pixel_format::FMT > >( src, dst );         \
        break;

    switch ( dst.format() )
    {
        OPENPIV_TO_FORMAT_CASE( GREY_U8 )
        OPENPIV_TO_FORMAT_CASE( GREY_U16 )
        OPENPIV_TO_FORMAT_CASE( GREY_U32 )
        OPENPIV_TO_FORMAT_CASE( GREY_F32 )
        OPENPIV_TO_FORMAT_CASE( GREY_F64 )
        OPENPIV_TO_FORMAT_CASE( RGB_U8 )
        OPENPIV_TO_FORMAT_CASE( RGB_U16 )
        OPENPIV_TO_FORMAT_CASE( RGB_U32 )
        OPENPIV_TO_FORMAT_CASE( RGB_F32 )
        OPENPIV_TO_FORMAT_CASE( RGB_F64 )
        OPENPIV_TO_FORMAT_CASE( RGBA_U8 )
        OPENPIV_TO_FORMAT_CASE( RGBA_U16 )
        OPENPIV_TO_FORMAT_CASE( RGBA_U32 )
        OPENPIV_TO_FORMAT_CASE( RGBA_F32 )
        OPENPIV_TO_FORMAT_CASE( RGBA_F64 )
        OPENPIV_TO_FORMAT_CASE( YUV_U8 )
        OPENPIV_TO_FORMAT_CASE( YUV_U16 )
        OPENPIV_TO_FORMAT_CASE( YUV_U32 )
        OPENPIV_TO_FORMAT_CASE( YUV_F32 )
        OPENPIV_TO_FORMAT_CASE( YUV_F64 )
        OPENPIV_TO_FORMAT_CASE( YUVA_U8 )
        OPENPIV_TO_FORMAT_CASE( YUVA_U16 )
        OPENPIV_TO_FORMAT_CASE( YUVA_U32 )
        OPENPIV_TO_FORMAT_CASE( YUVA_F32 )
        OPENPIV_TO_FORMAT_CASE( YUVA_F64 )
        OPENPIV_TO_FORMAT_CASE( COMPLEX_U8 )
        OPENPIV_TO_FORMAT_CASE( COMPLEX_U16 )
        OPENPIV_TO_FORMAT_CASE( COMPLEX_U32 )
        OPENPIV_TO_FORMAT_CASE( COMPLEX_F32 )
        OPENPIV_TO_FORMAT_CASE( COMPLEX_F64 )
        default:
            core::exception_builder<std::runtime_error>()
                << "unsupported destination format: " << dst.format();
    }

#undef OPENPIV_TO_FORMAT_CASE

    return dst;
}

/// convert an image to \a to; \returns a new image of the same
/// size with the requested format, using the pixel conversion
/// functions of pixel_types.h; the result is positioned as per
/// extract() when the format does not change
inline image convert( const image& src,
                      pixel_format to,
                      const image::allocator_t& allocator = image::default_allocator() )
{
    if ( to == pixel_format::UNKNOWN )
        core::exception_builder<std::invalid_argument>()
            << "destination format must be known: " << to;

    if ( to == src.format() )
        return src.extract( core::rect::from_size( src.size() ), allocator );

    image dst{ src.size(), to, allocator };
    copy_convert( src, dst );
    return dst;
}

} // namespace openpiv::core
