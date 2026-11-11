#pragma once

// std
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <ostream>
#include <tuple>
#include <utility>

// local
#include "core/exception_builder.h"
#include "core/format_utils.h"
#include "core/pixel_format.h"
#include "core/pixel_types.h"
#include "core/point.h"
#include "core/rect.h"
#include "core/size.h"

namespace openpiv::core {

/// basic 2-dimensional image.
///
/// an image does not own its data: storage is acquired from an external
/// allocator and referenced via std::shared_ptr. sharing is central to
/// the design - copying an image is a shallow, view-like operation and
/// images may alias one another via sub_image().
///
/// an image is fixed in size and format at construction; there is no
/// resize. to change size, construct a new image; to change format, use
/// convert().
class image
{
public:
    using byte_t = uint8_t;
    using index_t = size_t;

    /// storage is provided by an external allocator which is handed a
    /// byte count and must return a reference-counted buffer; the
    /// allocator is responsible for how the data is constructed and
    /// (once the last reference is dropped) released
    using allocator_t = std::function< std::shared_ptr<byte_t>( size_t ) >;

    /// default allocator: zero-initialized heap storage
    static allocator_t default_allocator()
    {
        return []( size_t bytes ) -> std::shared_ptr<byte_t>
        {
            auto storage = std::make_shared<std::vector<byte_t>>( bytes, byte_t{0} );
            return { storage, storage->data() };
        };
    }

    /// allocator for scratch images: the returned storage is
    /// uninitialised. Use only for intermediates that are fully
    /// written before being read; sharing semantics are unchanged
    static allocator_t scratch_allocator()
    {
        return []( size_t bytes ) -> std::shared_ptr<byte_t>
        {
            return { new byte_t[ bytes ], []( byte_t* p ) { delete[] p; } };
        };
    }

    // ctor
    /// images are fixed in geometry and format at construction;
    /// there is no empty image
    image() = delete;
    image( const image& ) = default;
    image( image&& ) = default;
    image& operator=( const image& ) = default;
    image& operator=( image&& ) = default;

    /// new image of the given format; the image is zero-initialized
    /// note: \a r may be positioned anywhere; the position is carried
    /// by rect() and is used when placing sub-images and extractions
    image( uint32_t w, uint32_t h, pixel_format fmt,
           const allocator_t& allocator = default_allocator() )
        : image( core::rect::from_size( core::size{ w, h } ), fmt, allocator )
    {}
    image( const core::size& s, pixel_format fmt,
           const allocator_t& allocator = default_allocator() )
        : image( core::rect::from_size( s ), fmt, allocator )
    {}
    image( const core::rect& r, pixel_format fmt,
           const allocator_t& allocator = default_allocator() )
    {
        if ( fmt == pixel_format::UNKNOWN )
            core::exception_builder<std::invalid_argument>()
                << "pixel format must be known: " << fmt;

        const pixel_format_info& fi{ core::format_info( fmt ) };
        r_ = r;
        fmt_ = fmt;
        pitch_ = static_cast<size_t>( r.width() ) * fi.bytes_per_pixel;
        data_ = allocator( static_cast<size_t>( r.area() ) * fi.bytes_per_pixel );
    }

    /// construct a sub-image sharing the underlying data; \a r is in
    /// the local coordinates of this image and must be wholly
    /// contained within it; the result has the same format and row
    /// pitch as this image and its rect() is positioned relative to
    /// this image's rect()
    image sub_image( const core::rect& r ) const
    {
        require_sub_rect( r );

        image result{ { { r_.left() + r.left(), r_.bottom() + r.bottom() }, r.size() },
                      fmt_,
                      pitch_,
                      { data_, data_.get() + offset_of( r ) } };
        return result;
    }

