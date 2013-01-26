
local mini = require 'expr-parser'
local expr_print = require 'expr-print'

local sqrt, abs = math.sqrt, math.abs
local type = type

local FACTOR_CLASS = 0
local SCALAR_CLASS = 1

local function find_column_type(t, j)
    local n = #t
    for i = 1, n do
        local x = t:get(i, j)
        if type(x) == 'string' then return FACTOR_CLASS end
    end
    return SCALAR_CLASS
end

local function mult(a, b)
    if a == 1 then return b end
    if b == 1 then return a end
    return {operator= '*', a, b}
end

local function scalar_infix(sym, a, b)
    if sym == '*' then
        return mult(a, b)
    else
        return {operator= sym, a, b}
    end
end

local function factor_infix(sym, a, b)
    if not (a or b) then return nil end
    if sym ~= '*' then
        error('non multiplicative opeation on factors')
    end
    local c = {}
    if a then for i, f in ipairs(a) do c[#c+1] = f end end
    if b then for i, f in ipairs(b) do c[#c+1] = f end end
    return c
end

local function infix_action(sym, a, b)
    local c = {}
    c.scalar = scalar_infix(sym, a.scalar, b.scalar)
    c.factor = factor_infix(sym, a.factor, b.factor)
    return c
end

local function prefix_action(sym, a)
    if a.factor then error('non multiplicative opeation on factors') end
    return {scalar= {operator= sym, a}}
end

local function enum_action(id)
	return {scalar= 1, factor= {id}}
end

local function func_eval_action(func_name, arg_expr)
    if arg_expr.factor then
        error('applying function ' .. func_name .. ' to an enumerated factor')
    end
    return {scalar= {func = func_name, arg = arg_expr.scalar}}
end

local function lm_actions_gen(t)
    local n, m = t:dim()

    local column_class = {}
    for j = 1, m do column_class[j] = find_column_type(t, j) end

    local function ident_action(id)
        local index = t:col_index(id)
        if column_class[index] == FACTOR_CLASS then
            return {scalar= 1, factor= {id}}
        else
            return {scalar= {name= id, index= t:col_index(id)}}
        end
    end

    return {
        infix     = infix_action,
        ident     = ident_action,
        prefix    = prefix_action,
        enum      = enum_action,
        func_eval = func_eval_action,
        number    = function(x) return {scalar= x} end,
        exprlist  = function(a, ls) if ls then ls[#ls+1] = a else ls = {a} end; return ls end,
        schema    = function(x, y) return {x= x, y= y} end,
    }
end

local function add_unique(t, val)
    for k, x in ipairs(t) do
        if x == val then return k end
    end
    local n = #t + 1
    t[n] = val
    return n
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

    -- Start the counter from 1 instead of 0 to omit the first
    -- level. It will be implicitely the reference.
    ks[n] = (factors.omit_ref_level and 1 or 0)
    while true do
        local lev = {}
        for i, name in ipairs(factors) do
            lev[i] = levels[name][ks[i] + 1]
        end
        ls[#ls + 1] = lev

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

local function expr_are_equal(a, b)
    if a == b then
        return true
    elseif type(a) == 'table' and type(b) == 'table' then
        if a.operator == b.operator then
            return expr_are_equal(a[1], b[1]) and expr_are_equal(a[2], b[2])
        end
    end
    return false
end

local function scalar_term_exists(expr_list, s)
    for i, expr in ipairs(expr_list) do
        if not expr.factor and expr_are_equal(expr.scalar, s) then
            return true
        end
    end
    return false
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

    local X, Y = matrix.alloc(N, XM), matrix.alloc(N, 1)
    local row_index = 1
    for i = 1, N do
        eval_set(i)
        local row_undef = false
        local col_index = 1
        for k = 1, NE do
            local expr = expr_list[k]
            local scalar_expr = expr.scalar
            local xs = eval_scalar(scalar_expr, eval_scope)
            row_undef = row_undef or (not xs)
            if xs then
                if not expr.factor then
                    X:set(row_index, col_index, xs)
                else
                    row_undef = row_undef or (not factors_defined(t, i, expr.factor))
                    local j0 = col_index
                    for j, req_lev in ipairs(expr.levels) do
                        local match = level_does_match(t, i, expr.factor, req_lev)
                        X:set(row_index, j0 + (j - 1), match * xs)
                    end
                end
            end
            col_index = col_index + expr.mult
        end
        if not row_undef then
            local y_val = eval_scalar(y_expr, eval_scope)
            if y_val then
                Y:set(row_index, 1, y_val)
                row_index = row_index + 1
            end
        end
    end

    local nb_rows = row_index - 1
    assert(nb_rows > 0, "undefined model")

    -- resize X to take into account the rows really defined
    X.size1 = nb_rows
    Y.size1 = nb_rows

    return X, Y
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

    -- flag the factors whose scalar part is already used
    -- in the model. In these cases the first level of the
    -- factor will be omitted from the model matrix.
    for k, expr in ipairs(expr_list) do
        if expr.factor then
            local s = expr.scalar
            if scalar_term_exists(expr_list, s) then
                expr.factor.omit_ref_level = true
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
            local inn = expr.factor.omit_ref_level and lnb - 1 or lnb
            expr.mult = inn
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
    return {coeff = coeff, c = c, chisq = chisq, cov = cov, X = X}
end

local function lm(t, model_formula)
    local actions = lm_actions_gen(t)
    local l = mini.lexer(model_formula)
    local schema = mini.schema(l, actions)

    local function matrix_eval(t_alt)
        return eval_lm_matrix(t_alt, schema.x, schema.y.scalar)
    end

    local names = build_lm_model(t, schema.x, schema.y.scalar)
    local X, y = matrix_eval(t)
    local fit = compute_fit(X, y, names)
    fit.model = matrix_eval
    return fit
end

gdt.lm = lm
