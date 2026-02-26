#include "Figure.hpp"

namespace core {

sf::FloatRect Figure::getBoundingBox() const {
  auto vertices = getVertices();
  if (vertices.empty()) {
    return sf::FloatRect(anchor.x, anchor.y, 0.f, 0.f);
  }

  float minX = vertices[0].x;
  float maxX = vertices[0].x;
  float minY = vertices[0].y;
  float maxY = vertices[0].y;

  for (const auto &v : vertices) {
    if (v.x < minX)
      minX = v.x;
    if (v.x > maxX)
      maxX = v.x;
    if (v.y < minY)
      minY = v.y;
    if (v.y > maxY)
      maxY = v.y;
  }

  // Convert relative to absolute
  return sf::FloatRect(anchor.x + minX, anchor.y + minY, maxX - minX,
                       maxY - minY);
}

void Figure::move(sf::Vector2f delta) { anchor += delta; }

} // namespace core
