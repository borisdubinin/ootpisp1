#include "Scene.hpp"

namespace core {

void Scene::addFigure(std::unique_ptr<Figure> fig) {
  if (fig) {
    m_figures.push_back(std::move(fig));
  }
}

bool Scene::removeFigure(Figure *fig) {
  for (auto it = m_figures.begin(); it != m_figures.end(); ++it) {
    if (it->get() == fig) {
      m_figures.erase(it);
      return true;
    }
  }
  return false;
}

Figure *Scene::hitTest(sf::Vector2f point) const {
  // Iterate backwards to check top-most figures first
  for (auto it = m_figures.rbegin(); it != m_figures.rend(); ++it) {
    if ((*it)->contains(point)) {
      return it->get();
    }
  }
  return nullptr;
}

void Scene::drawAll(sf::RenderTarget &target) const {
  for (const auto &fig : m_figures) {
    fig->draw(target);
  }
}

} // namespace core
