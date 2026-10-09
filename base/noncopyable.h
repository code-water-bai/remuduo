#ifndef NEO_MUDUO_BASE_NONCOPYABLE_H
#define NEO_MUDUO_BASE_NONCOPYABLE_H

namespace new_muduo {

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}  

#endif  