# LR2 + LR3 — Полное руководство по реализации для AI-агента

> Язык: C++17 · Графика: SFML 3 · UI: Dear ImGui + ImGui-SFML · Сборка: CMake · ОС: Linux

---

## 0. Обзор существующей архитектуры

```
src/
  core/
    Figure.hpp / Figure.cpp       — базовый класс Figure (вершины, рёбра, трансформации)
    Figures.hpp / Figures.cpp     — конкретные фигуры: Rectangle, Triangle, Hexagon, Rhombus, Trapezoid, Circle
    Scene.hpp / Scene.cpp         — сцена: std::vector<unique_ptr<Figure>> m_figures, hitTest, drawAll
    Viewport.hpp                  — преобразования world↔screen
    MathUtils.hpp                 — константы (PI)
  ui/
    Toolbar.hpp / Toolbar.cpp     — панель инструментов с enum Tool
    PropertiesPanel.hpp / .cpp    — боковая панель свойств
    CreateFigureModal.hpp / .cpp  — модальное окно создания фигуры через ПКМ
  utils/
    GeometryUtils.hpp / .cpp      — утилиты геометрии
  main.cpp                        — главный цикл (SFML + ImGui)
```

**Ключевые факты:**
- `Figure` — абстрактный базовый класс. Хранит `m_vertices` (относительные вершины), `anchor`, `parentOrigin`, `rotationAngle`, `scale`, `edges` (стили рёбер).
- `Scene::m_figures` — `std::vector<std::unique_ptr<Figure>>` без ограничения размера.
- Инструменты выбираются через `ui::Tool` (enum) в `Toolbar`.
- Правый клик открывает `CreateFigureModal`.
- `main.cpp` содержит весь игровой цикл (события, рендер, ImGui).

---

## 1. LR2 — Рефакторинг иерархии классов

### 1.1 Новая иерархия

Все существующие примитивы (`Rectangle`, `Triangle`, …) должны стать **частными случаями** единого класса `PolylineFigure`. Дополнительно нужен класс `CompoundFigure` для объединённых фигур.

```
Figure  (абстрактный, оставить как есть)
├── PolylineFigure   [NEW]  — фигура из произвольного набора отрезков (вершин)
│   ├── Rectangle    [MODIFY] — теперь просто фабричный хелпер / typedef
│   ├── Triangle     [MODIFY]
│   ├── Hexagon      [MODIFY]
│   ├── Rhombus      [MODIFY]
│   ├── Trapezoid    [MODIFY]
│   └── Circle       [MODIFY]  — оставить как есть (много вершин ≈ окружность)
└── CompoundFigure  [NEW]  — фигура, составленная из других Figure*
```

### 1.2 Класс `PolylineFigure`

**Файл:** `src/core/PolylineFigure.hpp` (новый)

```cpp
#pragma once
#include "Figure.hpp"
#include <string>

namespace core {

/// Универсальная фигура, задаваемая набором вершин.
/// Ломаная автоматически замыкается при отрисовке и contains().
/// Базовые фигуры (Rectangle, Triangle, …) становятся подклассами,
/// которые лишь заполняют m_vertices нужным образом в конструкторе.
class PolylineFigure : public Figure {
public:
    /// Имя фигуры (для меню выбора инструмента)
    std::string figureName;

    /// Построить из явного списка вершин (для кастомных ломаных)
    explicit PolylineFigure(std::vector<sf::Vector2f> vertices,
                            std::string name = "Custom");

    // --- Переопределения Figure ---
    bool hasSideLengths()  const override { return true; }
    void setSideLengths(const std::vector<float>& lengths) override;
    const char* getSideName(int idx) const override;

    /// Тип фигуры для сериализации
    virtual std::string typeName() const { return "polyline"; }

    /// Задать угол i-го отрезка (в градусах) и пересчитать следующую вершину
    void setEdgeAngle(int edgeIdx, float angleDeg);

    /// Получить угол i-го отрезка (в градусах)
    float getEdgeAngle(int edgeIdx) const;

protected:
    PolylineFigure() = default; // для подклассов
};

} // namespace core
```

