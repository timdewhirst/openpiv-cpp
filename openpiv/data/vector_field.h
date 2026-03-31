
#pragma once

// std
#include <array>
#include <concepts>
#include <deque>
#include <type_traits>
#include <utility>

// openpiv
#include "core/format_utils.h"
#include "core/point.h"
#include "core/vector.h"

// forward
#include <iosfwd>

namespace openpiv::data {

    template <typename VisitorT, typename DataT>
    concept VisitorC = requires(VisitorT v, const DataT& d)
    {
        v.visit(d);
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
    using point_vector_2d_2c = point_vector<core::point2<double>, core::vector2<double>>;
    using point_vector_2d_3c = point_vector<core::point2<double>, core::vector3<double>>;

    ///
    template <
        typename PointT,
        typename VectorT
    >
    class vector_field
    {
    public:
        using data_t = point_vector<PointT, VectorT>;

        vector_field() = default;
        vector_field(const vector_field&) = default;
        vector_field(vector_field&&) = default;
        vector_field& operator=(const vector_field&) = default;
        vector_field& operator=(vector_field&&) = default;

        vector_field(size_t sz)
            : _data(sz)
        {}

        void add(data_t d) { _data.push_back(d); }
        size_t size() const { return _data.size(); }

        template <typename VisitorT>
        requires VisitorC<VisitorT, data_t>
        void accept(VisitorT& v)
        {
            for (const auto& d : _data)
                v.visit(d);
        }

    private:
        std::deque<point_vector<PointT, VectorT>> _data;
    };

    ///
    using vector_field_2d_2c = vector_field<core::point2<double>, core::vector2<double>>;
    using vector_field_2d_3c = vector_field<core::point2<double>, core::vector3<double>>;

    template <
        template <size_t, typename> typename T,
        size_t N,
        typename DataT>
    concept DimensionedGeometryPrimitiveC = requires(T<N, DataT> t)
    {
        {t.data()} -> std::same_as<const std::array<DataT, N>&>;
    };

    template <
        template <size_t, typename> typename T,
        size_t N,
        typename DataT>
    requires DimensionedGeometryPrimitiveC<T, N, DataT>
    std::ostream& csv_output(std::ostream& os, const T<N, DataT>& p)
    {
        bool first = true;
        for (size_t i = 0; i < N; ++i) {
            if (!first) os << ", ";
            os << p[i];
            first = false;
        }
        return os;
    }

    template <
        typename PointT,
        typename VectorT
    >
    class csv_visitor
    {
    public:
        csv_visitor(std::ostream& os) : _os(os) {}

        void visit(const point_vector<PointT, VectorT>& visitable)
        {
            csv_output(_os, visitable.p) << ", ";
            csv_output(_os, visitable.v) << "\n";
        }

    private:
        std::ostream& _os;
    };

}
