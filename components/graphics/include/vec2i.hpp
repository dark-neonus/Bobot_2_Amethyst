#ifndef VEC2I_HPP
#define VEC2I_HPP

#include <string>

namespace Bobot {
namespace Graphics {

/**
 * @brief 2D integer vector class for positions, offsets, and speeds
 * 
 * Provides basic arithmetic operations and string representation
 */
class Vec2i {
public:
    int x;
    int y;

    // Constructors
    Vec2i() : x(0), y(0) {}
    Vec2i(int x, int y) : x(x), y(y) {}

    // Arithmetic operators
    Vec2i operator+(const Vec2i& other) const;
    Vec2i operator-(const Vec2i& other) const;
    Vec2i operator*(int scalar) const;
    Vec2i operator-() const;  // Negation

    // Compound assignment operators
    Vec2i& operator+=(const Vec2i& other);
    Vec2i& operator-=(const Vec2i& other);
    Vec2i& operator*=(int scalar);

    // Comparison operators
    bool operator==(const Vec2i& other) const;
    bool operator!=(const Vec2i& other) const;

    // String representation
    std::string toString() const;
};

} // namespace Graphics
} // namespace Bobot

#endif // VEC2I_HPP