**Файл:** `src/core/PolylineFigure.cpp` (новый)

Реализовать:
- Конструктор: сохраняет вершины, создаёт `edges.resize(vertices.size())`.
- `setSideLengths` — использует существующий `applyGenericSideLengths` из `Figure`.
- `getSideName` — возвращает `"Side N"` по индексу.
- `setEdgeAngle(i, angle)`: находит длину ребра `i`, вычисляет новую вершину `i+1` по формуле:
  ```
  vertex[i+1] = vertex[i] + length * {cos(angle_rad), sin(angle_rad)}
  ```
  Затем сдвигает все последующие вершины так, чтобы форма не "сломалась" (только вершина `i+1` перемещается). Если это не последнее ребро — последующие вершины сдвигаются на тот же дельта.
- `getEdgeAngle(i)` — `atan2(v[i+1].y - v[i].y, v[i+1].x - v[i].x)` в градусах.

### 1.3 Превращение базовых фигур в подклассы `PolylineFigure`

**Файл:** `src/core/Figures.hpp` — изменить базовый класс для всех существующих фигур:

```cpp
// Было:
class Rectangle : public Figure { ... };

// Станет:
class Rectangle : public PolylineFigure {
public:
    Rectangle(float width, float height);
    std::string typeName() const override { return "rectangle"; }
    // setSideLengths — оставить свою реализацию
    const char* getSideName(int idx) const override { ... }
};
```

Аналогично для `Triangle`, `Hexagon`, `Rhombus`, `Trapezoid`, `Circle`.

**В `Figures.cpp`:** в конструкторах добавить `figureName = "Rectangle"` (или соответствующее имя).

### 1.4 Класс `CompoundFigure`

**Файл:** `src/core/CompoundFigure.hpp` (новый)

```cpp
#pragma once
#include "Figure.hpp"
#include <vector>
#include <string>

namespace core {

/// Фигура, составленная из нескольких дочерних Figure.
/// Хранит список дочерних фигур и их относительные позиции.
class CompoundFigure : public Figure {
public:
    std::string figureName; ///< пользовательское имя

    struct Child {
        std::unique_ptr<Figure> figure;
        sf::Vector2f localOffset; ///< смещение относительно anchor CompoundFigure
        float localRotation = 0.f;
    };

    std::vector<Child> children;

    void draw(sf::RenderTarget& target) const override;
    bool contains(sf::Vector2f point) const override;

    /// Пересчитать bounding-box вершины (для выделения)
    const std::vector<sf::Vector2f>& getVertices() const override;

    std::string typeName() const { return "compound"; }

private:
    mutable std::vector<sf::Vector2f> m_cachedVerts;
};

} // namespace core
```

**Реализация `CompoundFigure::draw`:**
```cpp
void CompoundFigure::draw(sf::RenderTarget& target) const {
    sf::Vector2f absAnchor = parentOrigin + anchor;
    for (const auto& child : children) {
        // Временно сдвигаем дочернюю фигуру
        sf::Vector2f savedAnchor = child.figure->anchor;
        sf::Vector2f savedParent = child.figure->parentOrigin;

        child.figure->parentOrigin = absAnchor;
        child.figure->anchor = child.localOffset;
        child.figure->rotationAngle += localRotation;
        child.figure->draw(target);

        child.figure->anchor = savedAnchor;
        child.figure->parentOrigin = savedParent;
        child.figure->rotationAngle -= localRotation;
    }
}
```

**`CompoundFigure::contains`** — возвращает `true`, если хотя бы один дочерний `contains(point)`.

**`CompoundFigure::getVertices`** — собирает все абсолютные вершины детей и переводит их в локальную систему координат compound-фигуры (для bounding box).

---

## 2. LR2 — Создание составной фигуры из фигур на сцене

### 2.1 UI: режим "Compound Select"

