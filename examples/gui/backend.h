#pragma once

// std
#include <functional>
#include <string_view>
#include <memory>

/// abstract backend
class Backend
{
public:
    virtual bool init(std::string_view) = 0;
    virtual void run(const std::function<bool()>& draw_callback) = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual bool should_close() const = 0;

    /// factory method
    static std::unique_ptr<Backend> create(std::string_view);
};

