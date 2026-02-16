
#pragma once

// std
#include <deque>

// openpiv
#include "core/format_utils.h"
#include "core/point.h"
#include "core/vector.h"

// forward
#include <iosfwd>

namespace openpiv::data {

    ///
    template <
        typename PointT, 
        typename VectorT
    >
    class visitor
    {
    public:
        virtual void visit(point_vector<PointT, VectorT>& visitable) = 0;
    };

    /// 
    template <typename PointT>
    struct csv_output
    template <
        typename PointT, 
        typename VectorT
    >
    class csv_visitor : public visitor<PointT, VectorT>
    {
    public:
        csv_visitor(std::ostream& os) : _os(os) {}

        void visit(point_vector<PointT, VectorT>& visitable) override
        {
            _os << 
        }

    private:
        std::ostream& _os;
    };


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

        void accept(visitor<PointT, VectorT>& v)
        {
            for (auto& d : _data)
                v.visit(d);
        }

    private:
        std::deque<point_vector> _data;
    };

    ///
    using vector_field_2d_2c = vector_field<point2, vector2>;
    using vector_field_2d_3c = vector_field<point2, vector3>;
}