Добавить в `ui::Tool` (Toolbar.hpp):
```cpp
enum class Tool {
    Select, Rectangle, Triangle, Hexagon, Rhombus, Trapezoid, Circle,
    Polyline,   // [NEW] рисование ломаной кликами
    CompoundSelect, // [NEW] выбор фигур для объединения
};
```

В `Toolbar.cpp` добавить кнопки для новых инструментов.

### 2.2 Логика в `main.cpp`

**Состояние:**
```cpp
std::vector<core::Figure*> compoundSelection; // фигуры, выбранные для объединения
bool isCompoundSelectMode = false;
```

**При клике в режиме `CompoundSelect`:**
```cpp
core::Figure* hit = scene.hitTest(mousePos);
if (hit) {
    auto it = std::find(compoundSelection.begin(), compoundSelection.end(), hit);
    if (it == compoundSelection.end())
        compoundSelection.push_back(hit);
    else
        compoundSelection.erase(it); // повторный клик = убрать из выбора
}
```

**Кнопка "Merge" в ImGui (боковая панель или отдельное окно):**
```cpp
if (ImGui::Button("Merge Selected") && compoundSelection.size() >= 2) {
    // Диалог: запросить имя
    // После подтверждения:
    auto compound = std::make_unique<core::CompoundFigure>();
    compound->figureName = enteredName; // из InputText
    sf::Vector2f center = computeCenterOfFigures(compoundSelection);
    compound->anchor = center;
    for (auto* fig : compoundSelection) {
        core::CompoundFigure::Child child;
        child.localOffset = fig->anchor - center; // относительно центра
        child.localRotation = 0.f;
        // Перемещаем unique_ptr из Scene в compound:
        child.figure = scene.extractFigure(fig); // см. ниже
        compound->children.push_back(std::move(child));
    }
    scene.addFigure(std::move(compound));
    compoundSelection.clear();
    // Добавить имя в список пользовательских инструментов (см. 2.3)
}
```

**Добавить в `Scene`:**
```cpp
// Scene.hpp
std::unique_ptr<Figure> extractFigure(Figure* fig);

// Scene.cpp
std::unique_ptr<Figure> Scene::extractFigure(Figure* fig) {
    for (auto it = m_figures.begin(); it != m_figures.end(); ++it) {
        if (it->get() == fig) {
            auto ptr = std::move(*it);
            m_figures.erase(it);
            return ptr;
        }
    }
    return nullptr;
}
```

**Кнопка "Remove from Compound"** (в PropertiesPanel, если выбранная фигура — CompoundFigure):
```cpp
// В PropertiesPanel::render():
if (auto* cf = dynamic_cast<core::CompoundFigure*>(selectedFig)) {
    for (int i = 0; i < (int)cf->children.size(); ++i) {
        ImGui::Text("Child %d", i);
        ImGui::SameLine();
        if (ImGui::Button(("Remove##child" + std::to_string(i)).c_str())) {
            // Извлечь child и добавить обратно на сцену
            auto extracted = std::move(cf->children[i].figure);
            sf::Vector2f absAnchor = cf->parentOrigin + cf->anchor + cf->children[i].localOffset;
            extracted->anchor = absAnchor;
            extracted->parentOrigin = {0,0};
            scene.addFigure(std::move(extracted));
            cf->children.erase(cf->children.begin() + i);
            break;
        }
    }
}
```

### 2.3 Пользовательские фигуры в меню инструментов

Хранить список пользовательских типов:
```cpp
// main.cpp (или отдельный UserFigureRegistry)
struct UserFigureType {
    std::string name;
    // Для CompoundFigure: хранить "шаблон" CompoundFigure для клонирования
    std::function<std::unique_ptr<core::Figure>()> factory;
};
std::vector<UserFigureType> userFigureTypes;
```

При создании `CompoundFigure` регистрировать тип:
```cpp
userFigureTypes.push_back({compound->figureName, [snapshot = cloneCompound(compoundPtr)]() {
    return cloneCompound(snapshot.get()); // глубокое копирование
}});
```

В `Toolbar` передавать `userFigureTypes` и отображать их как дополнительные кнопки. При выборе такого инструмента устанавливать специальный режим: при следующем клике-и-drag вставлять клон.

