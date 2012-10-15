
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

static lua_fox_window*
check_fox_window_lock(lua_State* L, int index)
{
    lua_fox_window *lwin = object_check<lua_fox_window>(L, 1, GS_WINDOW);
    lwin->app->lock();
    if (lwin->status != running)
    {
        lwin->app->unlock();
        return 0;
    }
    return lwin;
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

int
fox_window_attach (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    if (!lwin) return luaL_error(L, "window is not running");

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;
    sg_plot* p = object_check<sg_plot>(L, 2, GS_PLOT);

    const char* slot_str = lua_tostring(L, 3);

    if (!slot_str)
        return luaL_error(L, "missing slot specification");

    fx_plot_canvas* canvas = win->canvas();
    int index = canvas->attach(p, slot_str);

    if (index < 0)
    {
        app->unlock();
        luaL_error(L, "invalid slot specification");
    }
    else
    {
        canvas->plot_draw(index);
        app->unlock();
        window_refs_add (L, index + 1, 1, 2);
    }
    return 0;
}

int
fox_window_close (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    if (!lwin) return 0;

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;

    int window_id = win->lua_id;

    app->window_close_request(win);
    app->wait_action();
    app->unlock();

    window_index_remove (L, window_id);

    return 0;
}

int
fox_window_slot_refresh (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    int slot_id = luaL_checkinteger (L, 2);

    if (!lwin || slot_id <= 0) return 0;

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;
    fx_plot_canvas* canvas = win->canvas();

    if (canvas->is_ready())
    {
        unsigned index = slot_id - 1;
        bool redraw = canvas->need_redraw(index);
        if (redraw)
            canvas->plot_render(index);
        canvas->plot_draw_queue(index, redraw);
    }

    app->unlock();
    return 0;
}

int
fox_window_slot_update (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    int slot_id = luaL_checkinteger (L, 2);

    if (!lwin || slot_id <= 0) return 0;

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;
    fx_plot_canvas* canvas = win->canvas();

    if (canvas->is_ready())
    {
        unsigned index = slot_id - 1;
        canvas->plot_render(index);
        canvas->plot_draw_queue(index, true);
    }

    app->unlock();
    return 0;
}

int
fox_window_save_slot_image (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    int slot_id = luaL_checkinteger (L, 2);

    if (!lwin || slot_id <= 0) return 0;

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;
    fx_plot_canvas* canvas = win->canvas();

    unsigned index = slot_id - 1;
    canvas->save_plot_image(index);

    app->unlock();
    return 0;
}

int
fox_window_restore_slot_image (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    int slot_id = luaL_checkinteger (L, 2);

    if (!lwin || slot_id <= 0) return 0;

    fx_plot_window* win = lwin->window;
    gsl_shell_app* app = lwin->app;
    fx_plot_canvas* canvas = win->canvas();

    unsigned index = slot_id - 1;
    if (!canvas->restore_plot_image(index))
    {
        canvas->plot_render(index);
        canvas->save_plot_image(index);
    }

    app->unlock();
    return 0;
}

int
fox_window_export_svg (lua_State *L)
{
    lua_fox_window *lwin = check_fox_window_lock(L, 1);
    const char *filename = lua_tostring(L, 2);
    const double w = luaL_optnumber(L, 3, 600.0);
    const double h = luaL_optnumber(L, 4, 600.0);

    if (!lwin)
    {
        lwin->app->unlock();
        return luaL_error(L, "invalid window");
    }

    if (!filename)
    {
        lwin->app->unlock();
        return gs_type_error(L, 2, "string");
    }

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
        lwin->app->unlock();
        return luaL_error(L, "cannot open filename: %s", filename);
    }

    fx_plot_window* win = lwin->window;
    fx_plot_canvas* fxcanvas = win->canvas();

    canvas_svg canvas(f, h);
    canvas.write_header(w, h);

    unsigned n = fxcanvas->get_plot_number();
    for (unsigned k = 0; k < n; k++)
    {
        agg::rect_i box;
        char plot_name[64];
        sg_plot* p = fxcanvas->get_plot(k, int(w), int(h), box);
        if (p)
        {
            sprintf(plot_name, "plot%u", k + 1);
            canvas.write_group_header(plot_name);
            p->draw(canvas, box, NULL);
            canvas.write_group_end(plot_name);
        }
    }

    canvas.write_end();
    fclose(f);

    lwin->app->unlock();
    return 0;
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
