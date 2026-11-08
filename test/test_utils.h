#pragma once

// std
#include <fstream>
#include <memory>
#include <string>

// to be tested
#include "core/image2.h"
#include "loaders/image_loader.h"

using namespace openpiv;
using namespace openpiv::core;

/// load the first image from a file; the format of the returned
/// image is determined by the file
inline image load_from_file( const std::string& filename )
{
    std::ifstream is(filename, std::ios::binary);
    if ( !is.is_open() )
        exception_builder<std::runtime_error>() << "failed to open " << filename;

    std::shared_ptr<image_loader> loader{ image_loader_registry::instance().find(is) };
    if ( !loader )
        exception_builder<std::runtime_error>() << "failed to find loader for " << filename;

    image result;
    loader->load( is, result );
    is.close();

    return result;
}

/// save an image as PNM; note that PNM is 16-bit so pixel data may
/// be converted
inline bool save_to_file( const std::string& filename, const image& im )
{
    std::shared_ptr<image_loader> writer{ image_loader_registry::instance().find("image/x-portable-anymap") };
    if ( !writer )
        exception_builder<std::runtime_error>() << "failed to find image writer";

    std::fstream os( filename, std::ios_base::trunc | std::ios_base::out | std::ios_base::binary );
    writer->save( os, im );
    os.close();

    return true;
}
