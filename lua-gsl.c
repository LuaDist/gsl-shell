
/* lua-gsl.c
 * 
 * Copyright (C) 2009 Francesco Abbate
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <lua.h>
#include <lauxlib.h>

#include "nlinfit.h"
#include "cnlinfit.h"
#include "matrix.h"
#include "cmatrix.h"
#include "linalg.h"
#include "integ.h"
#include "ode_solver.h"
#include "ode.h"
#include "code.h"

static const struct luaL_Reg gsl_methods_dummy[] = {{NULL, NULL}};

int
luaopen_gsl (lua_State *L)
{
  gsl_set_error_handler_off ();

#if 0
  luaL_register (L, "gsl", gsl_methods_dummy);
#endif
  lua_getglobal (L, "_G");
  
  solver_register (L);
  solver_complex_register (L);
  matrix_register (L);
  matrix_complex_register (L);
  linalg_register (L);
  integ_register (L);
  ode_solver_register (L);
  ode_register (L);
  ode_complex_register (L);

  return 1;
}
