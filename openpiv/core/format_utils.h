
#pragma once

// std
#include <format>
#include <sstream>

namespace openpiv::core {

    template <typename T>
    concept StreamableT = requires(T t)
    {
        operator<<(std::declval<std::ostream>(), t);
    };

    template<StreamableT T>
    struct StreamableFormatter
    {
        template<class ParseContext>
        constexpr ParseContext::iterator parse(ParseContext& ctx)
        {
            return ctx.begin();
        }
 
        template<class FmtContext>
        FmtContext::iterator format(const T& s, FmtContext& ctx) const
        {
            std::ostringstream out;
            out << s;

            return std::ranges::copy(std::move(out).str(), ctx.out()).out;
        }
    };

}