
local mini = require 'expr-parser'
local expr_print = require 'expr-print'

local sqrt, abs = math.sqrt, math.abs
local type, pairs, ipairs = type, pairs, ipairs

local function add_unique_gen(t, val, equal)
    for k, x in ipairs(t) do
        if equal(x, val) then return k end
    end
    local n = #t + 1
    t[n] = val
    return n
end

local std_equal = function(a, b) return a == b end

local function list_equal(a, b)
    local n = #a
    for i = 1, n do
        if a[i] ~= b[i] then return false end
    end
    return true
end

local function add_unique(t, val)
    return add_unique_gen(t, val, std_equal)
end

local function level_number(factors, levels)
    if not factors then return 0 end
    local nb = 1
    for _, factor_name in ipairs(factors) do
        nb = nb * #levels[factor_name]
    end
    return nb
end

local function enum_levels(factors, levels)
    local ls, ks, ms = {}, {}, {}
    local n = #factors
    for i, name in ipairs(factors) do
        ks[i], ms[i] = 0, #levels[name]
    end

    local first = true
    while true do
        local lev = {}
        for i, name in ipairs(factors) do
            lev[i] = levels[name][ks[i] + 1]
        end
        if not first then ls[#ls + 1] = lev end
        first = false

        for i = n, 0, -1 do
            if i == 0 then return ls end
            ks[i] = (ks[i] + 1) % ms[i]
            if ks[i] > 0 then break end
        end
    end
end

local function factors_defined(t, i, factors)
    for k, factor_name in ipairs(factors) do
        local y = t:get(i, t:col_index(factor_name))
        if not y then return false end
    end
    return true
end

local function level_does_match(t, i, factors, req_levels)
    for k, factor_name in ipairs(factors) do
        local y = t:get(i, t:col_index(factor_name))
        if y ~= req_levels[k] then return 0 end
    end
    return 1
end

local function print_expr_level(factors, levels)
    local t = {}
    for i, f in ipairs(factors) do
        t[i] = string.format("%s%s", f, levels[i])
    end
    return table.concat(t, ':')
end

local function expr_is_unit(e)
    return type(e) == 'number' and e == 1
end

local function eval_scalar_gen(t)
    local i
    local id_res = function(expr) return t:get(i, expr.index) end
    local func_res = function(expr) return math[expr.func] end
    local set = function(ix) i = ix end
    return set, {ident= id_res, func= func_res}
end

local function eval_lm_matrix(t, expr_list, y_expr)
    local eval_set, eval_scope = eval_scalar_gen(t)
    local eval_scalar = expr_print.eval

    local N = #t
    local NE = #expr_list
    local XM = 0
    for k, e in ipairs(expr_list) do XM = XM + e.mult end

    local X = matrix.alloc(N, XM)
    local Y = y_expr and matrix.alloc(N, 1)
    local index_map, index_map_start, index_map_len = {}, 1, 0
    local row_index = 1
    for i = 1, N do
        eval_set(i)
        local row_undef = false
        local col_index = 1
        for k = 1, NE do
            local expr = expr_list[k]
            local xs = eval_scalar(expr.scalar, eval_scope)
            local is_undef = (not xs) or (expr.factor and not factors_defined(t, i, expr.factor))
            row_undef = row_undef or is_undef
            if not is_undef then
                if not expr.factor then
                    X:set(row_index, col_index, xs)
                else
                    local j0 = col_index
                    for j, req_lev in ipairs(expr.levels) do
                        local match = level_does_match(t, i, expr.factor, req_lev)
                        X:set(row_index, j0 + (j - 1), match * xs)
                    end
                end
            end
            col_index = col_index + expr.mult
        end

        if y_expr and not row_undef then
            local y_val = eval_scalar(y_expr, eval_scope)
            row_undef = (not y_val)
            if y_val then Y:set(row_index, 1, y_val) end
        end

        if row_undef then
            local kk = #index_map
            index_map[kk+1] = index_map_start
            index_map[kk+2] = index_map_len
            index_map_start = i + 1
            index_map_len = 0
        else
            row_index = row_index + 1
            index_map_len = index_map_len + 1
        end
    end

    if index_map_len > 0 then
        local kk = #index_map
        index_map[kk+1] = index_map_start
        index_map[kk+2] = index_map_len
    end

    local nb_rows = row_index - 1
    assert(nb_rows > 0, "undefined model")

    -- resize X to take into account the rows really defined
    X.size1 = nb_rows
    if y_expr then
        Y.size1 = nb_rows
        return X, Y, index_map
    end

    return X
end

local check = require 'check'
local mon = require 'monomial'

local function monomial_to_expr(m, context)
    local coeff = m[1]
    local prod
    for k, sym, pow in mon.terms(m) do
        local base = context[sym]
        local t = (pow == 1 and base or {operator='^', base, pow})
        prod = (prod and {operator='*', t, prod} or t)
    end
    return coeff == 1 and (prod or 1) or (prod and {operator='*', coeff, prod} or coeff)
end

local function expr_to_monomial(expr, context)
    if type(expr) == 'number' then
        return {expr}
    elseif expr.operator == '*' then
        local a = expr_to_monomial(expr[1], context)
        local b = expr_to_monomial(expr[2], context)
        mon.mult(a, b)
        return a
    elseif expr.operator == '^' and check.is_integer(expr[2]) then
        local base = expr_to_monomial(expr[1], context)
        mon.power(base, expr[2])
        return base
    else
        local s = expr_print.expr(expr)
        context[s] = expr
        return mon.symbol(s)
    end
end

local function build_lm_model(t, expr_list, y_expr)
    local N, M = t:dim()

    -- list of unique factors referenced in expr_list
    local used_factors = {}
    for k, expr in ipairs(expr_list) do
        if expr.factor then
            for _, f_name in ipairs(expr.factor) do
                add_unique(used_factors, f_name)
            end
        end
    end

    -- for each unique used factor prepare the levels list and
    -- set the column index
    local levels, factor_index = {}, {}
    for k, name in ipairs(used_factors) do
        levels[name] = {}
        factor_index[name] = t:col_index(name)
    end

    -- find the levels for each of the used factors
    local get = t.get
    for i = 1, N do
        for _, name in ipairs(used_factors) do
            local v = get(t, i, factor_index[name])
            add_unique(levels[name], v)
        end
    end

    for k, expr in ipairs(expr_list) do
        if expr.factor then
            local lnb = level_number(expr.factor, levels)
            expr.mult = lnb - 1
        else
            expr.mult = 1
        end
    end

    -- first pass to find coefficient names and levels by expression
    local names = {}
    local col_index = 1
    for k, expr in ipairs(expr_list) do
        local j0 = col_index
        local scalar_repr = expr_print.expr(expr.scalar)
        if not expr.factor then
            names[j0] = scalar_repr
        else
            local flevels = enum_levels(expr.factor, levels)
            for j, req_lev in ipairs(flevels) do
                local level_repr = print_expr_level(expr.factor, req_lev)
                local nm
                if expr_is_unit(expr.scalar) then
                    nm = level_repr
                else
                    nm = scalar_repr .. ' * ' .. level_repr
                end
                names[j0 + (j - 1)] = nm
            end
            expr.levels = flevels
        end
        col_index = col_index + expr.mult
    end

    return names
end

local function t_test(xm, s, n, df)
    local t = xm / s
    local at = abs(t)
    local p_value = 2 * (1 - randist.tdist_P(at, df))
    return t, (p_value >= 2e-16 and p_value or '< 2e-16')
end

local function compute_fit(X, y, names)
    local n = #y
    local c, chisq, cov = num.linfit(X, y)
    local coeff = gdt.alloc(#c, {"term", "estimate", "std error", "t value" ,"Pr(>|t|)"})
    for i = 1, #c do
        coeff:set(i, 1, names[i])
        local xm, s = c[i], cov:get(i,i)
        coeff:set(i, 2, xm)
        coeff:set(i, 3, sqrt(s))
        local t, p_value = t_test(xm, sqrt(s), n, n - #c)
        coeff:set(i, 4, t)
        coeff:set(i, 5, p_value)
    end
    return {coeff = coeff, c = c, chisq = chisq, cov = cov, n = n, p = #c}
end

local function fit_compute_Rsquare(fit, X, y)
    local n, p = fit.n, fit.p
    local y_pred = X * fit.c

    local y_mean = 0
    for k = 1, #y do y_mean = y_mean + y:get(k, 1) end
    y_mean = y_mean / #y

    local SS_tot, SS_reg = 0, 0
    for k = 1, #y do
        SS_reg = SS_reg + (y:get(k, 1) - y_pred:get(k, 1))^2
        SS_tot = SS_tot + (y:get(k, 1) - y_mean)^2
    end

    local R2 = 1 - SS_reg/SS_tot
    local R2_adj = R2 - (1 - R2) * p / (n - p - 1)
    local SE = sqrt(SS_reg / (n - p))

    return SE, R2, R2_adj
end

local function fit_add_predicted(t, param_name, X, fit, index_map)
    local cname = string.format("%s (PREDICTED)", param_name)
    if not t:col_index(cname) then t:append(cname) end
    local cindex = t:col_index(cname)

    local y_pred = X * fit.c
    local jy = 1
    for k = 1, #index_map - 1, 2 do
        local idx, len = index_map[k], index_map[k+1]
        for j = 0, len - 1 do
            t:set(idx + j, cindex, y_pred[jy + j])
        end
        jy = jy + len
    end
end

local function monomial_exists(ls, e)
    local n = #ls
    for k = 1, n do
        if mon.equal(ls[k], e) then return true end
    end
    return false
end

local function expand_exprs(expr_list)
    -- mls is a table indexed with: 0 for scalar and an id >= 1 for
    -- each factor combination.
    -- Each value in mls is a list of the monomials already
    -- included in the expansion.
    -- mls[0] is the list of monomials for the purely scalar terms.
    -- msl[<factor id>] is the list of monomials for the named
    -- factor term.
    local mls, els = {[0]= {}}, {}
    local fls = {}
    for k, e in ipairs(expr_list) do
        local context = {}
        local m = expr_to_monomial(e.scalar, context)
        local ls = mon.combine(m)
        for _, mexp in ipairs(ls) do
            local eexp = monomial_to_expr(mexp, context)
            local smls = mls[0]
            if not monomial_exists(smls, mexp) then
                smls[#smls+1] = mexp
                els[#els+1] = {scalar= eexp}
            end
            local fact = e.factor
            local fact_id = add_unique_gen(fls, fact, list_equal)
            if fact then
                local fmls = mls[fact_id]
                if not fmls then
                    fmls = {}
                    mls[fact_id] = fmls
                end
                if not monomial_exists(fmls, mexp) then
                    fmls[#fmls+1] = mexp
                    els[#els+1] = {scalar= eexp, factor= fact}
                end
            end
        end
    end

    return els
end

local FIT = {}

function FIT.model(fit, t_alt)
    return eval_lm_matrix(t_alt, fit.x_exprs)
end

function FIT.predict(fit, t_alt)
    local X = eval_lm_matrix(t_alt, fit.x_exprs)
    return X * fit.c
end

function FIT.summary(fit)
    print(fit.coeff)
    print()
    print(string.format("Standard Error: %g, R2: %g, Adjusted R2: %g", fit.SE, fit.R2, fit.R2_adj))
end

function FIT.show(fit)
    return string.format("<fit %p: model: %s>", fit, fit.model_formula)
end

local FIT_MT = {__index = FIT}

-- used to eval a model for a single entry
function FIT.eval(fit, tn)
    local eval_table = fit.eval_table
    for k, name in ipairs(fit.headers) do
        eval_table:set(1, k, tn[name])
    end
    local coeff = fit.c
    local sX = eval_lm_matrix(eval_table, fit.x_exprs)
    local sy = 0
    for k = 0, #coeff - 1 do
        sy = sy + sX.data[k] * coeff.data[k]
    end
    return sy
end

local function lm(t, model_formula, options)
    local gdt_eval_actions = require('gdt-eval')
    local actions = gdt_eval_actions(t)
    local l = mini.lexer(model_formula)
    local schema = mini.schema(l, actions)

    local expand = not options or (options.expand == nil or options.expand)
    local x_exprs = expand and expand_exprs(schema.x) or schema.x

    local names = build_lm_model(t, x_exprs, schema.y.scalar)
    local X, y, index_map = eval_lm_matrix(t, x_exprs, schema.y.scalar)
    local fit = compute_fit(X, y, names)

    if options and options.predict then
        local y_name = expr_print.expr(schema.y.scalar)
        fit_add_predicted(t, y_name, X, fit, index_map)
    end

    fit.model_formula = model_formula
    fit.schema = schema
    fit.x_exprs = x_exprs
    fit.SE, fit.R2, fit.R2_adj = fit_compute_Rsquare(fit, X, y)
    fit.eval_table = gdt.alloc(1, t:headers())
    fit.headers = t:headers()

    return setmetatable(fit, FIT_MT)
end

gdt.lm = lm
