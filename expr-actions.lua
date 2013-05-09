
local function mult(a, b)
    if a == 1 then return b end
    if b == 1 then return a end
    return {operator= '*', a, b}
end

local function infix_action(sym, a, b)
    if sym == '*' then
        return mult(a, b)
    else
        return {operator= sym, a, b}
    end
end

local function prefix_action(sym, a)
    return {operator= sym, a}
end

local function enum_action(id)
    return "%" .. id
end

local function func_eval_action(func_name, arg_expr)
    return {func = func_name, arg = arg_expr}
end

local function ident_action(id) return id end

return {
    infix     = infix_action,
    ident     = ident_action,
    prefix    = prefix_action,
    enum      = enum_action,
    func_eval = func_eval_action,
    number    = function(x) return x end,
    exprlist  = function(a, ls) if ls then ls[#ls+1] = a else ls = {a} end; return ls end,
    schema    = function(x, y, conds) return {x= x, y= y, conds= conds} end,
}
