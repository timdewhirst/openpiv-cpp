// google
#include <benchmark/benchmark.h>

// openpiv
#include "algos/fft.h"
#include "loaders/image_loader.h"

// test
#include "test_utils.h"

using namespace openpiv;
using namespace openpiv::core;
using namespace openpiv::algos;

static void fft_cross_correlation_view_benchmark(benchmark::State& state)
{
    image im_a{ convert( load_from_file( "corr_a.tiff" ), pixel_format::GREY_F64 ) };
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    size s{ d, d };
    FFT fft( s );

    auto view_a = im_a.sub_image( rect{ {0, 0}, s } );
    auto view_b = im_a.sub_image( rect{ {1, 1}, s } );

    for (auto _ : state)
    {
        // measure FFT speed
        fft.cross_correlate( view_a, view_b );
    }
}
// Register the function as a benchmark
BENCHMARK(fft_cross_correlation_view_benchmark)->Threads(4)->RangeMultiplier(2)->Range(4, 64);

static void fft_cross_correlation_extract_benchmark(benchmark::State& state)
{
    image im_a{ convert( load_from_file( "corr_a.tiff" ), pixel_format::GREY_F64 ) };
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    size s{ d, d };
    FFT fft( s );

    auto sub_a = im_a.extract( rect{ {0, 0}, s } );
    auto sub_b = im_a.extract( rect{ {1, 1}, s } );

    for (auto _ : state)
    {
        // measure FFT speed
        fft.cross_correlate( sub_a, sub_b );
    }
}
// Register the function as a benchmark
BENCHMARK(fft_cross_correlation_extract_benchmark)->Threads(4)->RangeMultiplier(2)->Range(4, 64);

static void fft_auto_correlation_view_benchmark(benchmark::State& state)
{
    image im_a{ convert( load_from_file( "corr_a.tiff" ), pixel_format::GREY_F64 ) };
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    size s{ d, d };
    FFT fft( s );

    auto view_a = im_a.sub_image( rect{ {}, s } );

    for (auto _ : state)
    {
        // measure FFT speed
        fft.auto_correlate( view_a );
    }
}
// Register the function as a benchmark
BENCHMARK(fft_auto_correlation_view_benchmark)->Threads(4)->RangeMultiplier(2)->Range(4, 64);

static void fft_auto_correlation_extract_benchmark(benchmark::State& state)
{
    image im_a{ convert( load_from_file( "corr_a.tiff" ), pixel_format::GREY_F64 ) };
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    size s{ d, d };
    FFT fft( s );

    auto view_a = im_a.extract( rect{ {}, s } );

    for (auto _ : state)
    {
        // measure FFT speed
        fft.auto_correlate( view_a );
    }
}
// Register the function as a benchmark
BENCHMARK(fft_auto_correlation_extract_benchmark)->Threads(4)->RangeMultiplier(2)->Range(4, 64);

BENCHMARK_MAIN();
