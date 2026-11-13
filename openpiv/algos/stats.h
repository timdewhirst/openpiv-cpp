#pragma once

// std
#include <algorithm>
#include <limits>
#include <tuple>

// local
#include "core/exception_builder.h"
#include "core/image.h"
#include "core/pixel_format.h"

namespace openpiv::algos {

    using namespace core;

    /// find the minimum and maximum pixel values of a greyscale
    /// image
    inline std::tuple< double, double >
    find_image_range( const image& im )
    {
        if ( im.info().family != pixel_family::GREY )
            core::exception_builder<std::runtime_error>()
                << "find_image_range requires a greyscale image: " << im.format();

        double min{ std::numeric_limits<double>::max() };
        double max{ std::numeric_limits<double>::lowest() };

        dispatch_pixel_depth(
            im.info().depth,
            [&im, &min, &max]( auto tag )
            {
                using V = typename decltype( tag )::type;
                for ( uint32_t h = 0; h < im.height(); ++h )
                {
                    const V* p{ reinterpret_cast<const V*>( im.line( h ) ) };
                    for ( uint32_t w = 0; w < im.width(); ++w, ++p )
                    {
                        const double value{ static_cast<double>( *p ) };
                        min = std::min( min, value );
                        max = std::max( max, value );
                    }
                }
            } );

        return std::make_tuple( min, max );
    }

}
