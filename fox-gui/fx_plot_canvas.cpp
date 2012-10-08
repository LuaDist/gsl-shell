#include "util/agg_color_conv_rgb8.h"

#include "fx_plot_canvas.h"
#include "rendering_buffer_utils.h"
#include "fatal.h"
#include "lua-graph.h"

FXDEFMAP(fx_plot_canvas) fx_plot_canvas_map[]=
{
    FXMAPFUNC(SEL_PAINT,     0, fx_plot_canvas::on_cmd_paint),
//    FXMAPFUNC(SEL_CONFIGURE, 0, fx_plot_canvas::on_configure),
    FXMAPFUNC(SEL_UPDATE,    0, fx_plot_canvas::on_update),
};

FXIMPLEMENT(fx_plot_canvas,FXCanvas,fx_plot_canvas_map,ARRAYNUMBER(fx_plot_canvas_map));

fx_plot_canvas::fx_plot_canvas(FXComposite* p, const char* split_str, FXObject* tgt, FXSelector sel, FXuint opts, FXint x, FXint y, FXint w, FXint h):
    FXCanvas(p, tgt, sel, opts, x, y, w, h),
    m_img(), m_save_img(), m_canvas(0)
{
    split(split_str ? split_str : ".");
}

fx_plot_canvas::~fx_plot_canvas()
{
    delete m_canvas;
}

void fx_plot_canvas::split(const char* split_str)
{
    m_part.parse(split_str);
    m_part.split();

    m_plots.clear();
    plot_ref empty;
    for (unsigned k = 0; k < m_part.get_slot_number(); k++)
        m_plots.add(empty);
}

void fx_plot_canvas::prepare_image_buffer(unsigned ww, unsigned hh)
{
#warning a check should be added to check for allocation fail
    m_img.resize(ww, hh);
    m_canvas = new canvas(m_img, ww, hh, colors::white);
    plots_set_to_dirty();
}

void fx_plot_canvas::ensure_canvas_size(unsigned ww, unsigned hh)
{
    if (m_img.width() != ww || m_img.height() != hh)
    {
        prepare_image_buffer(ww, hh);
    }
}

void fx_plot_canvas::plot_render(plot_ref& ref, const agg::trans_affine& m)
{
    AGG_LOCK();
    ref.plot->draw(*m_canvas, m);
    AGG_UNLOCK();
    ref.is_image_dirty = false;
}

void fx_plot_canvas::plot_render(unsigned index)
{
    plot_ref& ref = m_plots[index];
    int ww = getWidth(), hh = getHeight();
    agg::trans_affine mat = m_part.area_matrix(index, ww, hh);
    plot_render(ref, mat);
}

opt_rect<double>
fx_plot_canvas::plot_render_queue(plot_ref& ref, const agg::trans_affine& m)
{
    opt_rect<double> r, draw_rect;
    AGG_LOCK();
    ref.plot->draw_queue(*m_canvas, m, draw_rect);
    AGG_UNLOCK();
    r.add<rect_union>(draw_rect);
    r.add<rect_union>(ref.dirty_rect);
    ref.dirty_rect = draw_rect;
    return r;
}

void fx_plot_canvas::update_region(const agg::rect_i& r)
{
    FXshort ww = r.x2 - r.x1, hh= r.y2 - r.y1;
    FXImage img(getApp(), NULL, IMAGE_OWNED|IMAGE_SHMI|IMAGE_SHMP, ww, hh);

    const unsigned bpp = 32;
    const unsigned pixel_size = bpp / 8;

    agg::rendering_buffer dest;
    dest.attach((agg::int8u*) img.getData(), ww, hh, -ww * pixel_size);

    rendering_buffer_ro src;
    rendering_buffer_get_const_view(src, m_img, r, gslshell::bpp / 8, true);

    my_color_conv(&dest, &src, color_conv_rgb24_to_rgba32());

    img.create();

    FXDCWindow dc(this);
    dc.drawImage(&img, r.x1, getHeight() - r.y2);
}