---

## 3. LR2 — Ломаная из отрезков (Polyline Tool)

### 3.1 Рисование ломаной кликами

**Режим:** `ui::Tool::Polyline`

**Состояние:**
```cpp
bool isDrawingPolyline = false;
std::vector<sf::Vector2f> polylinePoints;
bool snapAngle = false; // зажата Shift → привязка к 15°
```

**Логика:**
- Первый ЛКМ: начать ломаную, добавить первую точку.
- Каждый следующий ЛКМ: добавить точку.
- Если зажата Shift: вычислить угол от предыдущей точки до мыши, округлить до ближайших 15°, скорректировать позицию мыши.
- Двойной ЛКМ или Enter: завершить ломаную, **автоматически замкнуть** (добавить ребро от последней точки к первой).
- Escape: отменить.

**Привязка угла:**
```cpp
sf::Vector2f snapToAngle(sf::Vector2f from, sf::Vector2f to) {
    float dx = to.x - from.x, dy = to.y - from.y;
    float len = std::hypot(dx, dy);
    float angle = std::atan2(dy, dx);
    float snapped = std::round(angle / (math::PI / 12.f)) * (math::PI / 12.f); // 15°
    return { from.x + len * std::cos(snapped), from.y + len * std::sin(snapped) };
}
```

**Предпросмотр:** пока ломаная рисуется, отображать `sf::VertexArray` с пунктирной линией от последней точки до текущего положения мыши (рисовать в главном цикле после `scene.drawAll`).

**Завершение:**
```cpp
auto fig = std::make_unique<core::PolylineFigure>(polylinePoints, "Custom Polyline");
// Центрировать вершины:
sf::Vector2f c = centerOf(polylinePoints);
for (auto& v : fig->getVerticesMutable()) v -= c;
fig->anchor = c;
// Запросить имя через ImGui InputText диалог
scene.addFigure(std::move(fig));
polylinePoints.clear();
isDrawingPolyline = false;
```

### 3.2 Редактирование углов в PropertiesPanel

Если выбрана `PolylineFigure` (или её подкласс), в `PropertiesPanel` показать таблицу рёбер с полем для угла:

```cpp
if (auto* pl = dynamic_cast<core::PolylineFigure*>(selectedFig)) {
    for (int i = 0; i < (int)pl->getVertices().size(); ++i) {
        float angle = pl->getEdgeAngle(i);
        ImGui::Text("Edge %d angle:", i);
        ImGui::SameLine();
        if (ImGui::InputFloat(("##ea" + std::to_string(i)).c_str(), &angle, 1.f, 5.f, "%.1f°")) {
            pl->setEdgeAngle(i, angle);
        }
    }
}
```

---

## 4. LR3 — Массив фигур сцены (SceneArray)

### 4.1 Новый класс `SceneArray`

Заменить (или обернуть) `Scene::m_figures` на фиксированный массив.

**Файл:** `src/core/SceneArray.hpp` (новый)

```cpp
#pragma once
#include "Figure.hpp"
#include <array>
#include <cstddef>

namespace core {

constexpr int SCENE_MAX_FIGURES = 1000;

/// Массив фигур с фиксированной ёмкостью.
/// Хранит raw-указатели (владение остаётся за unique_ptr в SceneArray).
class SceneArray {
public:
    SceneArray();

    /// Добавить фигуру в конец. Возвращает false если массив полон.
    bool add(std::unique_ptr<Figure> fig);

    /// Удалить фигуру по указателю. Сдвигает оставшиеся элементы влево.
    /// Возвращает false если не найдена.
    bool remove(Figure* fig);

    /// Извлечь фигуру (передать владение вызывающему).
    std::unique_ptr<Figure> extract(Figure* fig);

    /// Количество фигур
    int count() const { return m_count; }

    /// Доступ по индексу
    Figure* get(int idx) const;

    /// Вызвать draw для каждой фигуры (процедура отображения)
    void drawAll(sf::RenderTarget& target) const;

    /// Итерация (для hitTest и др.)
    Figure* hitTest(sf::Vector2f point) const;

    /// Очистить массив
    void clear();

private:
    std::array<std::unique_ptr<Figure>, SCENE_MAX_FIGURES> m_data;
    int m_count = 0;

    /// Сдвинуть элементы влево начиная с позиции pos
    void shiftLeft(int pos);
};

} // namespace core
```

