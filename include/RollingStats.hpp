#pragma once
#include <deque>
#include <cstddef>
#include <cmath>

class RollingStats
{
private:
    std::deque<double> buf;
    size_t window;
    double sum;
    double sumSq;

public:
    explicit RollingStats(size_t window);
    void add(double x);
    bool ready() const;
    double mean() const;
    double stddev() const;
    size_t size() const;
    void clear();
};