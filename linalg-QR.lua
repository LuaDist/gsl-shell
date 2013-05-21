local ffi = require 'ffi'
local gsl = require 'gsl'
local gsl_check = require 'gsl-check'

local function QRPT(A)
	local m, n = A:dim()
	local q = matrix.alloc(m, m)
	local r = matrix.alloc(m, n)
	local k = math.min(m, n)
	local p = ffi.gc(gsl.gsl_permutation_alloc(n), gsl.gsl_permutation_free)
	local norm = gsl.gsl_vector_alloc(n)
	local tau = ffi.gc(gsl.gsl_vector_alloc(k), gsl.gsl_vector_free)
	local signum = ffi.new('int[1]')
	print(tau,p,signum,norm)
	gsl.gsl_linalg_QRPT_decomp2(A, q, r, tau, p, signum, norm)
	gsl.gsl_vector_free(norm)
	return q, r, tau, p
end

return QRPT