    /// construct a deep copy of a region; \a r is in the local
    /// coordinates of this image and must be wholly contained within
    /// it; the result uses fresh storage and its rect() is positioned
    /// as per sub_image()
    image extract( const core::rect& r, const allocator_t& allocator = default_allocator() ) const
    {
        require_sub_rect( r );

        image result{ core::rect{ { r_.left() + r.left(), r_.bottom() + r.bottom() }, r.size() }, fmt_, allocator };
        const size_t row_bytes{ static_cast<size_t>( r.width() ) * info().bytes_per_pixel };
        const byte_t* src{ data_.get() + offset_of( r ) };
        byte_t* dst{ result.data_.get() };
        for ( uint32_t row = 0; row < static_cast<uint32_t>( r.height() ); ++row, src += pitch_, dst += result.pitch_ )
            std::memcpy( dst, src, row_bytes );

        return result;
    }

    /// full deep copy
    image clone( const allocator_t& allocator = default_allocator() ) const
    {
        return extract( core::rect::from_size( r_.size() ), allocator );
    }

    /// equality; images are equal if they have the same geometry,
    /// format and content - the underlying storage may differ
    inline bool operator==( const image& rhs ) const
    {
        if ( r_ != rhs.r_ || fmt_ != rhs.fmt_ )
            return false;

        // compare rows: pitch may differ between shared views
        const size_t row_bytes{ static_cast<size_t>( r_.width() ) * info().bytes_per_pixel };
        const byte_t* l{ data_.get() };
        const byte_t* r{ rhs.data_.get() };
        for ( uint32_t row = 0; row < static_cast<uint32_t>( r_.height() ); ++row, l += pitch_, r += rhs.pitch_ )
            if ( std::memcmp( l, r, row_bytes ) != 0 )
                return false;

        return true;
    }
    inline bool operator!=( const image& rhs ) const { return !operator==( rhs ); }

    /// pixel accessor with runtime-checked pixel type; \a i is a
    /// linear index: x = i % width(), y = i / width()
    template <typename PixelT>
    PixelT& at( index_t i )
    {
        if ( format_for_v<PixelT> != fmt_ )
            throw_pixel_type_mismatch<PixelT>();
        if ( i >= pixel_count() )
            throw_pixel_index_out_of_range( i );

        const pixel_format_info& fi{ info() };
        const size_t x{ i % width() };
        const size_t y{ i / width() };
        return *reinterpret_cast<PixelT*>( data_.get() + y * pitch_ + x * fi.bytes_per_pixel );
    }
    template <typename PixelT>
    const PixelT& at( index_t i ) const { return const_cast<image*>(this)->at<PixelT>( i ); }

    /// pixel accessor with runtime-checked pixel type; \a xy is in
    /// local coordinates
    template <typename PixelT>
    PixelT& at( const point2<uint32_t>& xy )
    {
        if ( format_for_v<PixelT> != fmt_ )
            throw_pixel_type_mismatch<PixelT>();
        if ( xy[0] >= width() || xy[1] >= height() )
            throw_point_out_of_range( xy );

        const pixel_format_info& fi{ info() };
        return *reinterpret_cast<PixelT*>( data_.get() + static_cast<size_t>(xy[1]) * pitch_ + static_cast<size_t>(xy[0]) * fi.bytes_per_pixel );
    }
    template <typename PixelT>
    const PixelT& at( const point2<uint32_t>& xy ) const { return const_cast<image*>(this)->at<PixelT>( xy ); }

    /// raw pixel data as a typed pointer; the pixel type is
    /// runtime-checked; pixel data is only packed for full images -
    /// for sub-images use at() or line()
    template <typename PixelT>
    PixelT* pixels()
    {
        check_pixel_type<PixelT>();
        return reinterpret_cast<PixelT*>( data_.get() );
    }
    template <typename PixelT>
    const PixelT* pixels() const { return const_cast<image*>(this)->pixels<PixelT>(); }

    /// raw data accessor
    inline byte_t* data() { return data_.get(); }
    inline const byte_t* data() const { return const_cast<image*>(this)->data(); }

    /// raw data by line
    inline byte_t* line( size_t i )
    {
        if ( i > r_.height() )
            core::exception_builder<std::range_error>()
                << "line out of range (" << i << ", max is: " << r_.height() << ")";

        return data_.get() + i * pitch_;
    }
    inline const byte_t* line( size_t i ) const { return const_cast<image*>(this)->line( i ); }

