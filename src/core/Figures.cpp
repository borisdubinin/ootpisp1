#include "Figures.hpp"
#include <algorithm>
#include <cmath>

namespace core {

static constexpr float PI = 3.14159265358979323846f;

// ─── Rectangle ──────────────────────────────────────────────────────────────

Rectangle::Rectangle(float width, float height)
    : m_width(width), m_height(height) {
  edges.resize(4);
  m_vertices = {{-m_width / 2.f, -m_height / 2.f},
                {m_width / 2.f, -m_height / 2.f},
                {m_width / 2.f, m_height / 2.f},
                {-m_width / 2.f, m_height / 2.f}};
}

// Rectangle has 4 sides: top, right, bottom, left (with right angles).
// We interpret lengths[0]=top, [1]=right, [2]=bottom, [3]=left.
// Build keeping bottom-left corner at origin, center polygon at zero.
void Rectangle::setSideLengths(const std::vector<float> &lengths) {
  if (lengths.size() < 4)
    return;
  float top = std::max(1.f, lengths[0]);
  float right = std::max(1.f, lengths[1]);
  float bottom = std::max(1.f, lengths[2]);
  float left = std::max(1.f, lengths[3]);

  // Build a right-angle quadrilateral: BL->BR (bottom), BR->TR (right),
  // TR->TL (top), TL->BL (left). Angles auto-adjust—since it's non-uniform,
  // we use an actual trapezoid derivation where top and bottom are horizontal
  // and legs connect them. This is the most intuitive "quadrilateral"
  // interpretation where the four entered lengths are honoured and the shape
  // is kept flat.  We use the same geometric construction as Trapezoid.
  float w = bottom;
  float tw = top;
  float h = left; // left leg length is the height guide

  // Recalculate: place BL at (-w/2, h/2), BR at (w/2, h/2).
  // The right leg has length 'right', the left leg has length 'left'.
  // Horizontal offsets of top corners:
  float dx_left = 0.f; // will solve for left corner x-offset
  float dx_right = 0.f;
  // Actually use a proper trapezoid solver:
  // BL=(0,0), BR=(w,0).
  // TR = BR + (dx_right, -h_r) where h_r^2 + dx_right^2 = right^2
  // TL = BL + (dx_left,  -h_l) where h_l^2 + dx_left^2  = left^2
  // top = TR.x - TL.x = (w + dx_right) - dx_left = tw
  // => dx_right - dx_left = tw - w
  // We additionally require that TL and TR are at the same height (flat top):
  // h_l == h_r => sqrt(left^2 - dx_left^2) == sqrt(right^2 - dx_right^2)
  //
  // Solve: let D = tw - w      (signed)
  //    dx_right = dx_left + D
  //    left^2 - dx_left^2 == right^2 - (dx_left+D)^2
  //    left^2 - dx_left^2 == right^2 - dx_left^2 - 2*D*dx_left - D^2
  //    left^2              == right^2 - 2*D*dx_left - D^2
  //    2*D*dx_left         == right^2 - D^2 - left^2
  //    dx_left = (right^2 - D^2 - left^2) / (2*D)  if D != 0

  float D = tw - w;
  if (std::abs(D) < 1e-4f) {
    // Symmetric: both legs are symmetric
    float h_val = std::sqrt(std::max(0.f, left * left - (D / 2.f) * (D / 2.f)));
    dx_left = D / 2.f;
    dx_right = D / 2.f;
    (void)h_val;
  } else {
    dx_left = (right * right - D * D - left * left) / (2.f * D);
    dx_right = dx_left + D;
  }
  float h_val = std::sqrt(std::max(1.f, left * left - dx_left * dx_left));

  float BLx = 0.f, BLy = 0.f;
  float BRx = w, BRy = 0.f;
  float TRx = BRx + dx_right, TRy = -h_val;
  float TLx = BLx + dx_left, TLy = -h_val;

  // Centre at origin
  float cx = (BLx + BRx + TRx + TLx) / 4.f;
  float cy = (BLy + BRy + TRy + TLy) / 4.f;

  m_vertices = {{TLx - cx, TLy - cy},
                {TRx - cx, TRy - cy},
                {BRx - cx, BRy - cy},
                {BLx - cx, BLy - cy}};
  m_width = bottom;
  m_height = h_val;
}

// ─── Triangle ───────────────────────────────────────────────────────────────

Triangle::Triangle(float base, float height) : m_base(base), m_height(height) {
  edges.resize(3);
  m_vertices = {{0.f, -m_height / 2.f},
                {m_base / 2.f, m_height / 2.f},
                {-m_base / 2.f, m_height / 2.f}};
}

// Triangle uniquely defined by 3 side lengths (Law of Cosines).
void Triangle::setSideLengths(const std::vector<float> &lengths) {
  if (lengths.size() < 3)
    return;
  float a =
      std::max(1.f, lengths[0]); // side opposite to vertex A (bottom side)
  float b = std::max(1.f, lengths[1]); // side right
  float c = std::max(1.f, lengths[2]); // side left

  // Clamp to valid triangle: each side < sum of other two
  float sumBC = b + c, sumAC = a + c, sumAB = a + b;
  if (a >= sumBC)
    a = sumBC - 1.f;
  if (b >= sumAC)
    b = sumAC - 1.f;
  if (c >= sumAB)
    c = sumAB - 1.f;

  // Place base (a) horizontally.
  // V0 = (0, 0), V1 = (a, 0)
  // V2 = (x, y) with |V2-V0|=c, |V2-V1|=b
  // x^2 + y^2 = c^2
  // (x-a)^2 + y^2 = b^2 => x^2 - 2ax + a^2 + y^2 = b^2
  // c^2 - 2ax + a^2 = b^2 => x = (c^2 + a^2 - b^2) / (2a)
  float x2 = (c * c + a * a - b * b) / (2.f * a);
  float y2sq = c * c - x2 * x2;
  float y2 = -std::sqrt(std::max(0.f, y2sq)); // point upward

  // Center of mass
  float cx = (0.f + a + x2) / 3.f;
  float cy = (0.f + 0.f + y2) / 3.f;

  m_vertices = {
      {x2 - cx, y2 - cy},  // top
      {a - cx, 0.f - cy},  // bottom-right
      {0.f - cx, 0.f - cy} // bottom-left
  };
  m_base = a;
  m_height = std::abs(y2);
}

// ─── Hexagon ─────────────────────────────────────────────────────────────────

Hexagon::Hexagon(float width, float height) : m_width(width), m_height(height) {
  edges.resize(6);
  m_vertices.resize(6);
  for (int i = 0; i < 6; ++i) {
    float angle = i * 60.f * PI / 180.f;
    m_vertices[i] = {(m_width / 2.f) * std::cos(angle),
                     (m_height / 2.f) * std::sin(angle)};
  }
}

// Hexagon: iterative position-based constraint solver.
// Target: each edge i has length lengths[i]. We run spring relaxation,
// fixing centroid at origin.
void Hexagon::setSideLengths(const std::vector<float> &lengths) {
  if (lengths.size() < 6)
    return;
  const int n = 6;
  std::vector<float> L(n);
  for (int i = 0; i < n; ++i)
    L[i] = std::max(1.f, lengths[i]);

  // Start from current positions
  struct V2 {
    float x, y;
  };
  std::vector<V2> pts(n);
  for (int i = 0; i < n; ++i) {
    pts[i] = {m_vertices[i].x, m_vertices[i].y};
  }

  const int ITERS = 500;
  const float STIFFNESS = 0.5f;

  for (int iter = 0; iter < ITERS; ++iter) {
    // Edge constraints
    for (int i = 0; i < n; ++i) {
      int j = (i + 1) % n;
      float dx = pts[j].x - pts[i].x;
      float dy = pts[j].y - pts[i].y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < 1e-6f)
        continue;
      float err = (dist - L[i]) / dist;
      pts[i].x += STIFFNESS * 0.5f * err * dx;
      pts[i].y += STIFFNESS * 0.5f * err * dy;
      pts[j].x -= STIFFNESS * 0.5f * err * dx;
      pts[j].y -= STIFFNESS * 0.5f * err * dy;
    }
    // Re-centre
    float cx = 0.f, cy = 0.f;
    for (auto &p : pts) {
      cx += p.x;
      cy += p.y;
    }
    cx /= n;
    cy /= n;
    for (auto &p : pts) {
      p.x -= cx;
      p.y -= cy;
    }
  }

