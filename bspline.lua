
local ffi  = require 'ffi'
local cgsl = require 'cgsl'

local gsl_check = require 'gsl-check'

local function eval(bs, x)
   local dof, ws = bs.dof, bs.ws
   local c = matrix.alloc(dof, 1)
   local v = cgsl.gsl_matrix_column (c, 0)
   gsl_check(cgsl.gsl_bspline_eval(x, v, ws))
   return c
end

local function model(bs, x)
   local dof, ws = bs.dof, bs.ws
   local n = matrix.dim(x)
   local m = matrix.alloc(n, dof)
   for j = 0, n-1 do
      local xj = cgsl.gsl_matrix_get(x, j, 0)
      local v = cgsl.gsl_matrix_row (m, j)
      gsl_check(cgsl.gsl_bspline_eval(xj, v, ws))
   end
   return m
end

local mt = {
   __index = {eval= eval, model= model}
}

local function bspline(a, b, nbreak)
   local breaks
   local brk
   if type(a) ~= 'number' then
      breaks = type(a) == 'table' and matrix.vec(a) or a
      brk = cgsl.gsl_matrix_column (breaks, 0)
      nbreak = #breaks
   end

   local k = 4
   local dof = nbreak + k - 2
   local ws = ffi.gc(cgsl.gsl_bspline_alloc (k, nbreak), cgsl.gsl_bspline_free)

   if breaks then
      gsl_check(cgsl.gsl_bspline_knots (brk, ws))
   else
      gsl_check(cgsl.gsl_bspline_knots_uniform (a, b, ws))
   end

   local bs = {dof= dof, ws= ws}
   setmetatable(bs, mt)

   return bs
end

return bspline
