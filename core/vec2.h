#pragma once
#include <cmath>

namespace tonk {

struct Vec2 {
  float x = 0, y = 0;
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  float len() const { return std::sqrt(x * x + y * y); }
  Vec2 norm() const {
    float l = len();
    return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{};
  }
};

}  // namespace tonk