**Файл:** `src/core/SceneArray.cpp` (новый)

```cpp
#include "SceneArray.hpp"

namespace core {

bool SceneArray::add(std::unique_ptr<Figure> fig) {
    if (!fig || m_count >= SCENE_MAX_FIGURES) return false;
    m_data[m_count++] = std::move(fig);
    return true;
}

void SceneArray::shiftLeft(int pos) {
    for (int i = pos; i < m_count - 1; ++i)
        m_data[i] = std::move(m_data[i + 1]);
    m_data[m_count - 1].reset();
    --m_count;
}

bool SceneArray::remove(Figure* fig) {
    for (int i = 0; i < m_count; ++i) {
        if (m_data[i].get() == fig) {
            shiftLeft(i);
            return true;
        }
    }
    return false;
}

std::unique_ptr<Figure> SceneArray::extract(Figure* fig) {
    for (int i = 0; i < m_count; ++i) {
        if (m_data[i].get() == fig) {
            auto ptr = std::move(m_data[i]);
            shiftLeft(i);
            return ptr;
        }
    }
    return nullptr;
}

Figure* SceneArray::get(int idx) const {
    if (idx < 0 || idx >= m_count) return nullptr;
    return m_data[idx].get();
}

void SceneArray::drawAll(sf::RenderTarget& target) const {
    for (int i = 0; i < m_count; ++i)
        m_data[i]->draw(target);
}

Figure* SceneArray::hitTest(sf::Vector2f point) const {
    for (int i = m_count - 1; i >= 0; --i)
        if (m_data[i]->contains(point))
            return m_data[i].get();
    return nullptr;
}

void SceneArray::clear() {
    for (int i = 0; i < m_count; ++i)
        m_data[i].reset();
    m_count = 0;
}

} // namespace core
```

### 4.2 Интеграция `SceneArray` в `Scene`

В `Scene.hpp` заменить `std::vector<std::unique_ptr<Figure>>` на `SceneArray`:

```cpp
// Scene.hpp
#include "SceneArray.hpp"

class Scene {
public:
    void addFigure(std::unique_ptr<Figure> fig) { m_figures.add(std::move(fig)); }
    bool removeFigure(Figure* fig) { return m_figures.remove(fig); }
    std::unique_ptr<Figure> extractFigure(Figure* fig) { return m_figures.extract(fig); }
    Figure* hitTest(sf::Vector2f point) const { return m_figures.hitTest(point); }
    void drawAll(sf::RenderTarget& target, float markerScale = 1.f) const;
    int figureCount() const { return m_figures.count(); }
    Figure* getFigure(int idx) const { return m_figures.get(idx); }
    // ... остальные методы без изменений
private:
    SceneArray m_figures;
    Figure* m_selectedFigure = nullptr;
};
```

> **Важно:** убрать метод `getFigures()` (он возвращал `vector`). Обновить все места в `main.cpp`, где использовался `scene.getFigures()`, заменив на цикл `for (int i = 0; i < scene.figureCount(); ++i) scene.getFigure(i)`.

---

## 5. LR3 — Сериализация / Десериализация

### 5.1 Формат файла

Файл — текстовый (UTF-8). Расширение: `.scene`. Каждая фигура занимает один блок, разделённый пустой строкой.

