#ifndef NEO_MUDUO_BASE_TIMESTAMP_H
#define NEO_MUDUO_BASE_TIMESTAMP_H

#include <cstdint>
#include <string>
#include <chrono>
#include "noncopyable.h"

namespace new_muduo {

class Timestamp {
public:
    Timestamp() : microSecondsSinceEpoch_(0) {}

    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

  
    static Timestamp now();

  
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

   
    std::string toString() const;
    std::string toFormattedString(bool showMicroseconds = true) const;

   
    bool operator<(Timestamp rhs) const {
        return microSecondsSinceEpoch_ < rhs.microSecondsSinceEpoch_;
    }
    bool operator==(Timestamp rhs) const {
        return microSecondsSinceEpoch_ == rhs.microSecondsSinceEpoch_;
    }

   
    Timestamp operator+(double seconds) const {
        return Timestamp(microSecondsSinceEpoch_ +
                         static_cast<int64_t>(seconds * kMicroSecondsPerSecond));
    }
    double operator-(Timestamp rhs) const {
        int64_t diff = microSecondsSinceEpoch_ - rhs.microSecondsSinceEpoch_;
        return static_cast<double>(diff) / kMicroSecondsPerSecond;
    }

    static const int kMicroSecondsPerSecond = 1000 * 1000;

    static Timestamp invaild() { return Timestamp(); }
    bool valid() const { return microSecondsSinceEpoch_ > 0; }

private:
    int64_t microSecondsSinceEpoch_;
};

inline Timestamp addTime(Timestamp timestamp, double seconds) {
    return timestamp + seconds;
}

}  

#endif  