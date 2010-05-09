#ifndef AGGPLOT_CPLOT_H
#define AGGPLOT_CPLOT_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "vertex-source.h"

#include "utils.h"
#include "drawables.h"
#include "canvas.h"
#include "units.h"
#include "resource-manager.h"

#include "agg_conv_transform.h"
#include "agg_color_rgba.h"
#include "agg_path_storage.h"
#include "agg_array.h"

template<class VertexSource, class resource_manager = no_management>
class plot {

  class container {
  public:
    VertexSource* vs;
    agg::rgba8 color;
    bool outline;

    container(): vs(NULL), color(), outline(false) {};

    container(VertexSource* vs, agg::rgba8 c, bool as_outline): 
      vs(vs), color(c), outline(as_outline)
    {};

    ~container() {};

    void bounding_box(double *x1, double *y1, double *x2, double *y2)
    {
      VertexSource& vsi = get_vertex_source();
      vsi.bounding_box(x1, y1, x2, y2);
    };

    VertexSource& get_vertex_source() { return *vs; };
  };

public:
  plot() : m_elements(), m_trans(), m_bbox_updated(true) {
    m_title_size = 32;
    m_title = new char[m_title_size];
    m_title[0] = 0;
  };

  virtual ~plot() 
  {
#ifdef DEBUG_PLOT
    fprintf(stderr, "freeing plot: %p\n", this);
#endif

    for (unsigned j = 0; j < m_elements.size(); j++)
      {
	container& d = m_elements[j];
	resource_manager::dispose(d.vs);
      }

    delete [] m_title;
  };

  void set_title(const char *text) {
    unsigned int len = strlen(text);

    if (m_title_size < len + 1)
      {
	delete [] m_title;
	m_title = new char[len+1];
	m_title_size = len+1;
      }

    memcpy(m_title, text, len+1);
  };

  const char *get_title() const { return m_title; };

  void add(VertexSource* vs, agg::rgba8 color, bool outline = false) 
  { 
    container d(vs, color, outline);
    m_elements.add(d);
    m_bbox_updated = false;
    resource_manager::acquire(vs);
  };

  virtual void draw(canvas &canvas);

protected:
  void draw_elements(canvas &canvas);
  void draw_title(canvas& canvas);
  void calc_bounding_box();
  virtual void trans_matrix_update();

  static void viewport_scale(agg::trans_affine& trans);

  agg::pod_bvector<container> m_elements;
  agg::trans_affine m_trans;

  // bounding box
  bool   m_bbox_updated;
  double m_x1, m_y1;
  double m_x2, m_y2;

  char *m_title;
  unsigned int m_title_size;
};

template <class VS, class RM>
void plot<VS,RM>::draw(canvas &canvas)
{
  trans_matrix_update();
  draw_title(canvas);
  draw_elements(canvas);
};

template <class VS, class RM>
void plot<VS,RM>::draw_title(canvas &canvas)
{
  double xt = 0.5, yt = 1;

  agg::trans_affine m;
  this->viewport_scale(m);
  canvas.scale(m);

  agg::gsv_text title;
  agg::conv_stroke<agg::gsv_text> titlestroke(title);

  title.size(12.0);
  title.text(m_title);
  titlestroke.width(1.0);

  m.transform(&xt, &yt);

  xt += -title.text_width() / 2;
  yt += 10.0;

  title.start_point(xt, yt);
  canvas.draw(titlestroke, agg::rgba(0, 0, 0));
}

template<class VS, class RM>
void plot<VS,RM>::draw_elements(canvas &canvas)
{
  agg::trans_affine m = m_trans;
  viewport_scale(m);
  canvas.scale(m);

  for (unsigned j = 0; j < m_elements.size(); j++)
    {
      container& d = m_elements[j];
      VS& vs = d.get_vertex_source();
      vs.apply_transform(m, 1.0);

      if (d.outline)
	canvas.draw_outline(vs, d.color);
      else
	canvas.draw(vs, d.color);
    }
}

template<class VS, class RM>
void plot<VS,RM>::trans_matrix_update()
{
  if (! m_bbox_updated)
  {
    calc_bounding_box();

    double fx = m_x2 - m_x1, fy = m_y2 - m_y1;
    m_trans.reset();
    m_trans.scale(1/fx, 1/fy);
    m_trans.translate(-m_x1/fx, -m_y1/fy);
    m_bbox_updated = true;
  }
}

template<class VS, class RM>
void plot<VS,RM>::calc_bounding_box()
{
  bool is_set = false;

  for (unsigned j = 0; j < m_elements.size(); j++)
  {
    container& d = m_elements[j];

    double sx1, sy1, sx2, sy2;
    d.vs->bounding_box(&sx1, &sy1, &sx2, &sy2);
      
    if (! is_set)
    {
      m_x1 = sx1;
      m_x2 = sx2;
      m_y1 = sy1;
      m_y2 = sy2;

      is_set = true;
    }
    else
    {
      bbox_enlarge(&m_x1, &m_y1, &m_x2, &m_y2, sx1, sy1);
      bbox_enlarge(&m_x1, &m_y1, &m_x2, &m_y2, sx2, sy2);
    }
  }
}

template<class VS, class RM>
void plot<VS,RM>::viewport_scale(agg::trans_affine& m)
{
  const double xoffs = 0.09375, yoffs = 0.09375;
  static agg::trans_affine rsz(1-2*xoffs, 0.0, 0.0, 1-2*yoffs, xoffs, yoffs);
  trans_affine_compose (m, rsz);
}

#endif