**Пример:**
```
figure rectangle
name My Box
anchor 100.5 200.3
parent_origin 0.0 0.0
rotation 45.0
scale 1.5 1.0
fill 255 100 100 255
edges 4
  edge 0 width 2.0 color 0 0 0 255
  edge 1 width 2.0 color 0 0 0 255
  edge 2 width 2.0 color 0 0 0 255
  edge 3 width 2.0 color 0 0 0 255
vertices 4
  vertex 0 -50.0 -50.0
  vertex 1 50.0 -50.0
  vertex 2 50.0 50.0
  vertex 3 -50.0 50.0
end

figure compound
name Group1
anchor 300.0 400.0
parent_origin 0.0 0.0
rotation 0.0
scale 1.0 1.0
fill 0 0 0 0
children 2
  child_offset 0 -50.0 0.0
  child_rotation 0 0.0
  child_begin
  figure rectangle
  ...
  end
  child_end
  child_begin
  figure triangle
  ...
  end
  child_end
end
```

### 5.2 Класс `SceneSerializer`

**Файл:** `src/core/SceneSerializer.hpp` (новый)

```cpp
#pragma once
#include "Scene.hpp"
#include <string>

namespace core {

class SceneSerializer {
public:
    /// Сохранить сцену в файл. Возвращает true при успехе.
    static bool save(const Scene& scene, const std::string& filepath);

    /// Загрузить сцену из файла. Добавляет фигуры к существующей сцене.
    /// Возвращает true при успехе.
    static bool load(Scene& scene, const std::string& filepath);

private:
    static void writeFigure(std::ostream& out, const Figure* fig, int indent = 0);
    static std::unique_ptr<Figure> readFigure(std::istream& in);
    
    static void writePrimitive(std::ostream& out, const Figure* fig, 
                               const std::string& typeName, int indent);
    static void writeCompound(std::ostream& out, const CompoundFigure* fig, int indent);
    
    static std::unique_ptr<Figure> readPrimitive(std::istream& in, const std::string& type);
    static std::unique_ptr<CompoundFigure> readCompound(std::istream& in);
    
    /// Вспомогательная: прочитать общие поля Figure (anchor, rotation, edges, vertices…)
    static void readCommonFields(std::istream& in, Figure* fig);
    static void writeCommonFields(std::ostream& out, const Figure* fig, int indent);
};

} // namespace core
```

**Файл:** `src/core/SceneSerializer.cpp` (новый)

Реализовать каждый метод по описанному формату. Использовать `std::ifstream`/`std::ofstream`. Для `readFigure`: читать первую строку, разбирать тип (`rectangle`, `triangle`, `hexagon`, `rhombus`, `trapezoid`, `circle`, `polyline`, `compound`), создавать нужный объект, заполнять поля.

**Пример `save`:**
```cpp
bool SceneSerializer::save(const Scene& scene, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (int i = 0; i < scene.figureCount(); ++i) {
        writeFigure(out, scene.getFigure(i));
        out << "\n";
    }
    return true;
}
```

**Пример `writeFigure`** (для `PolylineFigure`/примитива):
```cpp
void SceneSerializer::writePrimitive(std::ostream& out, const Figure* fig,
                                     const std::string& type, int indent) {
    std::string pad(indent, ' ');
    out << pad << "figure " << type << "\n";
    writeCommonFields(out, fig, indent);
    out << pad << "end\n";
}
```

**При десериализации** (`readPrimitive`): создать нужный класс через `std::make_unique`, передать временные размеры (потом перезаписать через `getVerticesMutable()`).

### 5.3 UI: кнопки Save / Load

**В `Toolbar.cpp`** добавить кнопки Save и Load (или в меню ImGui::MainMenuBar):

```cpp
// В Toolbar::render():
if (ImGui::Button("Save")) {
    // Открыть нативный диалог сохранения файла или использовать ImGui::InputText
    showSaveDialog = true;
}
ImGui::SameLine();
if (ImGui::Button("Load")) {
    showLoadDialog = true;
}
```

Если нативный диалог недоступен, использовать ImGui PopUp с InputText для ввода пути:
```cpp
if (showSaveDialog) {
    ImGui::OpenPopup("Save Scene");
}
if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char pathBuf[512] = "scene.scene";
    ImGui::InputText("File path", pathBuf, sizeof(pathBuf));
    if (ImGui::Button("Save")) {
        core::SceneSerializer::save(scene, pathBuf);
        showSaveDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        showSaveDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
```

