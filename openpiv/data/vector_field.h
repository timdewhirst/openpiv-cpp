
#pragma once

// std
#include <deque>

// openpiv
#include "core/format_utils.h"
#include "core/point.h"
#include "core/vector.h"

///
template <
    typename PointT,
    typename VectorT
>
struct point_vector
{
    PointT p {};
    VectorT v {};
};

/// 
template <
    typename PointT,
    typename VectorT
>
class vector_field
{
public:
    vector_field() = default;
    vector_field(const vector_field&) = default;
    vector_field(vector_field&&) = default;
    vector_field& operator=(const vector_field&) = default;
    vector_field& operator=(vector_field&&) = default;

    vector_field(size_t sz)
        : _data(sz)
    {}

private:
    std::deque<point_vector> _data;
};

///
using vector_field_2d_2c = vector_field<point2, vector2>;
using vector_field_2d_3c = vector_field<point2, vector3>;