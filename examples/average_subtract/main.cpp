// std
#include <cinttypes>
#include <fstream>
#include <iostream>

// utils
#include <cxxopts.hpp>

// openpiv
#include "loaders/image_loader.h"
#include "core/enumerate.h"
#include "core/image_utils.h"
#include "core/log.h"
#include "core/stream_utils.h"
#include "algos/stats.h"

using namespace openpiv;
namespace logger = openpiv::core::logger;

core::image load_from_file(const std::string& filename)
{
    try {
        std::ifstream is(filename, std::ios::binary);
        if ( !is.is_open() )
            core::exception_builder<std::runtime_error>() << "failed to open " << filename;

        auto loader{ core::image_loader_registry::instance().find(is) };
        if ( !loader )
            core::exception_builder<std::runtime_error>() << "failed to find loader for " << filename;

        core::image image;
        loader->load( is, image );

        // process in double precision
        return core::convert( image, core::pixel_format::GREY_F64 );
    }
    catch ( std::exception& e )
    {
        logger::error("failed to load image: {}", e.what());
    }

    return {};
}

int main( int argc, char* argv[] )
{
    // log to stderr, up to INFO
    logger::Logger::instance().add_sink(
        [](logger::Level l, const std::string& m) -> bool
        {
            if ( l > logger::Level::INFO )
                return true;

            std::cerr << m << "\n";
            return true;
        });

    // get arguments
    cxxopts::Options options(argv[0]);
    options
        .positional_help("[input files]")
        .show_positional_help();

    std::vector<std::string> input_files;

    try
    {
        options
            .add_options()
            ("h, help", "help", cxxopts::value<bool>())
            ("i, input", "input files", cxxopts::value<std::vector<std::string>>(input_files));

        options.parse_positional({"input"});
        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help({""}) << "\n";
            return 0;
        }

        if (result.count("input") < 2)
        {
            logger::error("require two or more input images");
            return 1;
        }

    }
    catch (const std::exception& e)
    {
        logger::error("error parsing options: {}", e.what());
        return 1;
    }

    // load, process
    logger::info("input files: {}", core::join(input_files, ", "));

    // process: find average
    core::size s;
    core::image avg;
    size_t processed = 0;

    try {
        auto image = load_from_file(input_files[0]);
        if ( image.format() != core::pixel_format::UNKNOWN )
        {
            s = image.size();
            avg = core::image( s, core::pixel_format::GREY_F64 );
        }
    }
    catch (std::exception& e)
    {
        logger::error("failed to load first image: {}", e.what());
        return 1;
    }

    for ( size_t i=1; i<input_files.size(); ++i)
    {
        try {
            auto image = load_from_file(input_files[i]);
            if ( image.format() == core::pixel_format::UNKNOWN || image.size() != s )
            {
                logger::error("image has invalid size: {}", input_files[i]);
                continue;
            }

            for ( core::image::index_t j=0; j<avg.pixel_count(); ++j )
                avg.at<core::g_f>(j) = core::g_f( avg.at<core::g_f>(j) + image.at<core::g_f>(j) );
            ++processed;
        }
        catch ( std::exception& e )
        {
            logger::error("failed to load image: {}", e.what());
        }
    }

    if ( processed == 0 )
    {
        logger::error("failed to load any files!");
        return 1;
    }

    // normalize
    const double norm{ 1.0 / processed };
    for ( core::image::index_t j=0; j<avg.pixel_count(); ++j )
        avg.at<core::g_f>(j) = core::g_f( avg.at<core::g_f>(j) * norm );

    logger::info("found average; processed {} image files", processed);

    // process: for each image, subtract average and write as <name>.sub
    std::shared_ptr<core::image_loader> writer{ core::image_loader_registry::instance().find("image/x-portable-anymap") };
    if ( !writer )
    {
        logger::error("failed to find loader for pnm");
        return 1;
    }

    // iterate over all images, subtract average and write
    for ( size_t i=0; i<input_files.size(); ++i )
    {
        try {
            auto image = load_from_file(input_files[i]);
            if ( image.format() == core::pixel_format::UNKNOWN || image.size() != s )
                continue;

            for ( core::image::index_t j=0; j<image.pixel_count(); ++j )
                image.at<core::g_f>(j) = image.at<core::g_f>(j) - avg.at<core::g_f>(j);

            std::fstream os(
                input_files[i] + ".avg_sub.pnm",
                std::ios_base::trunc | std::ios_base::out | std::ios_base::binary );
            writer->save( os, image );
        }
        catch ( std::exception& )
        {}
    }

    return 0;
}
