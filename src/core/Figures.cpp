#include "Figures.hpp"
#include <cmath>

namespace core {

// Helper to check if point is in polygon
static bool isPointInPolygon(sf::Vector2f point,
                             const std::vector<sf::Vector2f> &vertices,
                             sf::Vector2f anchor) {
  bool c = false;
  int nvert = vertices.size();
  for (int i = 0, j = nvert - 1; i < nvert; j = i++) {
    sf::Vector2f vi = anchor + vertices[i];
    sf::Vector2f vj = anchor + vertices[j];
    if (((vi.y > point.y) != (vj.y > point.y)) &&
        (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x))
      c = !c;
  }
  return c;
}

// Basic draw for now - Phase 2 will implement accurate edges
static void basicDraw(const Figure &fig, sf::RenderTarget &target) {
  auto vertices = fig.getVertices();
  if (vertices.empty())
    return;

  sf::ConvexShape shape(vertices.size());
  for (size_t i = 0; i < vertices.size(); ++i) {
    shape.setPoint(i, vertices[i]);
  }
  shape.setPosition(fig.anchor);
  shape.setFillColor(fig.fillColor);

  // Simple outline for now
  if (!fig.edges.empty()) {
    shape.setOutlineThickness(fig.edges[0].width);
    shape.setOutlineColor(fig.edges[0].color);
  }

  target.draw(shape);
}

// Rectangle
Rectangle::Rectangle(float width, float height)
    : m_width(width), m_height(height) {
  edges.resize(4);
}
std::vector<sf::Vector2f> Rectangle::getVertices() const {
  return {{-m_width / 2.f, -m_height / 2.f},
          {m_width / 2.f, -m_height / 2.f},
          {m_width / 2.f, m_height / 2.f},
          {-m_width / 2.f, m_height / 2.f}};
}
void Rectangle::draw(sf::RenderTarget &target) const {
  basicDraw(*this, target);
}
bool Rectangle::contains(sf::Vector2f point) const {
  return isPointInPolygon(point, getVertices(), anchor);
}

// Triangle
Triangle::Triangle(float base, float height) : m_base(base), m_height(height) {
  edges.resize(3);
}
std::vector<sf::Vector2f> Triangle::getVertices() const {
  return {{0.f, -m_height / 2.f},
          {m_base / 2.f, m_height / 2.f},
          {-m_base / 2.f, m_height / 2.f}};
}
void Triangle::draw(sf::RenderTarget &target) const {
  basicDraw(*this, target);
}
bool Triangle::contains(sf::Vector2f point) const {
  return isPointInPolygon(point, getVertices(), anchor);
}

// Hexagon
Hexagon::Hexagon(float radius) : m_radius(radius) { edges.resize(6); }
std::vector<sf::Vector2f> Hexagon::getVertices() const {
  std::vector<sf::Vector2f> pts(6);
  for (int i = 0; i < 6; ++i) {
    float angle = i * 60.f * 3.14159f / 180.f;
    pts[i] = {m_radius * std::cos(angle), m_radius * std::sin(angle)};
  }
  return pts;
}
void Hexagon::draw(sf::RenderTarget &target) const { basicDraw(*this, target); }
bool Hexagon::contains(sf::Vector2f point) const {
  return isPointInPolygon(point, getVertices(), anchor);
}

// Rhombus
Rhombus::Rhombus(float width, float height) : m_width(width), m_height(height) {
  edges.resize(4);
}
std::vector<sf::Vector2f> Rhombus::getVertices() const {
  return {{0.f, -m_height / 2.f},
          {m_width / 2.f, 0.f},
          {0.f, m_height / 2.f},
          {-m_width / 2.f, 0.f}};
}
void Rhombus::draw(sf::RenderTarget &target) const { basicDraw(*this, target); }
bool Rhombus::contains(sf::Vector2f point) const {
  return isPointInPolygon(point, getVertices(), anchor);
}

// Trapezoid
Trapezoid::Trapezoid(float topWidth, float bottomWidth, float height)
    : m_topWidth(topWidth), m_bottomWidth(bottomWidth), m_height(height) {
  edges.resize(4);
}
std::vector<sf::Vector2f> Trapezoid::getVertices() const {
  return {{-m_topWidth / 2.f, -m_height / 2.f},
          {m_topWidth / 2.f, -m_height / 2.f},
          {m_bottomWidth / 2.f, m_height / 2.f},
          {-m_bottomWidth / 2.f, m_height / 2.f}};
}
void Trapezoid::draw(sf::RenderTarget &target) const {
  basicDraw(*this, target);
}
bool Trapezoid::contains(sf::Vector2f point) const {
  return isPointInPolygon(point, getVertices(), anchor);
}

} // namespace core
