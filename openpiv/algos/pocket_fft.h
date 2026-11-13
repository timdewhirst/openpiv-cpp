#pragma once

// std
#include <algorithm>
#include <exception>
#include <functional>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

// pocket
#define POCKETFFT_NO_MULTITHREADING
#include "pocketfft_hdronly.h"
#undef POCKETFFT_NO_MULTITHREADING

// local
#include "algos/fft_common.h"
#include "core/enum_helper.h"
#include "core/exception_builder.h"
#include "core/image.h"
#include "core/image_utils.h"
#include "core/pixel_format.h"
#include "core/util.h"

namespace openpiv::algos {

    using namespace core;
    namespace pfft = pocketfft;

    /// Wrapper for PocketFFT
    ///
    /// This class is thread-safe
    class PocketFFT
    {
        const size size_;

        /// storage for intermediate data
        struct data_t
        {
            image output;
            image temp;

            data_t( const core::size& s )
                : output( s, pixel_format::COMPLEX_F64 )
                , temp( s, pixel_format::COMPLEX_F64 )
            {}
        };

        /// helpers to allow TLS for intermediate storage
        using storage_t = std::vector< std::tuple<PocketFFT*, data_t> >;
        storage_t& storage() const
        {
            thread_local static storage_t static_data;
            return static_data;
        }

        /// cache contains a per-thread, per-instance copy of data
        /// that is lazily initialized; this allows a single instance
        /// of FFT to be called from multiple threads without locking
        data_t& cache() const
        {
            PocketFFT* self = const_cast<PocketFFT*>(this);
            for ( auto& [fft, data] : storage() )
            {
                if ( fft == self && data.output.size() == size_ )
                    return data;
            }

            auto& [fft, result] = storage().emplace_back( self, data_t{ size_ } );

            return result;
        }

        const pfft::shape_t shape() const
        {
            /// note: pocketfft shape is (x, y)
            return { size_.width(), size_.height() };
        }

        static pfft::stride_t strides( const image& im )
        {
            const auto [x, y] = im.stride();
            // pocketfft strides are (x, y) i.e. by dimension
            return { static_cast<long>( x ), static_cast<long>( y ) };
        }

    public:
        PocketFFT( const core::size& size )
            : size_(size)
        {
            // ensure power-of-two sizes
            if ( !(is_pow2(size_.width()) && is_pow2(size_.height()) ) )
                exception_builder<std::runtime_error>() << "dimensions must be power of 2: " << size_;
        }

        /// Perform a 2-D FFT; will always produce a complex
        /// floating point image output; note that the returned image
        /// aliases the internal cache storage - copy (detach) or
        /// transform the result before re-using this instance
        const image& transform( const image& input, direction d = direction::FORWARD ) const
        {
            DECLARE_ENTRY_EXIT
            if ( input.size() != size_ )
            {
                exception_builder< std::runtime_error >()
                    << "image size is different from expected: " << input.size() << ", " << size_;
            }

            // copy data, converting to complex
            if ( !input.info().is_mono && input.info().family != pixel_family::COMPLEX )
                exception_builder< std::runtime_error >()
                    << "transform requires a mono (greyscale or complex) image: "
                    << input.format();
            copy_convert( input, cache().temp );

            // can reinterpret core::complex to std::complex because core::complex
            // is packed and std::complex is also packed and makes guarantees about
            // accessibility through array access
            pfft::c2c<double>(
                shape(),
                strides( cache().temp ),
                strides( cache().output ),    // input and output strides should be the same
                { 0, 1 },                      // axes
                d == direction::FORWARD,       // forward
                reinterpret_cast<const std::complex<double>*>( cache().temp.data() ),
                reinterpret_cast<std::complex<double>*>( cache().output.data() ),
                1.0 );

            return cache().output;
        }

