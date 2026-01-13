#include "vec2i.hpp"
#include <sstream>

namespace Bobot {
namespace Graphics {

// Arithmetic operators
Vec2i Vec2i::operator+(const Vec2i& other) const {
    return Vec2i(x + other.x, y + other.y);
}

Vec2i Vec2i::operator-(const Vec2i& other) const {
    return Vec2i(x - other.x, y - other.y);
}

Vec2i Vec2i::operator*(int scalar) const {
    return Vec2i(x * scalar, y * scalar);
}

Vec2i Vec2i::operator-() const {
    return Vec2i(-x, -y);
}

// Compound assignment operators
Vec2i& Vec2i::operator+=(const Vec2i& other) {
    x += other.x;
    y += other.y;
    return *this;
}

Vec2i& Vec2i::operator-=(const Vec2i& other) {
    x -= other.x;
    y -= other.y;
    return *this;
}

Vec2i& Vec2i::operator*=(int scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
}

// Comparison operators
bool Vec2i::operator==(const Vec2i& other) const {
    return x == other.x && y == other.y;
}

bool Vec2i::operator!=(const Vec2i& other) const {
    return !(*this == other);
}

// String representation
std::string Vec2i::toString() const {
    std::ostringstream oss;
    oss << "Vec2i(" << x << ", " << y << ")";
    return oss.str();
}

} // namespace Graphics
} // namespace Bobot