Аналогично для Load (после загрузки — сбросить selectedFigure).

Передавать `showSaveDialog`, `showLoadDialog` и ссылку на `scene` в `Toolbar::render()` или выделить отдельный `SaveLoadDialog` класс.

---

## 6. Диаграмма классов (UML)

```
┌─────────────────────────────────────────────────────┐
│                    Figure                           │
│  + anchor: Vector2f                                 │
│  + parentOrigin: Vector2f                           │
│  + fillColor: Color                                 │
│  + rotationAngle: float                             │
│  + scale: Vector2f                                  │
│  + edges: vector<Edge>                              │
│  + lockedSides: vector<bool>                        │
│  + lockedLengths: vector<float>                     │
│  ─────────────────────────────────────────────────  │
│  + draw(target) [virtual]                           │
│  + contains(point): bool [virtual]                  │
│  + getVertices(): vector<Vector2f>& [virtual]       │
│  + getBoundingBox(): FloatRect                      │
│  + getAbsoluteVertex(v): Vector2f                   │
│  + move(delta)                                      │
│  + resetAnchor()                                    │
│  + applyGenericSideLengths(lengths)                 │
│  + hasSideLengths(): bool [virtual]                 │
│  + setSideLengths(lengths) [virtual]                │
│  + getSideName(idx): const char* [virtual]          │
└──────────────────────┬──────────────────────────────┘
                       │ inherits
         ┌─────────────┴──────────────────┐
         │                                │
┌────────▼──────────────────┐   ┌─────────▼──────────────────┐
│     PolylineFigure         │   │      CompoundFigure         │
│  + figureName: string      │   │  + figureName: string       │
│  + setEdgeAngle(i, angle)  │   │  + children: vector<Child>  │
│  + getEdgeAngle(i): float  │   │  ─────────────────────────  │
│  ─────────────────────────│   │  + draw(target)             │
│  + typeName(): string      │   │  + contains(point): bool    │
│  + setSideLengths(...)     │   │  + getVertices()            │
│  + getSideName(idx)        │   │  + typeName(): string       │
└────────┬──────────────────┘   └────────────────────────────┘
         │ inherits
   ┌─────┼──────────────────────────────────────┐
   │     │                                       │
┌──▼──┐ ┌▼────────┐ ┌────────┐ ┌────────┐ ┌────▼──┐ ┌──────┐
│Rect │ │Triangle │ │Hexagon │ │Rhombus │ │Trape- │ │Circle│
│angle│ │         │ │        │ │        │ │zoid   │ │      │
└─────┘ └─────────┘ └────────┘ └────────┘ └───────┘ └──────┘

┌──────────────────────────────────┐
│           SceneArray             │
│  - m_data: array<unique_ptr<Figure>, 1000> │
│  - m_count: int                  │
│  ────────────────────────────── │
│  + add(fig): bool                │
│  + remove(fig*): bool            │
│  + extract(fig*): unique_ptr<>   │
│  + get(idx): Figure*             │
│  + drawAll(target)               │
│  + hitTest(point): Figure*       │
│  + shiftLeft(pos)  [private]     │
│  + clear()                       │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│              Scene               │
│  - m_figures: SceneArray         │
│  - m_selectedFigure: Figure*     │
│  + addFigure(fig)                │
│  + removeFigure(fig*)            │
│  + extractFigure(fig*)           │
│  + hitTest(point): Figure*       │
│  + drawAll(target, markerScale)  │
│  + figureCount(): int            │
│  + getFigure(idx): Figure*       │
│  + setSelectedFigure(fig*)       │
│  + getSelectedFigure(): Figure*  │
│  + setCustomOrigin(pos)          │
│  + resetCustomOrigin()           │
└──────────────────────────────────┘

┌──────────────────────────────────┐
│         SceneSerializer          │
│  + save(scene, path): bool [static] │
│  + load(scene, path): bool [static] │
│  - writeFigure(out, fig, indent) │
│  - readFigure(in): unique_ptr<>  │
│  - writeCommonFields(out, fig)   │
│  - readCommonFields(in, fig)     │
│  - writeCompound(out, fig)       │
│  - readCompound(in)              │
└──────────────────────────────────┘
```

