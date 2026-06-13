#pragma once

// doctest compatibility header for migrating from Catch2
// This provides Catch2-like macros and matchers using doctest

#include <doctest.h>

// Standard library includes for string operations
#include <algorithm>
#include <cmath>
#include <string>
#include <cctype>
#include <cstring>

// String matching utilities to replace Catch2 matchers
namespace doctest::matchers {

    // Case-sensitive substring matching
    inline bool ContainsSubstring(const std::string& str, const std::string& substr) {
        return str.find(substr) != std::string::npos;
    }

    // Case-insensitive substring matching
    inline bool ContainsSubstringNoCase(const std::string& str, const std::string& substr) {
        std::string str_lower = str;
        std::string substr_lower = substr;
        std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        std::transform(substr_lower.begin(), substr_lower.end(), substr_lower.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        return str_lower.find(substr_lower) != std::string::npos;
    }

    // Check if string ends with substring (case-sensitive)
    inline bool EndsWith(const std::string& str, const std::string& suffix) {
        if (suffix.size() > str.size()) return false;
        return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // Check if string ends with substring (case-insensitive)
    inline bool EndsWithNoCase(const std::string& str, const std::string& suffix) {
        if (suffix.size() > str.size()) return false;
        std::string str_lower = str;
        std::string suffix_lower = suffix;
        std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        std::transform(suffix_lower.begin(), suffix_lower.end(), suffix_lower.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        return str_lower.compare(str_lower.size() - suffix_lower.size(), suffix_lower.size(), suffix_lower) == 0;
    }

    // Numeric approximation matcher (replaces WithinAbs)
    // Supports mixed types by converting all to double
    template<typename T, typename U, typename V>
    inline bool WithinAbs(T value, U expected, V epsilon) {
        return std::abs(static_cast<double>(value) - static_cast<double>(expected)) <= static_cast<double>(epsilon);
    }

} // namespace doctest::matchers

// Custom exception testing macro to replace _REQUIRE_THROWS_MATCHES
// Usage: _REQUIRE_THROWS_MATCHES(statement, ExceptionType, matcher_function)
// where matcher_function takes a string and returns bool
#define _REQUIRE_THROWS_MATCHES(stmt, ExceptionT, matcher) \
    do { \
        bool caught = false; \
        std::string exception_message; \
        try { \
            stmt; \
        } catch (const ExceptionT& e) { \
            caught = true; \
            exception_message = e.what(); \
            if (!(matcher.match(exception_message))) { \
                FAIL("Exception message does not match: " << exception_message); \
            } \
        } catch (...) { \
            FAIL("Wrong exception type thrown"); \
        } \
        if (!caught) { \
            FAIL("Expected exception " #ExceptionT " not thrown"); \
        } \
    } while (false)

// Compatibility aliases for Catch2 users
// Define CaseSensitive enum for compatibility with existing code
namespace Catch {
    enum class CaseSensitive { Yes, No };
}

// Alias for convenience
using CaseSensitive = Catch::CaseSensitive;

// ContainsSubstring matcher class (for compatibility with Catch2 style)
struct ContainsSubstringMatcher {
    std::string substring;
    Catch::CaseSensitive case_sensitive;
    
    ContainsSubstringMatcher(const std::string& substr, Catch::CaseSensitive cs = Catch::CaseSensitive::Yes)
        : substring(substr), case_sensitive(cs) {}
    
    bool match(const std::string& str) const {
        if (case_sensitive == Catch::CaseSensitive::Yes) {
            return doctest::matchers::ContainsSubstring(str, substring);
        } else {
            return doctest::matchers::ContainsSubstringNoCase(str, substring);
        }
    }
};

// Factory function for ContainsSubstring
inline ContainsSubstringMatcher ContainsSubstring(const std::string& substr, Catch::CaseSensitive cs = Catch::CaseSensitive::Yes) {
    return ContainsSubstringMatcher(substr, cs);
}

// EndsWith matcher class
struct EndsWithMatcher {
    std::string suffix;
    
    explicit EndsWithMatcher(const std::string& s) : suffix(s) {}
    
    bool match(const std::string& str) const {
        return doctest::matchers::EndsWith(str, suffix);
    }
};

// Factory function for EndsWith
inline EndsWithMatcher EndsWith(const std::string& s) {
    return EndsWithMatcher(s);
}

// WithinAbs matcher class for numeric comparisons
struct WithinAbsMatcher {
    double value;
    double epsilon;
    
    WithinAbsMatcher(double v, double eps) : value(v), epsilon(eps) {}
    
    template<typename T>
    bool match(T actual) const {
        return doctest::matchers::WithinAbs(static_cast<double>(actual), value, epsilon);
    }
};

// Factory function for WithinAbs
inline WithinAbsMatcher WithinAbs(double value, double epsilon) {
    return WithinAbsMatcher(value, epsilon);
}
