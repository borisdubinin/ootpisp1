#pragma once

#include "Figure.hpp"

namespace core {

class Rectangle : public Figure {
public:
  Rectangle(float width, float height);

private:
  float m_width;
  float m_height;
};

class Triangle : public Figure {
public:
  Triangle(float base, float height);

private:
  float m_base;
  float m_height;
};

class Hexagon : public Figure {
public:
  Hexagon(float width, float height);

private:
  float m_width;
  float m_height;
};

class Rhombus : public Figure {
public:
  Rhombus(float width, float height);

private:
  float m_width;
  float m_height;
};

class Trapezoid : public Figure {
public:
  Trapezoid(float topWidth, float bottomWidth, float height);

private:
  float m_topWidth;
  float m_bottomWidth;
  float m_height;
};

class Circle : public Figure {
public:
  Circle(float radiusX, float radiusY);
  bool hasUniformEdge() const override { return true; }

private:
  float m_radiusX;
  float m_radiusY;
};

} // namespace core
