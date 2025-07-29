
#include <RollingStats.hpp>

RollingStats::RollingStats(size_t window) : window(window), sum(0.0), sumSq(0.0) {
    if (window == 0) {
        throw std::invalid_argument("size has to be greater than 0");
    }
}

void RollingStats::add(double x) {
    buf.push_back(x);
    sum += x;
    sumSq += (x * x);
    
    if (buf.size() > window) {
        double old = buf.front();
        buf.pop_front();
        sum -= old;
        sumSq -= (old * old);
    }
}

bool RollingStats::ready() const {
    return buf.size() == window;
}

size_t RollingStats::size() const {
    return buf.size();
}

double RollingStats::mean() const {
    if (buf.empty()) {
        throw std::runtime_error("empty buffer");
    }
    return sum / static_cast<double>(buf.size());
}

double RollingStats::stddev() const {
    if (buf.size() < 2) {
        throw std::runtime_error("Need at least 2 poin ts");
    }
    
    double n = static_cast<double>(buf.size());
    double variance = (sumSq / n) - (sum / n) * (sum / n);
    
    if (variance < 0.0) {
        variance = 0.0;
    }
    return std::sqrt(variance);
}

void RollingStats::clear() {
    buf.clear();
    sum = 0.0;
    sumSq = 0.0;
}