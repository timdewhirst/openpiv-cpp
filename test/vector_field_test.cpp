
// catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

// std
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

// local
#include "test_utils.h"

// to be tested
#include "data/vector_field.h"

using namespace Catch;
using namespace openpiv::core;
using namespace openpiv::data;

TEST_CASE("vector_field_2d_2c - csv output")
{
    vector_field_2d_2c f;
    f.add({ .p = {10.0, 10.0}, .v = {0.1, 0.2} });
    CHECK( f.size() == 1 );
    std::stringstream output;

    csv_visitor<point2<double>, vector2<double>> visitor(output);
    f.accept(visitor);
    CHECK(output.str() == "10, 10, 0.1, 0.2\n");
}

TEST_CASE("vector_field_2d_3c - csv output")
{
    vector_field_2d_3c f;
    f.add({ .p = {15.0, 25.0}, .v = {1.0, 2.0, 3.0} });
    f.add({ .p = {30.0, 40.0}, .v = {4.0, 5.0, 6.0} });
    CHECK( f.size() == 2 );
    std::stringstream output;

    csv_visitor<point2<double>, vector3<double>> visitor(output);
    f.accept(visitor);
    CHECK(output.str() == "15, 25, 1, 2, 3\n30, 40, 4, 5, 6\n");
}

TEST_CASE("vector_field_3d_3c - csv output")
{
    using vector_field_3d_3c = vector_field<point3<double>, vector3<double>>;
    vector_field_3d_3c f;
    f.add({ .p = {1.0, 2.0, 3.0}, .v = {0.1, 0.2, 0.3} });
    f.add({ .p = {4.0, 5.0, 6.0}, .v = {0.4, 0.5, 0.6} });
    CHECK( f.size() == 2 );
    std::stringstream output;

    csv_visitor<point3<double>, vector3<double>> visitor(output);
    f.accept(visitor);
    CHECK(output.str() == "1, 2, 3, 0.1, 0.2, 0.3\n4, 5, 6, 0.4, 0.5, 0.6\n");
}

