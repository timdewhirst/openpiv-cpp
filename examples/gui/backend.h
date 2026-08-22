#pragma once

// std
#include <functional>

/// common interface for abstracting backend details;
/// no need to be virtual atm as implementation can
/// be a compile time option
class Backend
{
public:
    bool Init();
    void Run(const std::function<void()>& drawCallback);
    void BeginFrame();
    void EndFrame();
    bool ShouldClose() const;
};