  for (int i = 0; i < n; ++i) {
    m_vertices[i] = {pts[i].x, pts[i].y};
  }
}

// ─── Rhombus ─────────────────────────────────────────────────────────────────

Rhombus::Rhombus(float width, float height) : m_width(width), m_height(height) {
  edges.resize(4);
  m_vertices = {{0.f, -m_height / 2.f},
                {m_width / 2.f, 0.f},
                {0.f, m_height / 2.f},
                {-m_width / 2.f, 0.f}};
}

// For Rhombus we treat the 4 inputs as: half-diagonal-top, half-diagonal-right,
// half-diagonal-bottom, half-diagonal-left (i.e. distances from center to
// tips). The side-lengths in the UI correspond to distances between adjacent
// tips. We use the isometric trapezoid solver but keep it a proper rhombus-like
// shape. lengths[0]=top-edge, [1]=right-edge, [2]=bottom-edge, [3]=left-edge.
// A kite shape is built from two diagonal values derived from the four
// side-lengths.
void Rhombus::setSideLengths(const std::vector<float> &lengths) {
  if (lengths.size() < 4)
    return;
  float a = std::max(1.f, lengths[0]); // top edge  (diag_v_top to diag_h_right)
  float b = std::max(1.f, lengths[1]); // right edge
  float c = std::max(1.f, lengths[2]); // bottom edge
  float d = std::max(1.f, lengths[3]); // left edge

  // Use spring relaxation same as hexagon but just 4 points
  struct V2 {
    float x, y;
  };
  std::vector<V2> pts = {{m_vertices[0].x, m_vertices[0].y},
                         {m_vertices[1].x, m_vertices[1].y},
                         {m_vertices[2].x, m_vertices[2].y},
                         {m_vertices[3].x, m_vertices[3].y}};
  std::vector<float> L = {a, b, c, d};
  const int n = 4;

  for (int iter = 0; iter < 500; ++iter) {
    for (int i = 0; i < n; ++i) {
      int j = (i + 1) % n;
      float dx = pts[j].x - pts[i].x;
      float dy = pts[j].y - pts[i].y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < 1e-6f)
        continue;
      float err = (dist - L[i]) / dist;
      pts[i].x += 0.5f * 0.5f * err * dx;
      pts[i].y += 0.5f * 0.5f * err * dy;
      pts[j].x -= 0.5f * 0.5f * err * dx;
      pts[j].y -= 0.5f * 0.5f * err * dy;
    }
    float cx = 0.f, cy = 0.f;
    for (auto &p : pts) {
      cx += p.x;
      cy += p.y;
    }
    cx /= n;
    cy /= n;
    for (auto &p : pts) {
      p.x -= cx;
      p.y -= cy;
    }
  }
  for (int i = 0; i < n; ++i)
    m_vertices[i] = {pts[i].x, pts[i].y};
}

