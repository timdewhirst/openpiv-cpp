// openpiv
#include "core/image.h"
#include "core/pixel_format.h"

using namespace openpiv;
using namespace openpiv::core;

// local
#include "pyutils.h"

// std
#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

// pybind
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

namespace py = pybind11;

namespace {

    std::string buffer_format_for_depth( pixel_depth d )
    {
        switch ( d )
        {
            case pixel_depth::U8: return "B";
            case pixel_depth::U16: return "H";
            case pixel_depth::U32: return "I";
            case pixel_depth::F32: return "f";
            case pixel_depth::F64: return "d";
            default:
                core::exception_builder<std::runtime_error>()
                    << "unknown pixel depth: " << static_cast<uint8_t>( d );
                std::unreachable();
        }
    }

}

void add_image(py::module& m)
{
    // pixel formats as a python enum; names match the C++ enum
    // e.g. pyopenpivcore.pixel_format.GREY_U8
    py::enum_<pixel_family>( m, "pixel_family" )
        .value( "UNKNOWN", pixel_family::UNKNOWN )
        .value( "GREY",    pixel_family::GREY )
        .value( "RGB",     pixel_family::RGB )
        .value( "RGBA",    pixel_family::RGBA )
        .value( "YUV",     pixel_family::YUV )
        .value( "YUVA",    pixel_family::YUVA )
        .value( "COMPLEX", pixel_family::COMPLEX )
        .export_values();

    py::enum_<pixel_depth>( m, "pixel_depth" )
        .value( "UNKNOWN", pixel_depth::UNKNOWN )
        .value( "U8",  pixel_depth::U8 )
        .value( "U16", pixel_depth::U16 )
        .value( "U32", pixel_depth::U32 )
        .value( "F32", pixel_depth::F32 )
        .value( "F64", pixel_depth::F64 )
        .export_values();

    py::enum_<pixel_format>( m, "pixel_format" )
        .value( "UNKNOWN", pixel_format::UNKNOWN )
        .value( "GREY_U8",       pixel_format::GREY_U8 )
        .value( "GREY_U16",      pixel_format::GREY_U16 )
        .value( "GREY_U32",      pixel_format::GREY_U32 )
        .value( "GREY_F32",      pixel_format::GREY_F32 )
        .value( "GREY_F64",      pixel_format::GREY_F64 )
        .value( "RGB_U8",        pixel_format::RGB_U8 )
        .value( "RGB_U16",       pixel_format::RGB_U16 )
        .value( "RGB_U32",       pixel_format::RGB_U32 )
        .value( "RGB_F32",       pixel_format::RGB_F32 )
        .value( "RGB_F64",       pixel_format::RGB_F64 )
        .value( "RGBA_U8",       pixel_format::RGBA_U8 )
        .value( "RGBA_U16",      pixel_format::RGBA_U16 )
        .value( "RGBA_U32",      pixel_format::RGBA_U32 )
        .value( "RGBA_F32",      pixel_format::RGBA_F32 )
        .value( "RGBA_F64",      pixel_format::RGBA_F64 )
        .value( "YUV_U8",        pixel_format::YUV_U8 )
        .value( "YUV_U16",       pixel_format::YUV_U16 )
        .value( "YUV_U32",       pixel_format::YUV_U32 )
        .value( "YUV_F32",       pixel_format::YUV_F32 )
        .value( "YUV_F64",       pixel_format::YUV_F64 )
        .value( "YUVA_U8",       pixel_format::YUVA_U8 )
        .value( "YUVA_U16",      pixel_format::YUVA_U16 )
        .value( "YUVA_U32",      pixel_format::YUVA_U32 )
        .value( "YUVA_F32",      pixel_format::YUVA_F32 )
        .value( "YUVA_F64",      pixel_format::YUVA_F64 )
        .value( "COMPLEX_U8",    pixel_format::COMPLEX_U8 )
        .value( "COMPLEX_U16",   pixel_format::COMPLEX_U16 )
        .value( "COMPLEX_U32",   pixel_format::COMPLEX_U32 )
        .value( "COMPLEX_F32",   pixel_format::COMPLEX_F32 )
        .value( "COMPLEX_F64",   pixel_format::COMPLEX_F64 )
        .export_values();

    // format description
    py::class_<pixel_format_info>( m, "pixel_format_info" )
        .def_property_readonly( "name",
            []( const pixel_format_info& fi ) { return std::string( fi.name ); } )
        .def_readonly( "family", &pixel_format_info::family )
        .def_readonly( "depth", &pixel_format_info::depth )
        .def_readonly( "channels", &pixel_format_info::channels )
        .def_readonly( "bytes_per_pixel", &pixel_format_info::bytes_per_pixel )
        .def_readonly( "is_mono", &pixel_format_info::is_mono )
        .def_readonly( "is_complex", &pixel_format_info::is_complex )
        .def( "__repr__",
            []( const pixel_format_info& fi ) { return std::string( fi.name ); } );

    m.def( "format_info", &core::format_info );

    // single runtime-typed image class
    auto to_str = [](const image& im) {
        std::ostringstream ss;
        ss << im;
        return ss.str();
    };

    py::class_<image>( m, "image", py::buffer_protocol() )
        .def_buffer( []( image& im ) -> py::buffer_info {
            const pixel_format_info& fi{ im.info() };
            const size_t itemsize{ depth_bytes( fi.depth ) };
            const ssize_t channels{ fi.channels };

            // matrix convention: [height, width] or [height, width, channels]
            std::vector<ssize_t> shape{ static_cast<ssize_t>( im.height() ),
                                        static_cast<ssize_t>( im.width() ) };
            std::vector<ssize_t> strides{ static_cast<ssize_t>( im.pitch() ),
                                          static_cast<ssize_t>( fi.bytes_per_pixel ) };
            if ( channels > 1 )
            {
                shape.push_back( channels );
                strides.push_back( static_cast<ssize_t>( itemsize ) );
            }

            return py::buffer_info(
                im.data(),
                static_cast<py::ssize_t>( itemsize ),
                buffer_format_for_depth( fi.depth ),
                static_cast<py::ssize_t>( shape.size() ),
                shape,
                strides );
        } )
        .def( py::init<>() )
        .def( py::init<uint32_t, uint32_t, pixel_format>() )
        .def( py::init<const core::size&, pixel_format>() )
        .def( py::init<const core::rect&, pixel_format>() )
        .def( py::self == py::self )
        .def( py::self != py::self )
        .def( "sub_image", &image::sub_image )
        .def( "extract",
              []( const image& im, const core::rect& r ) { return im.extract( r ); },
              py::arg( "rect" ) )
        .def( "clone", []( const image& im ) { return im.clone(); } )
        .def( "width", &image::width )
        .def( "height", &image::height )
        .def( "size", &image::size )
        .def( "pixel_count", &image::pixel_count )
        .def( "rect", &image::rect )
        .def( "format", &image::format )
        .def( "info", &image::info )
        .def( "pitch", &image::pitch )
        .def( "__repr__", to_str )
        .def( "__str__", to_str );

    // image conversion
    m.def( "convert_image",
           []( const image& im, pixel_format to ) { return convert( im, to ); },
           py::arg( "image" ), py::arg( "format" ) );
}
