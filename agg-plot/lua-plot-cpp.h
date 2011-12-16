#ifndef AGGPLOT_LUA_PLOT_CPP_H
#define AGGPLOT_LUA_PLOT_CPP_H

#include "lua-plot.h"

extern "C" {
#include "lua.h"
}

#include "plot-auto.h"
#include "resource-manager.h"
#include "sg_object.h"

typedef plot<sg_object, manage_owner> sg_plot;
typedef plot_auto<sg_object, manage_owner> sg_plot_auto;

#endif