void fx_plot_canvas::plot_draw(unsigned index, int canvas_width, int canvas_height)
{
    agg::trans_affine plot_mtx = m_part.area_matrix(index, canvas_width, canvas_height);
    agg::rect_i r = m_part.rect(index, canvas_width, canvas_height);
    int ww = r.x2 - r.x1, hh = r.y2 - r.y1;

    plot_ref& ref = m_plots[index];

    if (plot_is_defined(index))
    {
        if (ref.is_image_dirty)
        {
            m_canvas->clear_box(r);
            plot_render(ref, plot_mtx);
        }

        update_region(r);
    }
    else
    {
        FXDCWindow dc(this);
        dc.setForeground(FXRGB(255,255,255));
        dc.fillRectangle(r.x1, hh - r.y2, ww, hh);
    }

    ref.is_dirty = false;
}

void fx_plot_canvas::plot_draw(unsigned index)
{
    int ww = getWidth(), hh = getHeight();
    plot_draw(index, ww, hh);
}

void
fx_plot_canvas::plot_draw_queue(unsigned index, int canvas_width, int canvas_height, bool draw_all)
{
    plot_ref& ref = m_plots[index];

    if (!ref.plot) return;

    agg::trans_affine plot_mtx = m_part.area_matrix(index, canvas_width, canvas_height);

    opt_rect<double> rect = plot_render_queue(ref, plot_mtx);

    if (draw_all)
    {
        agg::rect_i ri = m_part.rect(index, canvas_width, canvas_height);
        update_region(ri);
    }
    else if (rect.is_defined())
    {
        const int pd = 4;
        const agg::rect_d& r = rect.rect();
        const agg::rect_i box(0, 0, canvas_width, canvas_height);
        agg::rect_i ri(r.x1 - pd, r.y1 - pd, r.x2 + pd, r.y2 + pd);
        ri.clip(box);
        update_region(ri);
    }
}

void fx_plot_canvas::plot_draw_queue(unsigned index, bool draw_all)
{
    int ww = getWidth(), hh = getHeight();
    plot_draw_queue(index, ww, hh, draw_all);
}

void plot_ref::attach(sg_plot* p)
{
    plot = p;
    is_dirty = true;
    is_image_dirty = true;
    dirty_rect.clear();
}

int fx_plot_canvas::attach(sg_plot* p, const char* slot_str)
{
    int index = m_part.get_slot_index(slot_str);
    if (index >= 0)
        m_plots[index].attach(p);
    return index;
}

bool fx_plot_canvas::save_plot_image(unsigned index)
{
    int ww = getWidth(), hh = getHeight();

    if (!m_save_img.ensure_size(ww, hh)) return false;

    plot_ref& ref = m_plots[index];
    if (ref.is_image_dirty)
    {
        agg::trans_affine plot_mtx = m_part.area_matrix(index, ww, hh);
        plot_render(ref, plot_mtx);
    }

    agg::rect_i r = m_part.rect(index, ww, hh);

    rendering_buffer_ro src;
    rendering_buffer_get_const_view(src, m_img, r, 4, true);

    agg::rendering_buffer dest;
    rendering_buffer_get_view(dest, m_save_img, r, 4, true);

    dest.copy_from(src);
    return true;
}

bool fx_plot_canvas::restore_plot_image(unsigned index)
{
    int ww = getWidth(), hh = getHeight();

    if (!image::match(m_img, m_save_img))
        return false;

    agg::rect_i r = m_part.rect(index, ww, hh);

    rendering_buffer_ro src;
    rendering_buffer_get_const_view(src, m_save_img, r, 4, true);

    agg::rendering_buffer dest;
    rendering_buffer_get_view(dest, m_img, r, 4, true);

    dest.copy_from(src);
    return true;
}

long fx_plot_canvas::on_cmd_paint(FXObject *, FXSelector, void *ptr)
{
    int ww = getWidth(), hh = getHeight();
    ensure_canvas_size(ww, hh);
    for (unsigned k = 0; k < m_plots.size(); k++)
    {
        plot_draw(k, ww, hh);
    }
    return 1;
}

long fx_plot_canvas::on_update(FXObject *, FXSelector, void *)
{
    int ww = getWidth(), hh = getHeight();
    ensure_canvas_size(ww, hh);
    for (unsigned k = 0; k < m_plots.size(); k++)
    {
        plot_ref& ref = m_plots[k];
        if (ref.is_dirty)
        {
            plot_draw(k, ww, hh);
        }
    }
    return 1;
}

void fx_plot_canvas::plots_set_to_dirty()
{
    for (unsigned k = 0; k < m_plots.size(); k++)
    {
        plot_ref& ref = m_plots[k];
        ref.is_image_dirty = true;
    }    
}
