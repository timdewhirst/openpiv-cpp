#pragma once

// std
#include <algorithm>
#include <exception>
#include <functional>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

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

    /// A basic decimate-in-time FFT algorithm; not highly optimized.
    ///
    /// This class is thread-safe
    class FFT
    {
        const size size_;

        /// contains "twiddle factors" for each n: 2^n < N, n>=0
        using scaling_map_t = std::unordered_map< size_t, std::vector<c_f> >;
        const scaling_map_t forward_scaling_;
        const scaling_map_t reverse_scaling_;

        /// storage for intermediate data; images are complex
        /// floating point
        struct data_t
        {
            image output;
            std::vector< c_f > fft_buffer;
            image temp;

            data_t( const core::size& s )
                : output( s, pixel_format::COMPLEX_F64 )
                , fft_buffer( maximal_size( s ).width() )
                , temp( transpose( s ), pixel_format::COMPLEX_F64 )
            {}
        };

        /// helpers to allow TLS for intermediate storage
        using storage_t = std::vector< std::tuple<FFT*, data_t> >;
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
            FFT* self = const_cast<FFT*>(this);
            for ( auto& [fft, data] : storage() )
            {
                if ( fft == self && data.output.size() == size_ )
                    return data;
            }

            auto& [fft, result] = storage().emplace_back( self, data_t{ size_ } );

            return result;
        }

        /// fill out (complex floating point) with (first, second)
        /// from two real mono images
        void join_to_complex( const image& a, const image& b, image& out ) const
        {
            const pixel_format_info& ainfo{ a.info() };
            const pixel_format_info& binfo{ b.info() };
            if ( !ainfo.is_mono || !binfo.is_mono )
                core::exception_builder<std::runtime_error>()
                    << "transform_real requires greyscale images: " << a.format() << ", " << b.format();

            dispatch_pixel_depth(
                ainfo.depth,
                [&]( auto tag )
                {
                    using V = typename decltype( tag )::type;
                    using PixelT = g<V>;

                    // \a a and \a b may be sub-images, so advance
                    // rows by the pitch
                    const size_t a_stride{ a.pitch() / sizeof( PixelT ) };
                    const size_t b_stride{ b.pitch() / sizeof( PixelT ) };
                    for ( uint32_t h = 0; h < a.height(); ++h )
                    {
                        const PixelT* pa{ a.pixels<PixelT>() + h * a_stride };
                        const PixelT* pb{ b.pixels<PixelT>() + h * b_stride };
                        c_f* po{ out.pixels<c_f>() + h * out.width() };
                        for ( uint32_t w = 0; w < a.width(); ++w, ++pa, ++pb )
                            *po++ = c_f{ static_cast<double>( pa->v ),
                                         static_cast<double>( pb->v ) };
                    }
                } );
        }

    public:
        FFT( const core::size& size )
            : size_(size)
            , forward_scaling_( generate_scaling_factors(size, direction::FORWARD) )
            , reverse_scaling_( generate_scaling_factors(size, direction::REVERSE) )
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
            copy_convert( input, cache().output );

            // iterate over rows first
            image& output{ cache().output };
            for ( uint32_t h = 0; h < output.height(); ++h )
                fft( output.pixels<c_f>() + h * output.width(), output.width(), d );

            // transpose output -> temp
            copy_transpose( cache().output, cache().temp );

            // now do columns
            image& columns{ cache().temp };
            for ( uint32_t h = 0; h < columns.height(); ++h )
                fft( columns.pixels<c_f>() + h * columns.width(), columns.width(), d );

            // flip back: temp -> output
            copy_transpose( cache().temp, cache().output );

            return cache().output;
        }

        /// Perform a 2-D FFT of two real images; will produce two
        /// output (complex floating point) images
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

            // copy data to (real, imag), converting to complex
            join_to_complex( a, b, cache().output );

            // iterate over rows first
            for ( uint32_t h = 0; h < cache().output.height(); ++h )
                fft( cache().output.pixels<c_f>() + h * cache().output.width(),
                     cache().output.width(), d );

            // transpose output -> temp
            copy_transpose( cache().output, cache().temp );

            // now do columns
            for ( uint32_t h = 0; h < cache().temp.height(); ++h )
                fft( cache().temp.pixels<c_f>() + h * cache().temp.width(),
                     cache().temp.width(), d );

            // flip back: temp -> output
            copy_transpose( cache().temp, cache().output );

            // and unravel; all images are full complex images so rows
            // advance by width
            const image& transformed = cache().output;
            image out_a( transformed.size(), pixel_format::COMPLEX_F64 );
            image out_b( transformed.size(), pixel_format::COMPLEX_F64 );

            const auto width = transformed.width();
            const auto height = transformed.height();
            for ( uint32_t h=1; h<height/2; ++h)
            {
                const c_f* src{ transformed.pixels<c_f>() + h*width };
                const c_f* src_r{ transformed.pixels<c_f>() + (height - h)*width };
                c_f* dst_a{ out_a.pixels<c_f>() + h*width };
                c_f* dst_a_r{ out_a.pixels<c_f>() + (height - h)*width };
                c_f* dst_b{ out_b.pixels<c_f>() + h*width };
                c_f* dst_b_r{ out_b.pixels<c_f>() + (height - h)*width };

                for (uint32_t w=1; w<width; ++w)
                {
                    const auto t1 = src[w];
                    const auto t2 = src_r[width - w];
                    auto av = 0.5*(t1 + t2.conj());
                    dst_a[w] = av;
                    dst_a_r[width - w] = av.conj();

                    auto bv = 0.5*(t1 - t2.conj());
                    bv = c_f(bv.imag, -1.0 * bv.real);
                    dst_b[w] = bv;
                    dst_b_r[width - w] = bv.conj();
                }
            }

            return { out_a, out_b };
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
            image a_fft{ fa.clone( image::scratch_allocator() ) };
            const image& fb = transform( b, direction::FORWARD );

            // a_fft = fb * conj(a_fft)
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                const c_f* pb{ fb.pixels<c_f>() + h * fb.width() };
                c_f* pa{ a_fft.pixels<c_f>() + h * a_fft.width() };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = *pb++ * pa->conj();
            }

            image output( size_, a.format(), image::scratch_allocator() );
            copy_real( transform( a_fft, direction::REVERSE ), output );
            swap_quadrants( output );

            return output;
        }

        /// cross-correlate two greyscale images using the real
        /// transform trick
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
                const c_f* pb{ b_fft.pixels<c_f>() + h * b_fft.width() };
                c_f* pa{ a_fft.pixels<c_f>() + h * a_fft.width() };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = *pb++ * pa->conj();
            }

            image output( size_, a.format(), image::scratch_allocator() );
            copy_real( transform( a_fft, direction::REVERSE ), output );
            swap_quadrants( output );

            return output;
        }

        /// auto-correlate an image; the output is a complex image
        /// with zero imaginary part (matching the legacy behaviour)
        image
        auto_correlate( const image& a ) const
        {
            const pixel_format_info& ainfo{ a.info() };
            if ( !ainfo.is_mono && ainfo.family != pixel_family::COMPLEX )
                core::exception_builder<std::runtime_error>()
                    << "auto_correlate requires a mono (greyscale or complex) image: " << a.format();

            image a_fft{ transform( a, direction::FORWARD ).clone( image::scratch_allocator() ) };

            // a_fft = abs_sqr( a_fft )
            for ( uint32_t h = 0; h < a_fft.height(); ++h )
            {
                c_f* pa{ a_fft.pixels<c_f>() + h * a_fft.width() };
                for ( uint32_t w = 0; w < a_fft.width(); ++w, ++pa )
                    *pa = c_f( pa->abs_sqr() );
            }

            const image& fr = transform( a_fft, direction::REVERSE );

            image output( size_, pixel_format::COMPLEX_F64, image::scratch_allocator() );
            for ( uint32_t h = 0; h < output.height(); ++h )
            {
                const c_f* pi{ fr.pixels<c_f>() + h * fr.width() };
                c_f* po{ output.pixels<c_f>() + h * output.width() };
                for ( uint32_t w = 0; w < output.width(); ++w, ++pi )
                    *po++ = c_f( pi->real );
            }
            swap_quadrants( output );

            return output;
        }

    private:
        void fft_inner( c_f* in, c_f* out, const c_f* scaling, size_t n, size_t step ) const
        {
            DECLARE_ENTRY_EXIT

            if (step < n)
            {
                const size_t doublestep{ 2*step };
                fft_inner(out,        in,        scaling, n, doublestep);
                fft_inner(out + step, in + step, scaling, n, doublestep);

                for ( size_t i=0; i<n; i+=doublestep )
                {
                    const c_f e{ out[i] };
                    const c_f o{ out[i + step] * scaling[i] };
                    in[ i/2 ]      = e + o;
                    in[ (i+n)/2 ]  = e - o;
                }
            }
        }

        void fft( c_f* in, size_t n, direction d, size_t stride = 1 ) const
        {
            DECLARE_ENTRY_EXIT

            typed_memcpy( cache().fft_buffer.data(), in, n, stride );
            const auto& scaling = (d == direction::FORWARD ? forward_scaling_ : reverse_scaling_).at(n);
            fft_inner( in, cache().fft_buffer.data(), &scaling[0], n, 1 );
        }

        static scaling_map_t generate_scaling_factors( const core::size& size, direction d )
        {
            scaling_map_t result;
            size_t n = maximal_size( size ).width();
            do {
                const double scaling{ d == direction::FORWARD ? -1.0 : 1.0 };
                std::vector<c_f> twiddle(n);
                std::generate(
                    std::begin(twiddle), std::end(twiddle),
                    [i=0, n, scaling]() mutable {
                        const double theta = (scaling * M_PI * i++)/n;
                        return c_f{ std::cos( theta ), std::sin( theta ) };
                    } );

                result[n] = twiddle;
                n /= 2;
            } while (n > 2);

            return result;
        }
    };

}
