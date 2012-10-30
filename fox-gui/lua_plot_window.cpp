
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "lua_plot_window.h"
#include "gsl_shell_app.h"
#include "window_registry.h"
#include "fx_plot_window.h"
#include "lua-cpp-utils.h"
#include "lua-graph.h"
#include "gs-types.h"
#include "plot.h"
#include "canvas_svg.h"

__BEGIN_DECLS

static int fox_window_layout(lua_State* L);
static int fox_window_export_svg (lua_State *L);

static const struct luaL_Reg fox_window_functions[] =
{
    {"window",         fox_window_new},
    {NULL, NULL}
};

static const struct luaL_Reg fox_window_methods[] =
{
    {"layout",         fox_window_layout },
    {"attach",         fox_window_attach        },
    {"close",          fox_window_close        },
    {"refresh",        fox_window_slot_refresh        },
    {"update",         fox_window_slot_update },
    {"save_svg",       fox_window_export_svg },
    {NULL, NULL}
};

enum window_status_e { not_ready, running, closed };

struct lua_fox_window
{
    fx_plot_window* window;
    gsl_shell_app* app;
    enum window_status_e status;
};

__END_DECLS

typedef plot<manage_owner> sg_plot;

class window_mutex {
public:
    window_mutex(lua_State* L, int index)
    {
        m_handle = (lua_fox_window*) gs_is_userdata(L, index, GS_WINDOW);
        if (m_handle)
            m_handle->app->lock();
    }

    ~window_mutex()
    {
        if (m_handle)
            m_handle->app->unlock();
    }

    bool is_defined() { return (m_handle != NULL); }
    bool is_running() { return (m_handle->status == running); }

    gsl_shell_app*  app()    { return m_handle->app; }
    fx_plot_window* window() { return m_handle->window; }

private:
    lua_fox_window* m_handle;
};

static int
error_return(lua_State* L, const char* error_msg)
{
    lua_pushstring(L, error_msg);
    return (-1);
}

static int
type_error_return(lua_State* L, int narg, const char* req_type)
{
    const char *actual_type = full_type_name(L, narg);
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", narg, req_type, actual_type);
    return (-1);
}

int
fox_window_new (lua_State *L)
{
    gsl_shell_app* app = global_app;

    const char* split = lua_tostring(L, 1);

    app->lock();

    lua_fox_window* bwin = new(L, GS_WINDOW) lua_fox_window();
    fx_plot_window* win = new fx_plot_window(app, split, "GSL Shell FX plot", app->plot_icon, NULL, 480, 480);

    bwin->window = win;
    bwin->app    = app;
    bwin->status = not_ready;

    win->set_lua_window(bwin);

    win->setTarget(app);

    app->window_create_request(win);
    win->lua_id = window_index_add (L, -1);
    app->wait_action();

    bwin->status = running;

    app->unlock();
    return 1;
}

int
fox_window_layout(lua_State* L)
{
    return luaL_error(L, "window's layout method not yet implemented "
        "in FOX client");
}

static int
fox_window_attach_try(lua_State *L)
{
    window_mutex wm(L, 1);

    if (!wm.is_defined()) return type_error_return(L, 1, "window");
    if (!wm.is_running()) return error_return(L, "window is not running");

    sg_plot* p = object_cast<sg_plot>(L, 2, GS_PLOT);
    if (!p) return type_error_return(L, 2, "plot");

    const char* slot_str = lua_tostring(L, 3);

    if (!slot_str) return type_error_return(L, 3, "string");

    fx_plot_window* win = wm.window();
    fx_plot_canvas* canvas = win->canvas();
    int index = canvas->attach(p, slot_str);

    if (index < 0) return error_return(L, "invalid slot specification");

    canvas->plot_draw(index);

    window_refs_add (L, index + 1, 1, 2);
    return 0;
}

int
fox_window_attach(lua_State* L)
{
    int nret = fox_window_attach_try(L);
    if (nret < 0) lua_error(L);
    return nret;
}

static int
fox_window_close_try (lua_State *L)
{
    window_mutex wm(L, 1);

    if (!wm.is_defined()) return type_error_return(L, 1, "window");
    if (!wm.is_running()) return 0;

    fx_plot_window* win = wm.window();
    gsl_shell_app* app = wm.app();

    int window_id = win->lua_id;

    app->window_close_request(win);
    app->wait_action();

    window_index_remove (L, window_id);
    return 0;
}

int
fox_window_close(lua_State* L)
{
    int nret = fox_window_close_try(L);
    if (nret < 0) lua_error(L);
    return nret;
}

static int
fox_window_slot_generic_try(lua_State *L, void (*slot_func)(fx_plot_canvas*, unsigned))
{
    window_mutex wm(L, 1);

    if (!wm.is_defined()) return type_error_return(L, 1, "window");
    if (!wm.is_running()) return 0;

    if (!lua_isnumber(L, 2)) return type_error_return(L, 2, "integer");
    int slot_id = lua_tointeger(L, 2);

    if (slot_id <= 0) return error_return(L, "invalid slot index");

    fx_plot_window* win = wm.window();
    fx_plot_canvas* canvas = win->canvas();

    if (canvas->is_ready())
    {
        slot_func(canvas, slot_id - 1);
    }

    return 0;
}

