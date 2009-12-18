
#include "agg_color_rgba.h"
#include "agg_math_stroke.h"

// #include "units-plot.h"
#include "plot.h"
#include "trans.h"
#include "vertex-source.h"
#include "lua-plot-priv.h"
#include "c-drawables.h"

struct property_reg line_cap_properties[] = {
  {(int) agg::butt_cap,   "butt"  },
  {(int) agg::square_cap, "square"},
  {(int) agg::round_cap,  "round" },
  {0, NULL}
};

struct property_reg line_join_properties[] = {
  {(int) agg::miter_join,        "miter"      },
  {(int) agg::miter_join_revert, "miter.rev"  },
  {(int) agg::round_join,        "round"      },
  {(int) agg::bevel_join,        "bevel"      },
  {(int) agg::miter_join_round,  "miter.round"},
  {0, NULL}
};

typedef my::path path_type;
typedef my::ellipse ellipse_type;

static agg::rgba8
color_lookup (const char *color_str)
{
  const char *p = color_str;
  agg::rgba8 c;
  int val = 180;

  if (strcmp (p, "white") == 0)
    {
      c = agg::rgba8(255, 255, 255);
      return c;
    }

  if (strncmp (p, "light", 5) == 0)
    {
      val = 255;
      p += 5;
    }
  else if (strncmp (p, "dark", 4) == 0)
    {
      val = 120;
      p += 4;
    }

  if (strcmp (p, "red") == 0)
    c = agg::rgba8(val, 0, 0);
  else if (strcmp (p, "green") == 0)
    c = agg::rgba8(0, val, 0);
  else if (strcmp (p, "blue") == 0)
    c = agg::rgba8(0, 0, val);
  else if (strcmp (p, "cyan") == 0)
    c = agg::rgba8(0, val, val);
  else if (strcmp (p, "magenta") == 0)
    c = agg::rgba8(val, 0, val);
  else if (strcmp (p, "yellow") == 0)
    c = agg::rgba8(val, val, 0);
  else if (strcmp (p, "gray") == 0)
    c = agg::rgba8(val, val, val);
  else
    c = agg::rgba8(0, 0, 0);

  return c;
}

CPLOT *
plot_new(int with_units)
{
  plot_type *p = new plot_type();
  return (CPLOT *) p; 
}

void plot_free (CPLOT* _d)
{
  plot_type* p = (plot_type*) _d;
  delete p;
}

static vertex_source*
build_pipeline (vertex_source* in, struct trans_spec *base)
{
  struct trans_spec *spec;
  vertex_source* curr;
  size_t n;

  for (spec = base, n = 0; spec->tag != trans_end; spec++, n++) {}
  
  for (spec = base + (n-1); spec >= base; spec--)
    {
      switch (spec->tag)
	{
	  double *len;
	  trans::dash* ds;
	  trans::stroke* stroke;
	case trans_stroke:
	  stroke = new trans::stroke(in, spec->content.stroke.width);
	  stroke->line_cap((agg::line_cap_e) spec->content.stroke.line_cap);
	  stroke->line_join((agg::line_join_e) spec->content.stroke.line_join);
	  curr = stroke;
	  break;
	case trans_curve:
	  curr = new trans::curve(in);
	  break;
	case trans_dash:
	  ds = new trans::dash(in);
	  len = spec->content.dash.len;
	  ds->add_dash(len[0], len[1]);
	  curr = ds;
	  break;
	case trans_end:
	  ;
	}

      in = curr;
    }
  
  return in;
}

void plot_add(CPLOT *_p, CVERTSRC *_vs, const char *color,
	      struct trans_spec *post, struct trans_spec *pre)
{
  plot_type* p = (plot_type*) _p;
  vertex_source* vs = (vertex_source*) _vs;
  vertex_source* curr;

  curr = build_pipeline (vs, pre);

  if (curr->need_resize())
    curr = new trans::resize(curr);

  curr = build_pipeline (curr, post);

  p->add(curr, color_lookup(color));
}

void plot_add_line (CPLOT *_p, CVERTSRC *_vs, const char *color)
{
  plot_type* p = (plot_type*) _p;
  vertex_source* vs = (vertex_source*) _vs;

  trans::line* line = new trans::line(vs);
  p->add(line, color_lookup(color));
}

CPATH* path_new()
{
  path_type* p = new path_type();
  return (CPATH *) p;
}

void vertex_source_ref (CVERTSRC *_p)
{
  vertex_source* p = (vertex_source*) _p;
  p->ref();
}

void vertex_source_unref (CVERTSRC* _p)
{
  vertex_source* p = (vertex_source*) _p;
  unsigned rc = p->unref();
  if (rc == 0)
    delete p;
}

void
path_cmd (CPATH *_p, int _cmd, struct cmd_call_stack *s)
{
  path_type* p = (path_type*) _p;
  agg::path_storage& ps = p->get_path();
  path_cmd_e cmd = (path_cmd_e) _cmd;

  switch (cmd)
    {
    case CMD_MOVE_TO:
      ps.move_to (s->f[0], s->f[1]);
      break;
    case CMD_LINE_TO:
      ps.line_to (s->f[0], s->f[1]);
      break;
    case CMD_CLOSE:
      ps.close_polygon ();
      break;
    case CMD_ARC_TO:
      ps.arc_to (s->f[0], s->f[1], s->f[2], s->b[0], s->b[1], s->f[3], s->f[4]);
      break;
    case CMD_CURVE3:
      ps.curve3 (s->f[0], s->f[1], s->f[2], s->f[3]);
      break;
    case CMD_CURVE4:
      ps.curve4 (s->f[0], s->f[1], s->f[2], s->f[3], s->f[4], s->f[5]);
      break;
    case CMD_ERROR:
      ;
    }
}

CTEXT* text_new (double size, double width)
{
  my::text *t = new my::text(size, width);
  return (CTEXT *) t;
}

void text_set_text (CTEXT *_t, const char *text)
{
  my::text *t = (my::text *) _t;
  t->set_text(text);
}

void text_set_point (CTEXT *_t, double x, double y)
{
  my::text *t = (my::text *) _t;
  t->start_point (x, y);
}

CVERTSRC *
ellipse_new (double x, double y, double rx, double ry)
{
  ellipse_type* e = new ellipse_type(x, y, rx, ry);
  return (CVERTSRC *) e;
}

void
ellipse_free (CVERTSRC *_e)
{
  ellipse_type* e = (ellipse_type*) _e;
  delete e;
};
