
 -- igsl.lua
 -- 
 -- Copyright (C) 2009 Francesco Abbate
 -- 
 -- This program is free software; you can redistribute it and/or modify
 -- it under the terms of the GNU General Public License as published by
 -- the Free Software Foundation; either version 3 of the License, or (at
 -- your option) any later version.
 -- 
 -- This program is distributed in the hope that it will be useful, but
 -- WITHOUT ANY WARRANTY; without even the implied warranty of
 -- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 -- General Public License for more details.
 -- 
 -- You should have received a copy of the GNU General Public License
 -- along with this program; if not, write to the Free Software
 -- Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 --

function matrix_rowiter(m)
   local i = 0
   local n = m:dims()
   return function()
	     if i < n then
		i = i + 1
		local j = 0
		return function()
			  j = j + 1
			  local v = m:get(i-1, j-1)
			  if v then return v end
		       end
	     end
	  end
end

function matrix_f_set(m, f)
   local r, c = m:dims()
   for i = 0, r-1 do
      for j = 0, c-1 do
	 local z = f(i, j)
	 m:set(i, j, z)
      end
   end
   return m
end

function matrix_accu(m, accu, f)
   local r, c = m:dims()
   for i = 0, r-1 do
      for j = 0, c-1 do
	 accu = f(accu, m:get(i, j))
      end
   end
   return accu
end

local function tostring_eps(z, eps)
   local a, b = real(z), imag(z)
   local f = function(x) return string.format('%g', x) end
   local s = abs(a) > eps and f(a) or ''
   if b > eps then
      local sign = (s == '' and '' or '+')
      s = s .. string.format('%s%si', sign, f(b))
   elseif b < -eps then
      s = s .. string.format('-%si', f(-b))
   end
   return s == '' and '0' or s
end

local function matrix_from_table(ctor, t)
   local f = function(i, j) return t[i+1][j+1] end
   local m = ctor (#t, #t[1])
   matrix_f_set(m, f)
   return m
end

local function vector_from_table(ctor, t)
   local v = ctor (#t, 1)
   for i, x in ipairs(t) do v:set(i-1,0, x) end
   return v
end

function vector(t)
   return vector_from_table(new, t)
end

function cvector(t)
   return vector_from_table(cnew, t)
end

function matrix(t)
   return matrix_from_table(new, t)
end

function cmatrix(t)
   return matrix_from_table(cnew, t)
end

function matrix_print(m)
   local eps = m:norm() * 1e-8
   local fwidth = function(w, val)
		     local clen = # tostring_eps(val, eps)
		     return (clen > w and clen or w)
		  end
   local width = matrix_accu(m, 0, fwidth)
   local lines = {}
   for row in m:rowiter() do
      local line = {}
      for v in row do
	 local s = tostring_eps(v, eps)
	 line[#line+1] = string.rep(' ', width - #s) .. s 
      end
      lines[#lines+1] = '[ ' .. table.concat(line, ' ') .. ' ]'
   end
   return table.concat(lines, '\n')
end

function t(m)
   local r, c = m:dims()
   local mn = new(c, r)
   local f = function(i,j) return m:get(j,i) end
   return matrix_f_set(mn, f)
end

function h(m)
   local r, c = m:dims()
   local mn = cnew(c, r)
   local f = function(i,j) return conj(m:get(j,i)) end
   return matrix_f_set(mn, f)
end

function diag(v)
   local n = v:dims()
   local f = function(i,j) return i == j and v:get(i,0) or 0 end
   local mn = new(n, n)
   return matrix_f_set(mn, f)
end

function unit(n)
   local f = function(i,j) return i == j and 1 or 0 end
   local mn = new(n, n)
   return matrix_f_set(mn, f)
end

function matrix_norm(m)
   local sq = matrix_accu(m, 0, function(p, z) return p + z*conj(z) end)
   return sqrt(sq)
end

function matrix_columns (m, cstart, cnb)
   local r = m:dims()
   return m:slice(0, cstart, r, cnb)
end

function matrix_unpack(m)
   local ls = matrix_accu(m, {}, function(p, z) p[#p+1] = z; return p end)
   return unpack(ls)
end

function matrix_row_print(m)
   local eps = m:norm() * 1e-8
   local f = function(p, z) p[#p+1] = tostring_eps(z, eps); return p end
   return table.concat(matrix_accu(m, {}, f), ', ')
end

function set(d, s)
   matrix_f_set(d, function(i,j) return s:get(i,j) end)
end

function null(m)
   matrix_f_set(m, function(i,j) return 0 end)
end

local function add_matrix_method(s, m)
   Matrix[s] = m
   cMatrix[s] = m
end

function ode_iter(s, t0, y0, t1, tstep)
   s:set(t0, y0)
   return function()
	     local t, y = s.t, s.y
	     if t < t1 then
		s:evolve(t1, tstep)
		return t, y
	     end
	  end
end

ODE.iter  = ode_iter
cODE.iter = ode_iter

add_matrix_method('rowiter',    matrix_rowiter)
add_matrix_method('__tostring', matrix_print)
add_matrix_method('norm',       matrix_norm)
add_matrix_method('columns',    matrix_columns)
add_matrix_method('unpack',     matrix_unpack)
add_matrix_method('row_print',  matrix_row_print)