static void
slot_refresh(fx_plot_canvas* canvas, unsigned index)
{
    bool redraw = canvas->plot_need_redraw(index);
    if (redraw)
    {
        canvas->plot_render(index);
    }

    opt_rect<int> r = canvas->plot_render_queue(index);
    agg::rect_i area = canvas->get_plot_area(index);
    if (redraw)
    {
        canvas->update_region(area);
        fprintf(stderr, "slot_refresh: updating PLOT AREA: %i %i %i %i\n", area.x1, area.y1, area.x2, area.y2);
    }
    else
    {
        if (r.is_defined())
        {
            const int pad = 4;
            const agg::rect_i& ri = r.rect();
            agg::rect_i r_pad(ri.x1 - pad, ri.y1 - pad, ri.x2 + pad, ri.y2 + pad);
            r_pad.clip(area);
            canvas->update_region(r_pad);
            fprintf(stderr, "slot_refresh: updating RECT: %i %i %i %i\n", r_pad.x1, r_pad.y1, r_pad.x2, r_pad.y2);
        }
        else
        {
            fprintf(stderr, "slot_refresh: EMPTY BOX\n");
        }
    }
}

int
fox_window_slot_refresh(lua_State* L)
{
    int nret = fox_window_slot_generic_try(L, slot_refresh);
    if (nret < 0) lua_error(L);
    return nret;
}

static void
slot_update(fx_plot_canvas* canvas, unsigned index)
{
    canvas->plot_render(index);
    canvas->plot_render_queue(index);
    canvas->update_plot_region(index);
}

int
fox_window_slot_update(lua_State* L)
{
    int nret = fox_window_slot_generic_try(L, slot_update);
    if (nret < 0) lua_error(L);
    return nret;
}

static void
save_slot_image(fx_plot_canvas* canvas, unsigned index)
{
    canvas->save_plot_image(index);
}

int
fox_window_save_slot_image (lua_State *L)
{
    int nret = fox_window_slot_generic_try(L, save_slot_image);
    if (nret < 0) lua_error(L);
    return nret;
}

static void
restore_slot_image(fx_plot_canvas* canvas, unsigned index)
{
    if (canvas->plot_have_saved_image(index))
    {
        canvas->restore_plot_image(index);
    }
    else
    {
        canvas->plot_render(index);
        canvas->save_plot_image(index);
    }
}

int
fox_window_restore_slot_image (lua_State *L)
{
    int nret = fox_window_slot_generic_try(L, restore_slot_image);
    if (nret < 0) lua_error(L);
    return nret;
}

static int
fox_window_export_svg_try(lua_State *L)
{
    window_mutex wm(L, 1);
    const char *filename = lua_tostring(L, 2);
    const double w = luaL_optnumber(L, 3, 600.0);
    const double h = luaL_optnumber(L, 4, 600.0);

    if (!wm.is_defined()) return type_error_return(L, 1, "window");

    if (!wm.is_running()) return error_return(L, "window is not running");

    if (!filename) return type_error_return(L, 2, "string");

    unsigned fnlen = strlen(filename);
    if (fnlen <= 4 || strcmp(filename + (fnlen - 4), ".svg") != 0)
    {
        const char* basename = (fnlen > 0 ? filename : "unnamed");
        lua_pushfstring(L, "%s.svg", basename);
        filename = lua_tostring(L, -1);
    }

    FILE* f = fopen(filename, "w");
    if (!f)
    {
        lua_pushfstring(L, "cannot open filename: %s", filename);
        return (-1);
    }

    fx_plot_window* win = wm.window();
    fx_plot_canvas* fxcanvas = win->canvas();

    canvas_svg canvas(f, h);
    canvas.write_header(w, h);

    unsigned n = fxcanvas->get_plot_number();
    for (unsigned k = 0; k < n; k++)
    {
        char plot_name[64];
        sg_plot* p = fxcanvas->get_plot(k);
        if (p)
        {
            agg::rect_i area = fxcanvas->get_plot_area(k, int(w), int(h));
            sprintf(plot_name, "plot%u", k + 1);
            canvas.write_group_header(plot_name);
            p->draw(canvas, area, NULL);
            canvas.write_group_end(plot_name);
        }
    }

    canvas.write_end();
    fclose(f);

    return 0;
}

int
fox_window_export_svg(lua_State *L)
{
    int nret = fox_window_export_svg_try(L);
    if (nret < 0) return lua_error(L);
    return nret;
}

void lua_window_set_closed(void* _win)
{
    lua_fox_window *win = (lua_fox_window*) _win;
    win->status = closed;
}

void
fox_window_register (lua_State *L)
{
    luaL_newmetatable (L, GS_METATABLE(GS_WINDOW));
    lua_pushvalue (L, -1);
    lua_setfield (L, -2, "__index");
    luaL_register (L, NULL, fox_window_methods);
    lua_pop (L, 1);

    luaL_register (L, NULL, fox_window_functions);
}
