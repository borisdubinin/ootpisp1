#pragma once

#include "Figure.hpp"
#include <memory>
#include <vector>

namespace core {

class Scene {
public:
  void addFigure(std::unique_ptr<Figure> fig);

  // Removes the given figure from the scene. Returns true if removed.
  bool removeFigure(Figure *fig);

  // Returns the top-most figure at the given absolute point, or nullptr if none
  Figure *hitTest(sf::Vector2f point) const;

  // Draw all figures
  void drawAll(sf::RenderTarget &target) const;

  const std::vector<std::unique_ptr<Figure>> &getFigures() const {
    return m_figures;
  }

private:
  std::vector<std::unique_ptr<Figure>> m_figures;
};

} // namespace core
