
/* matrix_arith.c
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
#include <assert.h>
#include <string.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>

#include "gs-types.h"
#include "matrix.h"
#include "cmatrix.h"
#include "matrix_arith.h"
#include "lua-utils.h"

struct pmatrix {
  int tp;
  union {
    gsl_matrix *real;
    gsl_matrix_complex *cmpl;
  } m;
};

static int matrix_inv   (lua_State *L);
static int matrix_solve (lua_State *L);
static int matrix_mul   (lua_State *L);

static const struct luaL_Reg matrix_arith_functions[] = {
  //  {"dim",           matrix_dim)},
  //  {"copy",          matrix_copy},
  {"mul",           matrix_mul},
  {"solve",         matrix_solve},
  {"inv",           matrix_inv},
  {NULL, NULL}
};

static const char * size_err_msg = "matrices should have the same size in %s";

static gsl_matrix_complex *
push_matrix_complex_of_real (lua_State *L, const gsl_matrix *a)
{
  size_t n1 = a->size1, n2 = a->size2;
  gsl_matrix_complex *r = matrix_complex_push_raw (L, n1, n2);
  size_t i;
  for (i = 0; i < n1; i++)
    {
      double *rp0 = r->data + 2 * (r->tda * i);
      double *rp1 = r->data + 2 * (r->tda * i + n2);
      double *ap = a->data + a->tda * i;
      for (double *rp = rp0; rp < rp1; rp += 2, ap += 1)
	{
	  rp[0] = *ap;
	  rp[1] = 0.0;
	}
    }

  return r;
}

static void
check_matrix_type (lua_State *L, int index, struct pmatrix *r)
{
  if (gs_is_userdata (L, index, GS_MATRIX))
    {
      r->tp = GS_MATRIX;
      r->m.real = lua_touserdata (L, index);
    }
  else if (gs_is_userdata (L, index, GS_CMATRIX))
    {
      r->tp = GS_CMATRIX;
      r->m.cmpl = lua_touserdata (L, index);
    }
  else
    {
      gs_type_error (L, index, "matrix");
    }
}

static void
matrix_complex_promote (lua_State *L, int index, struct pmatrix *a)
{
  a->tp = GS_CMATRIX;
  a->m.cmpl = push_matrix_complex_of_real (L, a->m.real);
  lua_replace (L, index);
}

#define OPER_ADD
#include "template_matrix_oper_on.h"
#include "matrix_op_source.c"
#include "template_matrix_oper_off.h"
#undef OPER_ADD

#define OPER_SUB
#include "template_matrix_oper_on.h"
#include "matrix_op_source.c"
#include "template_matrix_oper_off.h"
#undef OPER_SUB

#define OPER_MUL
#include "template_matrix_oper_on.h"
#include "matrix_op_source.c"
#include "template_matrix_oper_off.h"
#undef OPER_MUL

#define OPER_DIV
#include "template_matrix_oper_on.h"
#include "matrix_op_source.c"
#include "template_matrix_oper_off.h"
#undef OPER_DIV

int
matrix_unm (lua_State *L)
{
  int tp = gs_is_userdata (L, 1, GS_MATRIX) ? GS_MATRIX : GS_CMATRIX;

  if (tp == GS_MATRIX)
    {
      const gsl_matrix *a = lua_touserdata (L, 1);
      size_t n1 = a->size1, n2 = a->size2;
      gsl_matrix *r = matrix_push_raw (L, n1, n2);
      size_t i;

      for (i = 0; i < n1; i++)
	{
	  double *rp0 = r->data + (r->tda * i);
	  double *rp1 = r->data + (r->tda * i + n2);
	  double *ap = a->data + a->tda * i;
	  for (double *rp = rp0; rp < rp1; rp++, ap++)
	    *rp = - (*ap);
	}
    }
  else
    {
      const gsl_matrix_complex *a = gs_check_userdata (L, 1, GS_CMATRIX);
      size_t n1 = a->size1, n2 = a->size2;
      gsl_matrix_complex *r = matrix_complex_push_raw (L, n1, n2);
      size_t i;

      for (i = 0; i < n1; i++)
	{
	  double *rp0 = r->data + 2* (r->tda * i);
	  double *rp1 = r->data + 2* (r->tda * i + n2);
	  double *ap  = a->data + 2* (a->tda * i);
	  for (double *rp = rp0; rp < rp1; rp += 2, ap += 2)
	    {
	      rp[0] = - ap[0];
	      rp[1] = - ap[1];
	    }
	}
    }

  return 1;
}

int
matrix_mul (lua_State *L)
{
  int nargs = lua_gettop (L);
  struct pmatrix a, b, r;
  int k;

  for (k = nargs - 1; k >= 1; k--)
    {
      check_matrix_type (L, k, &a);
      check_matrix_type (L, k+1, &b);

      r.tp = (a.tp == GS_MATRIX && b.tp == GS_MATRIX ? GS_MATRIX : GS_CMATRIX);
	
      if (a.tp != r.tp)
	matrix_complex_promote (L, k, &a);

      if (b.tp != r.tp)
	matrix_complex_promote (L, k+1, &b);

      if (r.tp == GS_MATRIX)
	r.m.real = matrix_push (L, a.m.real->size1, b.m.real->size2);
      else
	r.m.cmpl = matrix_complex_push (L, a.m.cmpl->size1, b.m.cmpl->size2);

      if (r.tp == GS_MATRIX)
	{
	  gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 
			  1.0, a.m.real, b.m.real, 1.0, r.m.real);
	}
      else
	{
	  gsl_complex u = {{1.0, 0.0}};
	  gsl_blas_zgemm (CblasNoTrans, CblasNoTrans, 
			  u, a.m.cmpl, b.m.cmpl, u, r.m.cmpl);
	}

      lua_insert (L, k);
      lua_pop (L, 2);
    }

  return 1;
}

int
matrix_inv (lua_State *L)
{
  struct pmatrix a;
  check_matrix_type (L, 1, &a);
  switch (a.tp)
    {
    case GS_MATRIX:
      return matrix_inverse_raw (L, a.m.real);
    case GS_CMATRIX:
      return matrix_complex_inverse_raw (L, a.m.cmpl);
    default:
      /* */;
    }
  return 0;
}

int
matrix_solve (lua_State *L)
{
  struct pmatrix a, b, r;
  check_matrix_type (L, 1, &a);
  check_matrix_type (L, 2, &b);
  
  r.tp = (a.tp == GS_MATRIX && b.tp == GS_MATRIX ? GS_MATRIX : GS_CMATRIX);

  if (a.tp != r.tp)
    matrix_complex_promote (L, 1, &a);

  if (b.tp != r.tp)
    matrix_complex_promote (L, 2, &b);

  switch (r.tp)
    {
    case GS_MATRIX:
      return matrix_solve_raw (L, a.m.real, b.m.real);
    case GS_CMATRIX:
      return matrix_complex_solve_raw (L, a.m.cmpl, b.m.cmpl);
    default:
      /* */;
    }

  return 0;
}

void
matrix_arith_register (lua_State *L)
{
  luaL_register (L, NULL, matrix_arith_functions);
}
