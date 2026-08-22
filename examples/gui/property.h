
#pragma once

// std
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>

// helper type for the visitor
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using PropertyValue = std::variant<std::monostate, int, double, bool, std::string>;
using Properties = std::unordered_map<std::string, PropertyValue>;

inline
std::ostream& operator<<(std::ostream& os, const Properties& ps)
{
    for (const auto& [k, v] : ps) {
        os << k << ": ";
        std::visit(
            overloaded(
                [&os](std::monostate){os << "[null]";},
                [&os](const auto& v){os << v;}
            ), v);
    }
    return os;
}