    /// geometry accessors
    inline uint32_t width() const { return r_.width(); }
    inline uint32_t height() const { return r_.height(); }
    inline core::size size() const { return r_.size(); }
    inline index_t pixel_count() const { return r_.area(); }
    inline core::rect rect() const { return r_; }

    /// format accessors
    inline pixel_format format() const { return fmt_; }
    inline pixel_format_info info() const
    {
        return core::format_info( fmt_ );
    }

    /// distance between pixel components (x direction) and rows (y
    /// direction), in bytes; row pitch is preserved by sub_image
    inline std::tuple<size_t, size_t> stride() const
    {
        return { info().bytes_per_pixel, pitch_ };
    }
    inline size_t pitch() const { return pitch_; }

    /// swap
    void swap( image& rhs )
    {
        std::swap( r_, rhs.r_ );
        std::swap( fmt_, rhs.fmt_ );
        std::swap( pitch_, rhs.pitch_ );
        std::swap( data_, rhs.data_ );
    }

private:
    /// construct an image over existing storage; for internal use
    /// (sub_image) - does not allocate
    image( const core::rect& r, pixel_format fmt, size_t pitch, std::shared_ptr<byte_t> data )
        : r_( r )
        , fmt_( fmt )
        , pitch_( pitch )
        , data_( std::move( data ) )
    {}

    /// byte offset of the first pixel of \a r (local coordinates)
    size_t offset_of( const core::rect& r ) const
    {
        return static_cast<size_t>( r.bottom() ) * pitch_ +
               static_cast<size_t>( r.left() ) * info().bytes_per_pixel;
    }

    /// ensure \a r (local coordinates) is valid for sub_image/extract
    void require_sub_rect( const core::rect& r ) const
    {
        const core::rect bounds{ core::rect::from_size( r_.size() ) };
        if ( !bounds.contains( r ) )
            core::exception_builder<std::out_of_range>()
                << "rect (" << r << ") not contained within image (" << bounds << ")";
    }

    template <typename PixelT>
    void check_pixel_type() const
    {
        if ( format_for_v<PixelT> != fmt_ )
            throw_pixel_type_mismatch<PixelT>();
    }

    /// error paths are isolated into cold, out-of-line functions:
    /// their bodies construct formatted exception messages which
    /// would otherwise prevent at() and friends from being inlined
    /// into callers
    template <typename PixelT>
    [[gnu::cold, gnu::noinline]]
    void throw_pixel_type_mismatch() const
    {
        core::exception_builder<std::runtime_error>()
            << "pixel type mismatch: " << pixeltype_name<PixelT>()
            << " does not match " << fmt_;
    }

    [[gnu::cold, gnu::noinline]]
    void throw_pixel_index_out_of_range( index_t i ) const
    {
        core::exception_builder<std::out_of_range>()
            << "pixel index out of range: " << i << ", max is: " << pixel_count();
    }

    [[gnu::cold, gnu::noinline]]
    void throw_point_out_of_range( const point2<uint32_t>& xy ) const
    {
        core::exception_builder<std::out_of_range>()
            << "point outside of image: [" << xy[0] << ", " << xy[1] << "], image is: "
            << width() << "x" << height();
    }

    core::rect r_{};
    pixel_format fmt_{ pixel_format::UNKNOWN };
    size_t pitch_{ 0 };
    std::shared_ptr<byte_t> data_;
};

/// swap
inline void swap( image& lhs, image& rhs )
{
    lhs.swap( rhs );
}


/// ostream operator
inline std::ostream& operator<<( std::ostream& os, const image& im )
{
    os << "image[" << im.format() << "][" << im.rect() << "]"
       << "[pitch: " << im.pitch() << "]"
       << "[data @ " << static_cast<const void*>( im.data() ) << "]";
    return os;
}

}

template <>
struct std::formatter<openpiv::core::image> : openpiv::core::StreamableFormatter<openpiv::core::image>
{};
