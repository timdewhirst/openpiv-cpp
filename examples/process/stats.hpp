
#pragma once

/// add observations and calculate mean on demand
class StatsMean
{
public:
    void add_observation(uint64_t v)
    {
        _total += v;
        ++_count;
    }

    uint64_t value() const
    {
        return _total / _count;
    }
    uint64_t total() const { return _total; }
    uint64_t count() const { return _count; }
    
private:
    uint64_t _total {};
    uint64_t _count {};
};

