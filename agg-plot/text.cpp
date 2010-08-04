
#include "text.h"

namespace draw {

  void
  text::rewind(unsigned path_id)
  {
    m_stroke.rewind(path_id);
  }

  unsigned
  text::vertex(double* x, double* y)
  {
    return m_stroke.vertex(x, y);
  }

  void
  text::apply_transform(const agg::trans_affine& m, double as)
  {
    double& x = m_matrix.tx;
    double& y = m_matrix.ty;

    x = m_x;
    y = m_y;

    m.transform(&x, &y);

    m_stroke.approximation_scale(as);
  }

  void
  text::bounding_box(double *x1, double *y1, double *x2, double *y2)
  {
    *x1 = *x2 = m_x;
    *y1 = *y1 = m_y;
  }

  bool
  text::dispose()
  {
    return false;
  }
}