        /// Perform a 2-D FFT of two greyscale images; will produce
        /// two output (complex floating point) images
        std::tuple<image, image>
        transform_real( const image& a,
                        const image& b,
                        direction d = direction::FORWARD ) const
        {
            DECLARE_ENTRY_EXIT
            if ( a.size() != size_ || b.size() != size_ )
            {
                exception_builder< std::runtime_error >()
                    << "image size is different from expected: " << a.size()
                    << ", " << size_;
            }

            const pixel_format_info& ainfo{ a.info() };
            const pixel_format_info& binfo{ b.info() };
            if ( !ainfo.is_mono || !binfo.is_mono )
                core::exception_builder<std::runtime_error>()
                    << "transform_real requires greyscale images: " << a.format() << ", " << b.format();

            // non-double inputs are converted for the real transform;
            // note that input strides may arise from sub-images
            const image& in_a = a.info().depth == pixel_depth::F64 ? a : convert( a, pixel_format::GREY_F64 );
            const image& in_b = b.info().depth == pixel_depth::F64 ? b : convert( b, pixel_format::GREY_F64 );

            // pocketfft's r2c emits the half spectrum along the last
            // transformed axis (the height, here): rows 0..height/2
            // of the full-size output are written and the remainder
            // is left at zero. The input is real, so its spectrum is
            // conjugate-symmetric: complete the missing rows before
            // detaching, so the returned spectra are full and usable
            // as complex spectra
            const auto fill_mirrored_rows = []( image& im )
                {
                    const uint32_t width{ im.width() };
                    const uint32_t height{ im.height() };
                    c_f* p{ im.pixels<c_f>() };

                    for ( uint32_t h = height/2 + 1; h < height; ++h )
                        for ( uint32_t w = 0; w < width; ++w )
                            p[ h*width + w ] =
                                p[ (height - h)*width + ( (width - w) % width ) ].conj();
                };

            pfft::r2c<double>(
                shape(),
                strides( in_a ),
                strides( cache().output ),
                { 0, 1 },                      // axes
                d == direction::FORWARD,        // forward
                reinterpret_cast<const double*>( in_a.data() ),
                reinterpret_cast<std::complex<double>*>( cache().output.data() ),
                1.0 );
            fill_mirrored_rows( cache().output );

            image out_a{ cache().output.detach() };

            pfft::r2c<double>(
                shape(),
                strides( in_b ),
                strides( cache().temp ),
                { 0, 1 },                      // axes
                d == direction::FORWARD,        // forward
                reinterpret_cast<const double*>( in_b.data() ),
                reinterpret_cast<std::complex<double>*>( cache().temp.data() ),
                1.0 );
            fill_mirrored_rows( cache().temp );

            image out_b{ cache().temp.detach() };

            return { std::move( out_a ), std::move( out_b ) };
        }

        /// cross-correlate two images; requires greyscale inputs
        /// and returns a greyscale image of the same depth
        image
        cross_correlate( const image& a,
                         const image& b ) const
        {
            const pixel_format_info& ainfo{ a.info() };
            if ( !ainfo.is_mono )
                core::exception_builder<std::runtime_error>()
                    << "cross_correlate requires greyscale images: " << a.format();

            const image& fa = transform( a, direction::FORWARD );
            image a_fft{ fa.detach() };
            const image& fb = transform( b, direction::FORWARD );

            // a_fft = fb * conj(a_fft); note that fa has been
            // invalidated by the second transform (it aliases the
            // cache), so the detached copy is used as the second
            // source - dst aliases the second source
            apply<c_f>( fb, a_fft, a_fft, []( const c_f& x, const c_f& y )
                {
                    return x * y.conj();
                } );

            image output( size_, a.format() );
            copy_real_swap_quadrants( transform( a_fft, direction::REVERSE ), output );

            return output;
        }

        /// cross-correlate two greyscale images using the real
        /// transform trick; the output is a greyscale floating
        /// point image
        image
        cross_correlate_real( const image& a,
                              const image& b ) const
        {
            const pixel_format_info& ainfo{ a.info() };
            if ( !ainfo.is_mono )
                core::exception_builder<std::runtime_error>()
                    << "cross_correlate_real requires greyscale images: " << a.format();

            auto [a_fft, b_fft] = transform_real( a, b, direction::FORWARD );

            // a_fft = b_fft * conj(a_fft)
            apply<c_f>( b_fft, a_fft, a_fft, []( const c_f& x, const c_f& y )
                {
                    return x * y.conj();
                } );

            image output( size_, pixel_format::GREY_F64 );
            copy_real_swap_quadrants( transform( a_fft, direction::REVERSE ), output );

            return output;
        }

        /// auto-correlate an image; the output is a complex image
        /// with zero imaginary part
        image
        auto_correlate( const image& a ) const
        {
            const pixel_format_info& ainfo{ a.info() };
            if ( !ainfo.is_mono && ainfo.family != pixel_family::COMPLEX )
                core::exception_builder<std::runtime_error>()
                    << "auto_correlate requires a mono (greyscale or complex) image: " << a.format();

            image a_fft{ transform( a, direction::FORWARD ).detach() };

            // a_fft = abs_sqr( a_fft )
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                c_f* pa{ reinterpret_cast<c_f*>( a_fft.line( h ) ) };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = c_f( pa->abs_sqr() );
            }

            const image& fr = transform( a_fft, direction::REVERSE );

            // extract the real part of the reverse transform into
            // the output (a complex image with zero imaginary part,
            // matching the legacy behaviour) while swapping
            // quadrants - a single pass
            image output( size_, pixel_format::COMPLEX_F64 );
            const uint32_t half_w{ output.width() / 2 };
            const uint32_t half_h{ output.height() / 2 };
            for ( uint32_t h = 0; h < output.height(); ++h )
            {
                const c_f* pi{ fr.pixels<c_f>() + h * fr.width() };
                const uint32_t dst_h{ h < half_h ? h + half_h : h - half_h };
                c_f* po{ output.pixels<c_f>() + dst_h * output.width() };
                for ( uint32_t w = 0; w < half_w; ++w )
                {
                    po[ w + half_w ] = c_f( pi[ w ].real );
                    po[ w ] = c_f( pi[ w + half_w ].real );
                }
            }

            return output;
        }
    };

}
