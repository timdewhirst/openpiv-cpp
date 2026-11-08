#include "loaders/pnm_image_loader.h"

// std
#include <array>
#include <istream>
#include <ostream>
#include <vector>

// local
#include "core/image.h"
#include "core/log.h"
#include "core/util.h"
#include "algos/stats.h"

namespace {

    using namespace openpiv;
    using namespace openpiv::core;

    template <int> constexpr uint16_t stobe( uint16_t v );
    template <>
    [[maybe_unused]]
    inline constexpr uint16_t stobe<__ORDER_BIG_ENDIAN__>( uint16_t v )
    {
        return v;
    }

    template <>
    inline constexpr uint16_t stobe<__ORDER_LITTLE_ENDIAN__>( uint16_t v )
    {
        return (v>>8) | (v<<8);
    }

    template <int E> inline constexpr uint16_t betos( uint16_t v )
    {
        return stobe<E>(v);
    }

    /// support chomping comment lines
    struct comment_tag {};
    comment_tag comment() { return {}; }
    std::istream& operator>>( std::istream& is, const comment_tag& )
    {
        while ( is.peek() == '#' )
            is.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

        return is;
    }

    /// extract header info along with a flag to
    /// indicate if the header is valid
    std::tuple<bool, uint8_t> get_header_info( char* b )
    {
        bool result = (b[0] == 'P' && (b[1] == '5' || b[1] == '6'));
        return std::tuple<bool, uint8_t>{ result, b[1] - '0' };
    }

    /// check if \a b contains "P5" or "P6"; ignore binary pbm images
    bool is_supported( char* b )
    {
        bool result;
        std::tie( result, std::ignore ) = get_header_info( b );

        return result;
    }
}

namespace openpiv::core {

    ///
    struct pnm_image_loader::impl
    {
        bool valid      = false;
        uint8_t type    = 0;
        uint32_t width  = 0;
        uint32_t height = 0;
        uint16_t depth  = 0;
        std::istream& is;

        impl( std::istream& is_ )
            : is(is_)
        {}

        /// \a is must be placed after header data; the format of the
        /// resulting image is determined by the PNM type and depth
        void extract( image& im ) const
        {
            if ( type == 5 )
            {
                // binary greyscale
                im = image( width, height, depth > 255 ? pixel_format::GREY_U16 : pixel_format::GREY_U8 );

                for ( uint32_t h = 0; !is.eof() && h < height; ++h )
                {
                    is.read( reinterpret_cast<char*>( im.line( h ) ), im.pitch() );

                    if ( im.format() == pixel_format::GREY_U16 )
                    {
                        // from (big-endian) file to machine order
                        uint16_t* line{ reinterpret_cast<uint16_t*>( im.line( h ) ) };
                        for ( uint32_t w = 0; w < width; ++w )
                            line[w] = betos<__BYTE_ORDER__>( line[w] );
                    }
                }
            }
            else
            {
                // binary RGB
                im = image( width, height, depth > 255 ? pixel_format::RGB_U16 : pixel_format::RGB_U8 );

                for ( uint32_t h = 0; !is.eof() && h < height; ++h )
                {
                    is.read( reinterpret_cast<char*>( im.line( h ) ), im.pitch() );

                    if ( im.format() == pixel_format::RGB_U16 )
                    {
                        uint16_t* line{ reinterpret_cast<uint16_t*>( im.line( h ) ) };
                        for ( uint32_t w = 0; w < 3*width; ++w )
                            line[w] = betos<__BYTE_ORDER__>( line[w] );
                    }
                }
            }
        }
    };

    ///
    pnm_image_loader::pnm_image_loader()
    {}

    pnm_image_loader::~pnm_image_loader()
    {}

    std::string pnm_image_loader::name() const
    {
        return "image/x-portable-anymap";
    }

    int pnm_image_loader::priority() const
    {
        return 1;
    }

    image_loader_ptr_t pnm_image_loader::clone() const
    {
        return std::make_unique<pnm_image_loader>();
    }

    bool pnm_image_loader::can_load( std::istream& is ) const
    {
        if ( !is.good() )
            exception_builder<image_loader_exception>() << "input stream is not ready for reading";

        // record position
        peeker peek(is);

        // fetch header data
        std::array<char, 2> header;
        is.read(&header[0], sizeof(header));
        if ( !is.good() )
            exception_builder<image_loader_exception>() << "input stream doesn't contain enough data";

        if ( is_supported(&header[0]) )
            return true;

        return false;
    }

    bool pnm_image_loader::can_save() const
    {
        return true;
    }

    size_t pnm_image_loader::num_images() const
    {
        /// PNM files only ever contain a single image
        return 1;
    }

    bool pnm_image_loader::open( std::istream& is )
    {
        impl_ = std::make_unique<impl>(is);

        // PPM header is very simple:
        // P6
        // # CREATOR: GIMP PNM Filter Version 1.1
        // 1920 1080
        // 255
        char buffer[ 1024 ];
        impl_->is.getline( &buffer[0], sizeof( buffer ) );

        std::tie( impl_->valid, impl_->type ) = get_header_info( buffer );

        if ( !impl_->valid )
        {
            logger::error("image type not supported: {}", std::string( buffer, 2 ) );
            return false;
        }

        // get dimensions & depth
        impl_->is >> comment() >> impl_->width
                  >> comment() >> impl_->height
                  >> comment() >> impl_->depth;
        // ensure we chomp any remaining whitespace to EOL
        impl_->is.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );

        logger::debug("PNM{} [{}, {}], {}", impl_->type, impl_->width, impl_->height, impl_->depth);
        return true;
    }

    bool pnm_image_loader::extract( size_t, image& im )
    {
        if ( !impl_ || !impl_->valid )
            return false;

        impl_->extract( im );
        return true;
    }

    namespace {

        /// write a 16-bit sample directly, with scaling for
        /// floating point data
        inline uint16_t save_sample( double value, double min, double range )
        {
            if ( range == 0.0 )
                return 0;

            // values in [0, 65535]
            const double scaled{ 65535.0 * ( ( value - min ) / range ) };
            if ( scaled <= 0.0 )
                return 0;
            if ( scaled >= 65535.0 )
                return 65535;

            return static_cast<uint16_t>( scaled );
        }

    }

    void pnm_image_loader::save( std::ostream& os, const image& im ) const
    {
        const pixel_format_info& info{ im.info() };

        if ( im.data() == nullptr )
            exception_builder<image_loader_exception>() << "cannot save empty image";

        if ( info.is_mono )
        {
            // PG5: greyscale, 16-bit samples
            auto [min, max] = algos::find_image_range( im );
            double range{ max - min };

            os << "P5\n"
               << "# created by pnm_image_loader\n"
               << im.width() << " " << im.height() << "\n"
               << "65535\n";

            dispatch_pixel_depth(
                info.depth,
                [&os, &im, min, range]( auto tag )
                {
                    using V = typename decltype( tag )::type;
                    std::vector<uint16_t> buffer( im.width() );
                    for ( uint32_t h = 0; h < im.height(); ++h )
                    {
                        const V* p{ reinterpret_cast<const V*>( im.line( h ) ) };
                        for ( uint32_t w = 0; w < im.width(); ++w, ++p )
                        {
                            if constexpr ( std::is_integral_v<V> )
                            {
                                // integral values are written as-is
                                buffer[w] = stobe<__BYTE_ORDER__>( static_cast<uint16_t>( *p ) );
                            }
                            else
                            {
                                // floating point values are scaled to [0, 65535]
                                buffer[w] = stobe<__BYTE_ORDER__>(
                                    save_sample( static_cast<double>( *p ), min, range ) );
                            }
                        }
                        os.write( reinterpret_cast<const char*>( buffer.data() ),
                                  buffer.size() * sizeof(uint16_t) );
                    }
                } );

            os.flush();
            return;
        }

        if ( info.family == pixel_family::RGB ||
             info.family == pixel_family::RGBA )
        {
            // P6: RGB, 16-bit per sample; alpha (if present) is dropped
            os << "P6\n"
               << "# created by pnm_image_loader\n"
               << im.width() << " " << im.height() << "\n"
               << "65535\n";

            dispatch_pixel_depth(
                info.depth,
                [&os, &im, &info]( auto tag )
                {
                    using V = typename decltype( tag )::type;
                    const auto channels{ info.channels };
                    std::vector<uint16_t> buffer( 3 * im.width() );

                    // floating point images are scaled to the full range
                    double min{ 0.0 };
                    double range{ 1.0 };
                    if constexpr ( !std::is_integral_v<V> )
                    {
                        min = std::numeric_limits<double>::max();
                        double max{ std::numeric_limits<double>::lowest() };
                        for ( uint32_t h = 0; h < im.height(); ++h )
                        {
                            const V* p{ reinterpret_cast<const V*>( im.line( h ) ) };
                            for ( size_t i = 0; i < channels * im.width(); ++i, ++p )
                            {
                                min = std::min( min, static_cast<double>( *p ) );
                                max = std::max( max, static_cast<double>( *p ) );
                            }
                        }
                        range = max - min;
                    }

                    for ( uint32_t h = 0; h < im.height(); ++h )
                    {
                        const V* p{ reinterpret_cast<const V*>( im.line( h ) ) };
                        for ( uint32_t w = 0; w < im.width(); ++w, p += channels )
                        {
                            for ( size_t c = 0; c < 3; ++c )
                            {
                                if constexpr ( std::is_integral_v<V> )
                                {
                                    buffer[ 3*w + c ] =
                                        stobe<__BYTE_ORDER__>( static_cast<uint16_t>( p[c] ) );
                                }
                                else
                                {
                                    buffer[ 3*w + c ] = stobe<__BYTE_ORDER__>(
                                        save_sample( static_cast<double>( p[c] ), min, range ) );
                                }
                            }
                        }
                        os.write( reinterpret_cast<const char*>( buffer.data() ),
                                  buffer.size() * sizeof(uint16_t) );
                    }
                } );

            os.flush();
            return;
        }

        exception_builder<image_loader_exception>()
            << "pnm_image_loader: unsupported image format for saving: " << im.format();
    }

}