---

## 7. Порядок реализации (чеклист для агента)

### Шаг 1 — Новые классы ядра
- [ ] Создать `src/core/PolylineFigure.hpp` и `PolylineFigure.cpp`
- [ ] Изменить `src/core/Figures.hpp` — базовый класс всех фигур → `PolylineFigure`
- [ ] Обновить `src/core/Figures.cpp` — добавить `figureName` в конструкторах, убрать дублирующие поля
- [ ] Создать `src/core/CompoundFigure.hpp` и `CompoundFigure.cpp`
- [ ] Создать `src/core/SceneArray.hpp` и `SceneArray.cpp`
- [ ] Изменить `src/core/Scene.hpp` и `Scene.cpp` — использовать `SceneArray`, добавить `extractFigure`, `figureCount`, `getFigure`
- [ ] Создать `src/core/SceneSerializer.hpp` и `SceneSerializer.cpp`
- [ ] Обновить `CMakeLists.txt` (добавить новые `.cpp` в `target_sources`)

### Шаг 2 — UI и логика
- [ ] Добавить `Tool::Polyline` и `Tool::CompoundSelect` в `ui/Toolbar.hpp`
- [ ] Добавить кнопки/горячие клавиши в `ui/Toolbar.cpp`
- [ ] Реализовать polyline-рисование с привязкой угла в `main.cpp`
- [ ] Реализовать CompoundSelect-режим (выбор фигур кликами) в `main.cpp`
- [ ] Добавить кнопку "Merge" и диалог имени в `main.cpp`
- [ ] Обновить `PropertiesPanel`: показывать детей CompoundFigure с кнопкой Remove, показывать углы рёбер для PolylineFigure
- [ ] Реализовать Save/Load диалоги (PopUp с InputText)
- [ ] Заменить `scene.getFigures()` → `scene.figureCount()` / `scene.getFigure(i)` везде в `main.cpp`
- [ ] Добавить регистр пользовательских фигур (`userFigureTypes`) и показывать в Toolbar

### Шаг 3 — Верификация
- [ ] Собрать проект: `cd build && cmake .. && make`
- [ ] Запустить: `./GraphEditor`
- [ ] Нарисовать ломаную из 5 точек, убедиться что замыкается
- [ ] Создать CompoundFigure из двух примитивов, убедиться что название появляется в меню
- [ ] Удалить один элемент из CompoundFigure через Properties
- [ ] Сохранить сцену в файл (`Save`), открыть в текстовом редакторе, убедиться что формат читаем
- [ ] Очистить сцену, загрузить файл (`Load`), убедиться что все фигуры восстановлены

---

## 8. Дополнительные замечания

### Горячие клавиши (предлагаемые)
| Клавиша | Действие |
|---------|----------|
| `P`     | Инструмент Polyline |
| `M`     | Режим CompoundSelect |
| `Enter` | Завершить ломаную / подтвердить объединение |
| `Ctrl+S`| Save |
| `Ctrl+O`| Load |

### Привязка к углу при drawing
При зажатой `Shift` в режиме Polyline: угол нового отрезка привязывается к ближайшему кратному 15°.  
При зажатой `Shift` в режиме вращения (уже реализовано) — округление до 15°.

### Клонирование CompoundFigure
Для создания копии CompoundFigure нужна рекурсивная функция глубокого копирования:
```cpp
std::unique_ptr<Figure> cloneFigure(const Figure* src);
```
Реализовать как виртуальный метод `clone()` в `Figure`, переопределить во всех подклассах. Это потребуется и для сериализации (или можно использовать SceneSerializer как механизм клонирования через строку).

### Ограничение массива
Если сцена содержит 1000 фигур и пользователь пытается добавить ещё — показывать ImGui предупреждение: `ImGui::OpenPopup("Scene full")`.
