
#include "agg_rendering_buffer.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_pixfmt_rgb.h"
#include "agg_trans_affine.h"

#include "text.h"

agg::font_engine_freetype_int32 global_font_eng;
agg::font_cache_manager<agg::font_engine_freetype_int32> global_font_man(global_font_eng);

static bool font_engine_initialized = false;

static void font_engine_initialize()
{
    agg::glyph_rendering gren = agg::glyph_ren_outline;
    global_font_eng.load_font("trebuc.ttf", 0, gren);
    global_font_eng.hinting(true);
    font_engine_initialized = true;
}

agg::font_engine_freetype_int32& gslshell::font_engine()
{
    if (!font_engine_initialized)
        font_engine_initialize();
    return global_font_eng;
}

agg::font_cache_manager<agg::font_engine_freetype_int32>& gslshell::font_manager()
{
    if (!font_engine_initialized)
        font_engine_initialize();
    return global_font_man;
}

namespace draw {

  void
  text::apply_transform(const agg::trans_affine& m, double as)
  {
    m_user_matrix.tx = m_x;
    m_user_matrix.ty = m_y;

    m.transform(&m_user_matrix.tx, &m_user_matrix.ty);

    m_matrix = m_user_matrix;
  }

  void
  text::bounding_box(double *x1, double *y1, double *x2, double *y2)
  {
    *x1 = *x2 = m_x;
    *y1 = *y2 = m_y;
  }

  str
  text::write_svg(int id, agg::rgba8 c, double h)
  {
    const agg::trans_affine& m = m_user_matrix;
    const double eps = 1.0e-6;
    str s;

    const str& content = m_text_label.text();
    if (str_is_null(&content))
      return s;

    str style;
    int hjust = lrint(m_hjustif * 2.0);
    if (hjust == 1)
      style.append(";text-anchor:middle");
    else if (hjust >= 2)
      style.append(";text-anchor:end");

    if (c.r != 0 || c.g != 0 || c.b != 0) {
      char rgbstr[8];
      format_rgb(rgbstr, c);
      style.printf_add(";fill:%s", rgbstr);
    }

    bool need_rotate = !is_unit_matrix(m, eps);

    int txt_size = text_height();

    double x = 0.0, y = - m_vjustif * text_height() * 1.2;

    if (!need_rotate) {
      x = x + m.tx;
      y = svg_y_coord(y + m.ty, h);
    } else {
      y = -y;
    }

    const char* cont = get_text();
    str txt = str::print("<text x=\"%g\" y=\"%g\" id=\"text%i\""        \
                         " style=\"font-size:%i%s\">"                        \
                         " <tspan id=\"tspan%i\">%s</tspan>" \
                         "</text>",
                         x, y, id, txt_size, style.cstr(),
                         id, cont);

    if (need_rotate) {
      s = str::print("<g transform=\"matrix(%g,%g,%g,%g,%g,%g)\">%s</g>",
                     m.sx, m.shx, m.shy, m.sy, m.tx, svg_y_coord(m.ty, h),
                     txt.cstr());
    } else {
      s = txt;
    }

    return s;
  }
}
