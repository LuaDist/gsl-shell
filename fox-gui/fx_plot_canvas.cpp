#include "util/agg_color_conv_rgb8.h"

#include "fx_plot_canvas.h"
#include "rendering_buffer_utils.h"
#include "fatal.h"
#include "lua-graph.h"

FXDEFMAP(fx_plot_canvas) fx_plot_canvas_map[]=
{
    FXMAPFUNC(SEL_PAINT,  0, fx_plot_canvas::on_cmd_paint),
    FXMAPFUNC(SEL_UPDATE, 0, fx_plot_canvas::on_update),
};

FXIMPLEMENT(fx_plot_canvas,FXCanvas,fx_plot_canvas_map,ARRAYNUMBER(fx_plot_canvas_map));

fx_plot_canvas::fx_plot_canvas(FXComposite* p, FXObject* tgt, FXSelector sel, FXuint opts, FXint x, FXint y, FXint w, FXint h):
    FXCanvas(p, tgt, sel, opts, x, y, w, h),
    m_img(0), m_save_img(0), m_canvas(0)
{
}

fx_plot_canvas::~fx_plot_canvas()
{
    delete m_canvas;
}

void fx_plot_canvas::prepare_image_buffer(unsigned ww, unsigned hh)
{
#warning a check should be added to check for allocation fail
    m_img.resize(ww, hh);
    m_canvas = new canvas(m_img, ww, hh, colors::white);
    plots_set_to_dirty();
//    m_dirty_img = true;
}

void fx_plot_canvas::ensure_canvas_size(unsigned ww, unsigned hh)
{
    if (m_img.width() != ww || m_img.height() != hh)
    {
//        m_area_mtx.sx = ww;
//        m_area_mtx.sy = hh;
        prepare_image_buffer(ww, hh);
    }
}

void fx_plot_canvas::clear_plot_area(unsigned index)
{
    const agg::rect_i& rect = m_part.rect(index);
    m_canvas->clear_box(rect);
}

void fx_plot_canvas::plot_render(plot_ref& ref, const agg::trans_affine& m)
{
    AGG_LOCK();
    ref.plot->draw(*m_canvas, m);
    AGG_UNLOCK();
    ref.is_image_dirty = false;
}

void fx_plot_canvas::plot_draw(unsigned index, const agg::trans_affine& area_mtx)
{
    FXDCWindow dc(this);
//    int ww = getWidth(), hh = getHeight();

//    ensure_canvas_size(index, ww, hh);

    agg::rect_i r = rect_of_slot_matrix<int>(m);
    int ww = r.x2 - r.x1, hh = r.y2 - r.y1;

    plot_ref& ref = m_plots[index];

    if (plot_is_defined(index))
    {
        agg::trans_affine plot_mtx = m_part.area_matrix(index, area_mtx);

        if (m_dirty_img)
        {
            clear_plot_area(index);
            plot_render(ref, plot_mtx);
        }

        FXImage area_img(getApp(), NULL, IMAGE_OWNED|IMAGE_SHMI|IMAGE_SHMP, ww, hh);
        agg::int8u* data = (agg::int8u*) area_img.getData();
        agg::rendering_buffer area_img_rbuf(data, ww, hh, - ww * 4);

        rendering_buffer_ro src_rbuf;
        rendering_buffer_get_const_view(src_rbuf, m_img, r, image_pixel_width, true);

        my_color_conv(&area_img_rbuf, &src_rbuf, color_conv_rgb24_to_rgba32());
        area_img.create();

        dc.drawImage(&area_img, r.x1, r.y1);
    }
    else
    {
        dc.setForeground(FXRGB(255,255,255));
        dc.fillRectangle(r.x1, r.y1, ww, hh);
    }

    m_dirty_flag = false;
}

void fx_plot_canvas::update_region(const agg::rect_base<int>& _r)
{
    int iw = m_img.width(), ih = m_img.height();
    const agg::rect_base<int> b(0, 0, iw, ih);
    agg::rect_base<int> r = agg::intersect_rectangles(_r, b);

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

opt_rect<double> fx_plot_canvas::plot_render_queue(const agg::trans_affine& m)
{
    opt_rect<double> r, draw_rect;
    AGG_LOCK();
    m_plot->draw_queue(*m_canvas, m, draw_rect);
    AGG_UNLOCK();
    r.add<rect_union>(draw_rect);
    r.add<rect_union>(m_dirty_rect);
    m_dirty_rect = draw_rect;
    return r;
}

void fx_plot_canvas::plot_draw_queue(const agg::trans_affine& m, bool draw_all)
{
    if (!m_canvas || !m_plot) return;

    opt_rect<double> rect = plot_render_queue(m);

    if (draw_all)
    {
        const agg::rect_base<int> ri(0, 0, getWidth(), getHeight());
        update_region(ri);
    }
    else if (rect.is_defined())
    {
        const int pd = 4;
        const agg::rect_base<double>& r = rect.rect();
        const agg::rect_base<int> ri(r.x1 - pd, r.y1 - pd, r.x2 + pd, r.y2 + pd);
        update_region(ri);
    }
}

bool fx_plot_canvas::save_image()
{
    int ww = getWidth(), hh = getHeight();
    if (!m_img.defined() || !m_save_img.resize(ww, hh)) return false;
    if (m_dirty_img)
        plot_render(m_area_mtx);
    m_save_img.copy_from(m_img);
    return true;
}

bool fx_plot_canvas::restore_image()
{
    if (!image::match(m_img, m_save_img))
        return false;
    m_img.copy_from(m_save_img);
    return true;
}

void fx_plot_canvas::attach(unsigned slot_id, sg_plot* p)
{
    m_plots[slot_id] = p;
    m_dirty_flag = true;
    m_dirty_img = true;
}

long fx_plot_canvas::on_cmd_paint(FXObject *, FXSelector, void *ptr)
{
    FXEvent* ev = (FXEvent*) ptr;
    plot_draw(m_area_mtx);
    return 1;
}

long fx_plot_canvas::on_update(FXObject *, FXSelector, void *)
{
    bool need_upd = m_dirty_flag;
    if (need_upd)
        plot_draw(m_area_mtx);
    return (need_upd ? 1 : 0);
}
