
local cgdt = require 'cgdt'

local element_is_number = gdt.element_is_number
local format = string.format
local concat = table.concat
local sqrt, abs = math.sqrt, math.abs

-- status: 0 => string, 1 => numbers
local function find_column_type(t, j)
	local n = #t
	for i = 1, n do
		local x = gdt.get_number_unsafe(t, i, j)
		if not x then return 0 end
	end
	return 1
end

local function lm_expr_names(t, expr)
	local n, m = t:dim()

	local code_lines = {}
	local code = function(line) code_lines[#code_lines+1] = line end

    code([[local _LM = require 'lm-helpers']])
    code([[local enum = function(x) return x end]])
    local var_name = t:headers()
	for j = 1, m do
		local name = var_name[j]
		code(format([[local %s = _LM.var_name(%q)]], name, name))
	end
    code(format([[local expr_names = _LM.find_names(%s)]], expr))
    code([[return expr_names]])

    local code_str = concat(code_lines, '\n')
    local f = load(code_str)
    local expr_names = f()
    return expr_names
end

local function lm_prepare(t, expr)
	local n, m = t:dim()
	local column_class = {}
	local code_lines = {}
	local code = function(line) code_lines[#code_lines+1] = line end
    local name = t:headers(j)
    code([[local _LM = require 'lm-helpers']])
    code([[local select = select]])
    code([[local _get, _set = gdt.get, gdt.set]])
    code([[local enum = function(x) return {value = x} end]])
	for j = 1, m do
		column_class[j] = find_column_type(t, j)
		local value = (column_class[j] == 1 and "0" or format("_LM.factor(\"%s\")", name[j]))
		code(format("local %s = %s", name[j], value))
	end

	code(format([[local _y_spec = _LM.eval_test(%s)]], expr))

	code([[local _eval_func = _LM.eval_func]])

	code(format("local _eval = gdt.alloc(%d, _y_spec.np)", n))

	code(format("for _i = 1, %d do", n))
	for j = 1, m do
		local line
		if column_class[j] == 1 then
			line = format("    %s = _get(_t, _i, %d)", name[j], j)
		else
			line = format("    %s.value = _get(_t, _i, %d)", name[j], j)
		end
		code(line)
	end
	code("")
	code(format("    _eval_func(_y_spec, _eval, _i, %s)", expr))
	code(format("end", n))

	return format("return function(_t)\n%s\nreturn _eval, _y_spec\nend", concat(code_lines, "\n"))
end

local function add_unique(t, val)
	for k, x in ipairs(t) do
		if x == val then return 0 end
	end
	local n = #t + 1
	t[n] = val
	return n
end

local function lm_main(Xt, t, inf)
	local N = #t

	local index = {}
	local curr_index = 1
	local coeff_name = {}
	inf.factors, inf.factor_index = {}, {}
	for k = 1, inf.np do
		local factor_name = inf.factor_names[k]
		index[k] = curr_index
		if inf.class[k] == 0 then
			local factors, factor_index = {}, {}
			for i = 1, N do
				local str = Xt:get(i, k)
				local ui = add_unique(factors, str)
				if ui > 0 then
					factor_index[str] = ui
					if ui > 1 then coeff_name[curr_index + (ui - 2)] = factor_name .. "/" .. str end
				end
			end
			inf.factors[k] = factors
			inf.factor_index[k] = factor_index
			curr_index = curr_index + (#factors - 1)
		else
			coeff_name[curr_index] = factor_name
			curr_index = curr_index + 1
		end
	end

	local col = curr_index - 1

	local X = matrix.alloc(N, col)
	local X_data = X.data
	for i = 1, N do
		local idx0 = col * (i - 1)
		for k = 1, inf.np do
			local j = index[k]
			if inf.class[k] == 1 then
				X_data[idx0 + j - 1] = gdt.get_number_unsafe(Xt, i, k)
			else
				local factors = inf.factors[k]
				local factor_index = inf.factor_index[k]
				local req_f = Xt:get(i, k)
				local nf = #factors - 1
				for kf = 1, nf do
					X_data[idx0 + (j - 1) + (kf - 1)] = 0
				end
				local kfx = factor_index[req_f] - 1
				if kfx > 0 then X_data[idx0 + (j - 1) + (kfx - 1)] = 1 end
			end
		end
	end

	return X, coeff_name
end

local function lm_model(t, expr)
	local code = lm_prepare(t, expr)
	local f_code = load(code)()
	local Xt, inf = f_code(t)
	inf.factor_names = lm_expr_names(t, expr)
	return lm_main(Xt, t, inf)
end

local function t_test(xm, s, n, df)
	local t = xm / s
	local at = abs(t)
	local p_value = (1 - randist.tdist_P(at, df)) + (1 - randist.tdist_Q(-at, df))
	return t, (p_value >= 2e-16 and p_value or '< 2e-16')
end

local function lm(t, expr)
	local a, b = string.match(expr, "%s*([%S]+)%s*~(.+)")
	assert(a, "invalid lm expression")
	local n, m = t:dim()
	local jy = t:col_index(a)
	assert(jy, "invalid variable specification in lm expression")
	local set = gdt.set
	local y = matrix.new(n, 1, |i| t:get(i, jy))
	local X, name = lm_model(t, b)
	local c, chisq, cov = num.linfit(X, y)
	local coeff = gdt.alloc(#c, {"term", "estimate", "std error", "t value" ,"Pr(>|t|)"})
	for i = 1, #c do
		coeff:set(i, 1, name[i])
		local xm, s = c[i], cov:get(i,i)
		coeff:set(i, 2, xm)
		coeff:set(i, 3, sqrt(s))
		local t, p_value = t_test(xm, sqrt(s), n, n - #c)
		coeff:set(i, 4, t)
		coeff:set(i, 5, p_value)
	end
	return {coeff = coeff, c = c, chisq = chisq, cov = cov, X = X}
end

gdt.lm = lm
