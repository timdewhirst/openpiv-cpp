// google
#include <benchmark/benchmark.h>

// openpiv
#include "core/image_utils2.h"
#include "loaders/image_loader.h"

using namespace openpiv;
using namespace openpiv::core;

static void transpose_benchmark(benchmark::State& state)
{
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    image im{ d, d, pixel_format::GREY_U8 };

    for (auto _ : state)
    {
        auto t{ transpose( im ) };
    }
}
BENCHMARK(transpose_benchmark)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);

template <pixel_format F>
static void transpose_format_benchmark(benchmark::State& state)
{
    const uint32_t d{ static_cast<uint32_t>( state.range(0) ) };
    image im{ d, d, F };

    for (auto _ : state)
    {
        auto t{ transpose( im ) };
    }
}
BENCHMARK_TEMPLATE(transpose_format_benchmark, pixel_format::GREY_U16)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);
BENCHMARK_TEMPLATE(transpose_format_benchmark, pixel_format::GREY_F64)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);
BENCHMARK_TEMPLATE(transpose_format_benchmark, pixel_format::RGBA_U8)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);
BENCHMARK_TEMPLATE(transpose_format_benchmark, pixel_format::RGBA_U16)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);
BENCHMARK_TEMPLATE(transpose_format_benchmark, pixel_format::COMPLEX_F64)->Threads(2)->RangeMultiplier(2)->Range(2, 1024);

BENCHMARK_MAIN();
