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
        /// aliases the internal cache storage - copy (clone) or
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

            pfft::r2c<double>(
                shape(),
                strides( in_a ),
                strides( cache().output ),
                { 0, 1 },                      // axes
                d == direction::FORWARD,        // forward
                reinterpret_cast<const double*>( in_a.data() ),
                reinterpret_cast<std::complex<double>*>( cache().output.data() ),
                1.0 );

            image out_a{ cache().output.clone() };

            pfft::r2c<double>(
                shape(),
                strides( in_b ),
                strides( cache().temp ),
                { 0, 1 },                      // axes
                d == direction::FORWARD,        // forward
                reinterpret_cast<const double*>( in_b.data() ),
                reinterpret_cast<std::complex<double>*>( cache().temp.data() ),
                1.0 );

            image out_b{ cache().temp.clone() };

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
            image a_fft{ fa.clone() };
            const image& fb = transform( b, direction::FORWARD );

            // a_fft = fb * conj(a_fft)
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                const c_f* pb{ reinterpret_cast<const c_f*>( fb.line( h ) ) };
                c_f* pa{ reinterpret_cast<c_f*>( a_fft.line( h ) ) };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = *pb++ * pa->conj();
            }

            image output( size_, a.format() );
            copy_real( transform( a_fft, direction::REVERSE ), output );
            swap_quadrants( output );

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
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                const c_f* pb{ reinterpret_cast<const c_f*>( b_fft.line( h ) ) };
                c_f* pa{ reinterpret_cast<c_f*>( a_fft.line( h ) ) };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = *pb++ * pa->conj();
            }

            image output( size_, pixel_format::GREY_F64 );
            copy_real( transform( a_fft, direction::REVERSE ), output );
            swap_quadrants( output );

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

            image a_fft{ transform( a, direction::FORWARD ).clone() };

            // a_fft = abs_sqr( a_fft )
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                c_f* pa{ reinterpret_cast<c_f*>( a_fft.line( h ) ) };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = c_f( pa->abs_sqr() );
            }

            const image& fr = transform( a_fft, direction::REVERSE );

            image output( size_, pixel_format::COMPLEX_F64 );
            for ( uint32_t h = 0; h < output.height(); ++h )
            {
                const c_f* pi{ reinterpret_cast<const c_f*>( fr.line( h ) ) };
                c_f* po{ reinterpret_cast<c_f*>( output.line( h ) ) };
                for ( uint32_t w = 0; w < output.width(); ++w, ++pi )
                    *po++ = c_f( pi->real );
            }
            swap_quadrants( output );

            return output;
        }
    };

}
