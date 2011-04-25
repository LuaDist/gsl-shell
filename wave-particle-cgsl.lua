local ffi = require 'ffi'
local cgsl = require 'cgsl'

local template = require 'template'
local qag = template.load('num/qag.lua.in', {limit=64, order=21})

use 'complex'

local rsin, rcos, rsqrt, rexp = math.sin, math.cos, math.sqrt, math.exp
local atan2, pi = math.atan2, math.pi

local root = dofile('root.lua')

x1 = -16
x2 =  16

v1 = 0
v2 = 40

function ks(e)
   return sqrt(2*(e-v1)), sqrt(2*(e-v2))
end

smat = gsl.cnew(3, 3)
bmat = gsl.cnew(3, 1)

function A2submat(e)
   local k1, k2 = ks(e)
   local a1, a2 = exp(I*k1*x1), exp(I*k2*x2)

   smat:set(1,1, 1/a1)
   smat:set(2,2, 1/a2)
   smat:set(2,3, a2)
   smat:set(3,1,  1)
   smat:set(3,2, -1)
   smat:set(3,3, -1)

   bmat[1] = -a1
   bmat[2] =  0
   bmat[3] = -1
end

function phi_A2x(A2, As, e)
   local A1, B1, B2 = As[1], As[2], As[3]
   local k1, k2 = ks(e)
   return function(x)
	     if x <= 0 then
		return A1 * exp(-I*k1*x) + A2 * exp(I*k1*x)
	     else
		return B1 * exp(-I*k2*x) + B2 * exp(I*k2*x)
	     end
	  end
end

function feigenv(e)
   A2submat(e)
   local As = gsl.solve(smat, bmat)
   return phi_A2x(1, As, e)
end

function feval(e, x)
   A2submat(e)
   local As = gsl.solve(smat, bmat)
   local A1, A2, B1, B2 = As[1], 1, As[2], As[3]
   local k1, k2 = ks(e)
   if x <= 0 then
      return A1 * exp(-I*k1*x) + A2 * exp(I*k1*x)
   else
      return B1 * exp(-I*k2*x) + B2 * exp(I*k2*x)
   end
end

function edet(e)
   local k1, k2 = ks(e)
   return k1 * rsin(k2*x2) * rcos(k1*x1) - k2 * rsin(k1*x1) * rcos(k2*x2)
end

function edet_sub(e)
   local k1, g2 = rsqrt(2*(e-v1)), rsqrt(2*(v2-e))
   return k1 * math.sinh(g2*x2) * rcos(k1*x1) - g2 * rsin(k1*x1) * math.cosh(g2*x2)
end

