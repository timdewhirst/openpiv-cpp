#pragma once

// std
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

// local
#include "core/image.h"

namespace openpiv::core {

    class image_loader_exception : public std::runtime_error
    {
    public:
        image_loader_exception( const std::string& what )
            : std::runtime_error( what )
        {}
    };

    class image_loader;
    using image_loader_ptr_t = std::unique_ptr<image_loader>;

    /// central repository for registered image loaders
    class image_loader_registry
    {
    public:
        static image_loader_registry& instance();

        /// find a loader for an image; if no ImageLoader is returned
        /// then this image is unhandled; the image loader returned is owned
        /// by the caller
        image_loader_ptr_t find( std::istream& );

        /// find a loader by name for saving/forced loading
        image_loader_ptr_t find( const std::string& );

        /// register a loader; done this way to avoid the provider knowing
        /// anything about the allocation/ownership of the loader.
        template < typename T >
        typename std::enable_if_t<
            std::is_base_of_v< image_loader, T >,
            bool >
        add()
        {
            return add( std::make_unique<T>() );
        }

    private:
        bool add( image_loader_ptr_t&& );
    };

    /// interface for ImageLoaders; the std::istream passed should be at
    /// the start of the available data i.e. start of file or received
    /// data.
    ///
    /// image_loader may be stateful: if an image format supports
    /// multiple images then the loader may store e.g. dimensional
    /// information along with offsets to the contained images.
    ///
    /// A call to load() or open() must reset the state held within
    /// a loader.
    ///
    /// The format of an extracted image is determined by the
    /// contents of the file; use image utilities to convert to a
    /// required format. Similarly, saving accepts any image, with
    /// the supported formats determined by the file format - an
    /// unsupported image format is a runtime error.
    class image_loader
    {
    public:
        virtual ~image_loader() {}

        /// return the name of this loader: MIME type
        /// \todo: change to have a list of supported
        /// MIME types
        virtual std::string name() const = 0;

        /// return the priority of this loader
        virtual int priority() const = 0;

        /// polymorphic clone
        virtual image_loader_ptr_t clone() const = 0;

        /// \return true if this loader can load the data found in \a stream
        virtual bool can_load( std::istream& ) const = 0;

        /// \return true if this loader can save data
        virtual bool can_save() const = 0;

        /// \return number of images contained within this file
        virtual size_t num_images() const = 0;

        /// Open an image file for reading, usually reading
        /// any header data.
        /// \return true if \a is contains valid image data
        virtual bool open( std::istream& is ) = 0;

        /// Extract an image from an opened stream; the format of
        /// the resulting image is determined by the contents of the
        /// file
        /// \return the image, or an empty optional if \a index is
        /// out of range or the file variant is unsupported
        virtual std::optional<image> extract( size_t index ) = 0;

        /// Load the image from \a stream; this is a convenience method
        /// that combines open/extract for the first image found
        /// \return the image, or an empty optional on failure
        virtual std::optional<image> load( std::istream& is )
        {
            if ( !open( is ) )
                return std::nullopt;
            return extract( 0 );
        }

        /// Save the image to \a stream
        /// may throw ImageLoaderException if there is an issue
        virtual void save( std::ostream&, const image& im ) const = 0;
    };

}