// ─── Trapezoid ───────────────────────────────────────────────────────────────

Trapezoid::Trapezoid(float topWidth, float bottomWidth, float height)
    : m_topWidth(topWidth), m_bottomWidth(bottomWidth), m_height(height) {
  edges.resize(4);
  m_vertices = {{-m_topWidth / 2.f, -m_height / 2.f},
                {m_topWidth / 2.f, -m_height / 2.f},
                {m_bottomWidth / 2.f, m_height / 2.f},
                {-m_bottomWidth / 2.f, m_height / 2.f}};
}

// Trapezoid with parallel top and bottom.
// lengths: [0]=top, [1]=right leg, [2]=bottom, [3]=left leg.
// We solve for the height and horizontal offsets of the legs using Pythagoras.
void Trapezoid::setSideLengths(const std::vector<float> &lengths) {
  if (lengths.size() < 4)
    return;
  float top = std::max(1.f, lengths[0]);
  float right = std::max(1.f, lengths[1]);
  float bottom = std::max(1.f, lengths[2]);
  float left = std::max(1.f, lengths[3]);

  // Same geometric solver as Rectangle::setSideLengths
  // BL=(0,0), BR=(bottom,0), TR=(bottom+dx_r, -h), TL=(dx_l, -h)
  // top = TR.x - TL.x = bottom + dx_r - dx_l = top
  // |left_leg|^2 = dx_l^2 + h^2 = left^2
  // |right_leg|^2 = dx_r^2 + h^2 = right^2
  // dx_r - dx_l = top - bottom = D
  float D = top - bottom;
  float dx_left, dx_right;
  if (std::abs(D) < 1e-4f) {
    dx_left = 0.f;
    dx_right = 0.f;
  } else {
    // left^2 - dx_left^2 = right^2 - (dx_left+D)^2
    // dx_left = (right^2 - D^2 - left^2) / (2*D)
    dx_left = (right * right - D * D - left * left) / (2.f * D);
    dx_right = dx_left + D;
  }
  float h = std::sqrt(std::max(1.f, left * left - dx_left * dx_left));

  float BLx = 0.f, BLy = 0.f;
  float BRx = bottom, BRy = 0.f;
  float TRx = BRx + dx_right, TRy = -h;
  float TLx = BLx + dx_left, TLy = -h;

  float cx = (BLx + BRx + TRx + TLx) / 4.f;
  float cy = (BLy + BRy + TRy + TLy) / 4.f;

  m_vertices = {{TLx - cx, TLy - cy},
                {TRx - cx, TRy - cy},
                {BRx - cx, BRy - cy},
                {BLx - cx, BLy - cy}};
  m_topWidth = top;
  m_bottomWidth = bottom;
  m_height = h;
}

// ─── Circle ──────────────────────────────────────────────────────────────────

Circle::Circle(float radiusX, float radiusY)
    : m_radiusX(radiusX), m_radiusY(radiusY) {
  int segments = 60;
  edges.resize(1);
  m_vertices.resize(segments);
  for (int i = 0; i < segments; ++i) {
    float angle = i * 2.f * PI / segments;
    m_vertices[i] = {m_radiusX * std::cos(angle), m_radiusY * std::sin(angle)};
  }
}

} // namespace core
