#ifndef FOXGUI_FX_PLOT_CANVAS_H
#define FOXGUI_FX_PLOT_CANVAS_H

#include <fx.h>
#include <agg_rendering_buffer.h>

#include "sg_object.h"
#include "plot-auto.h"
#include "canvas.h"

class fx_plot_canvas : public FXCanvas {
  FXDECLARE(fx_plot_canvas)

public:
  typedef plot<sg_object, manage_owner> plot_type;

  fx_plot_canvas(FXComposite* p, FXObject* tgt=NULL, FXSelector sel=0,
		 FXuint opts=FRAME_NORMAL,
		 FXint x=0, FXint y=0, FXint w=0, FXint h=0);

  ~fx_plot_canvas();

  void attach(plot_type* p);
  void draw(FXEvent* event);

  long on_cmd_paint(FXObject *, FXSelector, void *);
  long on_update(FXObject *, FXSelector, void *);

protected:
  fx_plot_canvas() {}

private:
  void prepare_image_buffer(int ww, int hh);
  void ensure_canvas_size(int ww, int hh);

  agg::int8u* m_img_data;
  int m_img_width, m_img_height;
  agg::rendering_buffer m_rbuf;
  plot_type* m_plot;
  canvas* m_canvas;
  bool m_dirty_flag;
};

#endif
