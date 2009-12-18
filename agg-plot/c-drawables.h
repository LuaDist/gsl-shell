#ifndef CPLOT_CINTFC_H
#define CPLOT_CINTFC_H

#include "defs.h"

#define CPLOT    struct my_c_plot
#define CVERTSRC struct my_c_vs
#define CPATH    struct my_c_path
#define CTEXT    struct my_c_text

#ifdef __cplusplus
typedef plot<vertex_source, ref_manager> plot_type;
#endif

__BEGIN_DECLS

struct cmd_call_stack;
struct trans_spec;

CPLOT;
CVERTSRC;
CPATH;

extern CPLOT *  plot_new      (int with_units);
extern void     plot_free     (CPLOT *p);
extern void     plot_add      (CPLOT *p, CVERTSRC *src, const char *color,
			       struct trans_spec *post, struct trans_spec *pre);
extern void     plot_add_line (CPLOT *p, CVERTSRC *src, const char *color);

extern void     vertex_source_ref   (CVERTSRC *p);
extern void     vertex_source_unref (CVERTSRC *p);

extern CPATH *  path_new  ();
extern void     path_cmd   (CPATH *p, int cmd, struct cmd_call_stack *stack);



extern CVERTSRC *  ellipse_new  (double x, double y, double rx, double ry);
extern void        ellipse_free (CVERTSRC *e);

extern CTEXT *     text_new        (double size, double width);
extern void        text_set_text   (CTEXT *t, const char *text);
extern void        text_set_point  (CTEXT *t, double x, double y);

__END_DECLS

#ifndef __cplusplus
typedef CPLOT    plot;
typedef CVERTSRC vertex_source;
typedef CPATH    path;
#endif

#endif
