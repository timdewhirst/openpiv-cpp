#include "loaders/tiff_image_loader.h"

// std
#include <array>
#include <istream>
#include <memory>
#include <vector>

// libtiff
#include <tiffio.h>
#include <tiffio.hxx>

// local
#include "core/util.h"
#include "core/image.h"
#include "core/log.h"
#include "core/enum_helper.h"


namespace {

    using namespace openpiv;
    using namespace openpiv::core;
    namespace logger = openpiv::core::logger;

    enum class planar_config : uint16_t {
        UNDEFINED = 0,
        CONTIG    = 1,
        SEPARATE  = 2
    };
    DECLARE_ENUM_HELPER( planar_config, {
            { planar_config::UNDEFINED, "undefined" },
            { planar_config::CONTIG,    "contiguous" },
            { planar_config::SEPARATE,  "separate" }
        } )

    enum class sample_format : uint16_t {
        UNDEFINED     = 0,
        UINT          = 1,
        INT           = 2,
        IEEEFP        = 3,
        VOID          = 4,
        COMPLEXINT    = 5,
        COMPLEXIEEEFP = 6
    };
    DECLARE_ENUM_HELPER( sample_format, {
            { sample_format::UNDEFINED,     "undefined" },
            { sample_format::UINT,          "unsigned int" },
            { sample_format::INT,           "int" },
            { sample_format::IEEEFP,        "floating point" },
            { sample_format::VOID,          "void" },
            { sample_format::COMPLEXINT,    "complex int" },
            { sample_format::COMPLEXIEEEFP, "complex floating point" }
        } )
}


namespace openpiv::core {

    struct tiff_image_loader::impl
    {
        std::istream& is;
        std::unique_ptr<TIFF, decltype(&TIFFClose)> tiff;
        uint32_t width = 0;
        uint32_t height = 0;
        uint16_t bps = 0;
        uint16_t spp = 0;
        sample_format format = sample_format::UNDEFINED;
        planar_config planar = planar_config::UNDEFINED;
        size_t num_images = 0;

        impl( std::istream& is_ )
            : is(is_)
            , tiff( nullptr, &TIFFClose )
        {}

        bool open()
        {
            tiff = std::unique_ptr<TIFF, decltype(&TIFFClose)>( TIFFStreamOpen( "sbTIFF", &is ), &TIFFClose );

            if ( !tiff )
            {
                logger::error("failed to open TIFF for reading\n");
                return false;
            }

            TIFF* tiff_ = tiff.get();

            // Reading the data from the tiff file's header; N.B. this is the
            // first image's dimensions
            TIFFGetField( tiff_, TIFFTAG_IMAGEWIDTH,      &width );
            TIFFGetField( tiff_, TIFFTAG_IMAGELENGTH,     &height );
            TIFFGetField( tiff_, TIFFTAG_BITSPERSAMPLE,   &bps );
            TIFFGetField( tiff_, TIFFTAG_SAMPLESPERPIXEL, &spp );
            TIFFGetField( tiff_, TIFFTAG_SAMPLEFORMAT,    &format );
            TIFFGetField( tiff_, TIFFTAG_PLANARCONFIG,    &planar );

            // find number of images
            do {
                ++num_images;
            } while (TIFFReadDirectory(tiff_));

            logger::debug(
                "tiff info: {} x {}, bps: {}, spp: {}, format: {}, planar: {}, num: {}",
                width, height,
                bps,
                spp,
                to_string(format),
                to_string(planar),
                num_images);

            // ... and reset current directory
            TIFFSetDirectory( tiff_, 0 );

            return true;
        }

        /// extract image #index; the format of the resulting image
        /// is determined by the file contents: 8 or 16 bit unsigned
        /// samples; 1 sample per pixel (grey) or 3 contiguous
        /// samples per pixel (RGB); libtiff renders sample data in
        /// machine byte order
        bool extract( size_t index, image& im ) const
        {
            if ( !tiff )
                return false;

            if ( !(spp == 1 || spp == 3) )
            {
                logger::error("images with spp != 1 or 3 not yet supported: ({})", spp);
                return false;
            }

            if ( num_images <= index )
            {
                logger::error("image index out of range (index: {}, num_images: {}", index, num_images);
                return false;
            }

            if ( !(bps == 8 || bps == 16) ||
                 format == sample_format::IEEEFP ||
                 format == sample_format::COMPLEXIEEEFP ||
                 planar == planar_config::SEPARATE )
            {
                logger::error("unsupported tiff: bps: {}, format: {}, planar: {}",
                              bps, to_string(format), to_string(planar));
                return false;
            }

            // get correct image
            if ( TIFFSetDirectory( tiff.get(), static_cast<uint16_t>( index ) ) == 0 )
            {
                logger::error("failed to set directory to: {}", index);
                return false;
            }

            const pixel_format im_format{
                spp == 1
                    ? (bps == 8 ? pixel_format::GREY_U8 : pixel_format::GREY_U16)
                    : (bps == 8 ? pixel_format::RGB_U8 : pixel_format::RGB_U16) };
            im = image( width, height, im_format );

            // per-line reads; scanline size matches image pitch
            std::vector<uint8_t> buffer( TIFFScanlineSize( tiff.get() ) );
            for ( uint32_t h = 0; h < height; ++h )
            {
                TIFFReadScanline( tiff.get(), buffer.data(), h, 0 );
                std::memcpy( im.line( h ), buffer.data(), im.pitch() );
            }

            return true;
        }
    };

    //
    //
    //
    tiff_image_loader::tiff_image_loader()
    {}

    tiff_image_loader::~tiff_image_loader()
    {}

    std::string tiff_image_loader::name() const
    {
        return "image/tiff";
    }

    int tiff_image_loader::priority() const
    {
        return 1;
    }

    image_loader_ptr_t tiff_image_loader::clone() const
    {
        return std::make_unique<tiff_image_loader>();
    }

    bool tiff_image_loader::can_load( std::istream& is ) const
    {
        if ( !is.good() )
            exception_builder<image_loader_exception>() << "input stream is not ready for reading";

        // record position
        peeker peek(is);

        // fetch header data
        std::array<char, 4> header;
        is.read(&header[0], sizeof(header));
        if ( !is.good() )
            exception_builder<image_loader_exception>() << "input stream doesn't contain enough data";

        static const std::array<char, 4> LE_HEADER{ { 0x49, 0x49, 0x2a, 0x00 } };
        static const std::array<char, 4> BE_HEADER{ { 0x4d, 0x4d, 0x00, 0x2a } };

        if ( header == LE_HEADER || header == BE_HEADER )
            return true;

        return false;
    }

    bool tiff_image_loader::can_save() const
    {
        return false;
    }

    size_t tiff_image_loader::num_images() const
    {
        if ( !impl_ || !impl_->tiff )
            return 0;

        return impl_->num_images;
    }

    bool tiff_image_loader::open( std::istream& is )
    {
        impl_ = std::make_unique<impl>(is);
        return impl_->open();
    }

    bool tiff_image_loader::extract( size_t index, image& im )
    {
        if ( !impl_ )
            return false;

        return impl_->extract( index, im );
    }

    void tiff_image_loader::save( std::ostream&, const image& ) const
    {
        exception_builder<image_loader_exception>() << name() << ": cannot save data";
    }

}
