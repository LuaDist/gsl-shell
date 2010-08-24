
function read_data(filename)
   local f = io.open(filename, "r")
   local t = {}
   for ln in f:lines() do
      local a, b = string.match(ln, "([0-9%.]+)%s*,%s*([0-9%.]+)")
      t[#t+1] = {tonumber(a), tonumber(b)}
   end
   return matrix(t)
end

function step_func(brs)
   return function(x)
	     for p in brs:rows() do
		if p:get(1,1) > x then return p:get(1,2) end
	     end
	     error 'x out of range in step function'
	  end
end

function step_func_gauss(brs, sg)
   return function(x)
	     local xinf, xsup = x - 5*sg, x + 5*sg
	     local xc = xinf
	     local parz = 0
	     for p in brs:rows() do
		local xi, yi = p:get(1,1), p:get(1,2)
		if xi > xc then
		   local del = cdf.gaussian(xi-x, sg) - cdf.gaussian(xc-x, sg)
		   parz = parz + yi * del
		end
		xc = xi
		if xc > xsup then break end
	     end
	     return parz
	  end
end

dt = read_data('examples/data/sige-sims-prof.csv')
brs, sg0 = matrix {{2, 0}, {4, 36}, {18,32}, {100, 0}}, 0.8

mysf = step_func(brs)
mycf = step_func_gauss(brs, sg0)

p = plot()
p:addline(xyline(dt:col(1), dt:col(2)))
p:addline(fxline(mysf, 0, 27), "blue")
p:addline(fxline(mycf, 0, 27), "green")
p:show()

function apply_param(brs, xp)
   brs:set(1,1, xp[1])
   brs:set(2,1, xp[2])
   brs:set(2,2, xp[3])
   brs:set(3,1, xp[4])
   brs:set(3,2, xp[5])
end

function minf_gener(lbrs)
   lbrs = lbrs:copy()
   return function(xp, g)
	     if g then error 'cannot calc gradient' end

	     if xp[1] > xp[2] or xp[2] > xp[4] then
		return 0/0
	     end

	     if xp[3] < 0 or xp[5] < 0 or xp[3] > 42 then
		return 0/0
	     end

	     apply_param(lbrs, xp)
	     local mycf = step_func_gauss(lbrs, xp[6])

	     local resid = 0
	     for r in dt:rows() do
		local xi, yi = r:get(1,1), r:get(1,2)
		resid = resid + (yi - mycf(xi))^2
	     end
	  
	     return resid
	  end
end

function do_minimize()
   local m = minimizer {f= minf_gener(brs), n= 6}

   local x0 = vector {2,   4,  36, 18, 32, 0.8}
   local dx = vector {0.8, 0.8, 6, 2, 6,   0.1}

   m:set(x0, dx)

   while m:step() == 'continue' do
      print(tr(m.x), m.value)
   end

   print('SUCCESS:', tr(m.x), m.value)

   return m.x
end

xopt = do_minimize()
-- xopt = vector {1.476, 2.66143, 42, 18.4212, 31.5867, 39.7966, 0.604194}

apply_param(brs, xopt)

p = plot'SiGe profile from SIMS data'
p:addline(xyline(dt:col(1), dt:col(2)))
p:addline(fxline(step_func(brs), 0, 27), "blue")
p:addline(fxline(step_func_gauss(brs, xopt[6]), 0, 27), "green")
p:show()