function root_grid_search(emax)
   local roots = {}
   local de = v2/4000
   local fa = edet_sub(de)
   local fb
   local ea, eb = de, de
   while eb < v2 do
      eb = eb + de
      fb = edet_sub(eb)
      if fa * fb < 0 then
	 local r = root(edet_sub, ea, eb, 1e-8, 1e-8)
	 print('found:', r, 'between', ea, eb)
	 roots[#roots+1] = r
	 ea, fa = eb, fb
      end
   end

   ea = v2 + de
   fa = edet(ea)
   while ea < emax do
      eb = eb + de
      fb = edet(eb)
      if fa * fb < 0 then
	 roots[#roots+1] = root(edet, ea, eb, 1e-8, 1e-8)
	 ea, fa = eb, fb
      end
   end

   return gsl.vector(roots)
end

roots = root_grid_search(120)
print(roots)

local function csqrn(z)
   return real(z) * real(z) + imag(z) * imag(z)
end

function phi_norm(e)
   local k1, k2 = ks(e)
   A2submat(e)
   local As = gsl.solve(smat, bmat)
   local A1, A2, B1, B2 = As[1], 1, As[2], As[3]
   local phi1, phi2, dphi

   local A1s, A2s = csqrn(A1), csqrn(A2)
   phi1, phi2 = atan2(imag(A1), real(A1)), atan2(imag(A2), real(A2))
   dphi = phi2 - phi1
   local n1 = (-x1) * (A1s + A2s) + rsqrt(A1s * A2s)/k1 * (rsin(dphi) - rsin(2*k1*x1 + dphi))

   local B1s, B2s = csqrn(B1), csqrn(B2)
   phi1, phi2 = atan2(imag(B1), real(B1)), atan2(imag(B2), real(B2))
   dphi = phi2 - phi1

   local n2
   if imag(k2) == 0 then
      n2 = x2 * (B1s + B2s) + rsqrt(B1s * B2s)/k2 * (rsin(2*k2*x2 + dphi) - rsin(dphi))
   else
      local g2 = imag(k2)
      n2 = B1s * (rexp(2*g2*x2) - 1)/(2*g2) + B2s * (1-rexp(-2*g2*x2))/(2*g2) + 2 * rsqrt(B1s * B2s) * rcos(dphi) * x2
   end
   
   return n1 + n2
end

function As_mat_compute(roots)
   local n = #roots
   local m = cgsl.gsl_matrix_complex_calloc (n, 4)
   local cxx = ffi.new('gsl_complex')
   for i=1, n do
      local e = roots[i]
      A2submat(e)
      local As = gsl.solve(smat, bmat)
      local nc = rsqrt(1 / phi_norm(e))

      cxx.dat[0] = nc * real(As[1])
      cxx.dat[1] = nc * imag(As[1])
      cgsl.gsl_matrix_complex_set(m, i-1, 0, cxx)

      cxx.dat[0] = nc
      cxx.dat[1] = 0
      cgsl.gsl_matrix_complex_set(m, i-1, 1, cxx)

      cxx.dat[0] = nc * real(As[2])
      cxx.dat[1] = nc * imag(As[2])
      cgsl.gsl_matrix_complex_set(m, i-1, 2, cxx)

      cxx.dat[0] = nc * real(As[3])
      cxx.dat[1] = nc * imag(As[3])
      cgsl.gsl_matrix_complex_set(m, i-1, 3, cxx)
   end

   return m
end

local As_mat = As_mat_compute(roots)

local cxx = ffi.new('gsl_complex')
local function As_coeff(i, j)
   cxx = cgsl.gsl_matrix_complex_get(As_mat, i, j)
   return cxx.dat[0], cxx.dat[1]
end

function feval_ri(i, x)
   local e = cgsl.gsl_matrix_get (roots, i, 0)
   if e > v2 then
      if x < 0 then
	 local k1 = rsqrt(2*(e-v1))
	 local A1r, A1i = As_coeff(i, 0)
	 local A2r, A2i = As_coeff(i, 1)
	 local c, s = rcos(k1*x), rsin(k1*x)
	 return A1r*c + A1i*s + A2r*c - A2i*s, -A1r*s + A1i*c + A2r*s + A2i*c
      else
	 local k2 = rsqrt(2*(e-v2))
	 local B1r, B1i = As_coeff(i, 2)
	 local B2r, B2i = As_coeff(i, 3)
	 local c, s = rcos(k2*x), rsin(k2*x)
	 return B1r*c + B1i*s + B2r*c - B2i*s, -B1r*s + B1i*c + B2r*s + B2i*c
      end
   else
      if x < 0 then
	 local k1 = rsqrt(2*(e-v1))
	 local A1r, A1i = As_coeff(i, 0)
	 local A2r, A2i = As_coeff(i, 1)
	 local c, s = rcos(k1*x), rsin(k1*x)
	 return A1r*c + A1i*s + A2r*c - A2i*s, -A1r*s + A1i*c + A2r*s + A2i*c
      else
	 local g2 = rsqrt(2*(v2-e))
	 local B1r, B1i = As_coeff(i, 2)
	 local B2r, B2i = As_coeff(i, 3)
	 local c = rexp(g2*x)
	 return B1r*c + B2r/c, B1i*c + B2i/c
      end
   end
end

--norm_coeff = {}
--for i= 1, #roots do
--   norm_coeff[i] = rsqrt(1 / phi_norm(roots[i]))
--end

function coherent_state(x0, p0, sig)
   return function(x)
	     return exp(-(x-x0)^2/(4*sig^2) + I*p0*x)
	  end
end

fcs = coherent_state(-4, 8, 0.4)
pcs = graph.fxplot(|x| rsqrt(csqrn(fcs(x))), x1, x2)

function plot_roots()
   local ps = graph.fxplot(edet, v2, roots[#roots])
   ps:addline(graph.fxline(edet_sub, v1, v2), 'magenta')

   local rln = graph.path(roots[1], 0)
   for i = 2, #roots do
      rln:line_to(roots[i], 0)
   end

   ps:addline(rln, 'blue', {{'marker', size=5}})
   io.read '*l'
end

-- plot_roots()

function state_plot()
   local px = graph.plot('Eigenstates Waveforms')
   px:show()
   for i=1, #roots do
      local e = roots[i]
      fz = feigenv(e)
      px:clear()
      local lnr = graph.fxline(|x| real(fz(x)), x1, x2)
      local lni = graph.fxline(|x| imag(fz(x)), x1, x2)

      local nn = phi_norm(e)
      local nv = qag(|x| csqrn(fz(x)), x1, x2, 1e-8, 1e-8)
      print('NORM', nn, nv)
      print('Energy:', e)

      px:addline(lnr, 'red')
      px:addline(lni, 'blue')
      io.read '*l'
   end
end

-- state_plot()

function coeff(f, e)
   local fc = feigenv(e)
   local cr = qag(|x| real(f(x) * conj(fc(x))), x1, x2, 1e-8, 1e-8)
   local ci = qag(|x| imag(f(x) * conj(fc(x))), x1, x2, 1e-8, 1e-8)
   return rsqrt(1 / phi_norm(e)) * (cr + I * ci)
end


coeffs = gsl.cnew(#roots, 1)
-- ex = roots[12]
-- local ncx = phi_norm(ex)
-- fev12 = |x| rsqrt(1 / ncx) * feval(ex, x)
for i = 1, #roots do
   local e = roots[i]
   coeffs[i] = coeff(fcs, e)
end

print(coeffs)

function plot_coeffs()
   --graph.fiplot(|i| csqrn(coeffs[i]), 1, #coeffs)
   local p = graph.plot()
   ln = graph.ipath(gsl.sequence(function(i) return roots[i], csqrn(coeffs[i]) end, 1, #roots))
   p = graph.plot()
   p:addline(ln)
   p:show()
end

plot_coeffs()

-- state_plot()

local csexp = cgsl.gsl_vector_complex_alloc (#roots)
function coeff_inv(cs, t)
   local n = #roots

   for i=0, n-1 do
      local e = cgsl.gsl_matrix_get (roots, i, 0)

      local gslc = cgsl.gsl_matrix_complex_get (cs, i, 0)
      local cr, ci = gslc.dat[0], gslc.dat[1]

      local exr, exi = rcos(-e*t), rsin(-e*t)

      gslc.dat[0] = cr*exr - ci*exi
      gslc.dat[1] = cr*exi + ci*exr

      cgsl.gsl_vector_complex_set (csexp, i, gslc)
   end

   local function eval(x)
      local sr, si = 0, 0
      for i=0, n-1 do
	 local e = cgsl.gsl_matrix_get (roots, i, 0)

	 local gslc = cgsl.gsl_vector_complex_get (csexp, i)
	 local cr, ci = gslc.dat[0], gslc.dat[1]

	 local fr, fi = feval_ri(i, x)

	 sr = sr + (cr*fr - ci*fi)
	 si = si + (cr*fi + ci*fr)
      end
      return sr, si
   end

   return eval
end

local function coeff_inv_csqr(cs, t)
   local eval = coeff_inv(cs, t)
   return function(x)
	     local r, i = eval(x)
	     return r*r + i*i
	  end
end

pcs.sync = false
pcs:pushlayer()
for t= 0, 8, 0.125/2 do
   print('calculating', t)
   frec = coeff_inv_csqr(coeffs, t)
   pcs:clear()
   pcs:addline(graph.fxline(frec, x1, x2), 'green')
   pcs:flush()
end
