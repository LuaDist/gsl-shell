local ffi = require 'ffi'

ffi.cdef [[
    int strcmp ( const char * str1, const char * str2 );
]]

local M

local function symbol_class(s)
    if type(s) ~= 'table' then
        return type(s)
    elseif s.operator then
        return 'op' .. s.operator
    else
        return 'poly'
    end
end

local function mon_symbol_compare(sa, sb)
    local ta, tb = symbol_class(sa), symbol_class(sb)

    local tcmp = ffi.C.strcmp(ta, tb)
    if tcmp ~= 0 then
        return tcmp
    else
        if ta == 'string' then
            return ffi.C.strcmp(sa, sb)
        elseif ta == 'poly' then
            return M.poly_compare(sa, sb)
        elseif ta:sub(1, 2) == 'op' then
            local a1, b1 = sa[1], sb[1]
            local c1 = M.poly_compare(a1, b1)
            if c1 ~= 0 then
                return c1
            end
            local a2, b2 = sa[2], sb[2]
            if a2 == nil and b2 == nil then
                return 0
            elseif a2 == nil then
                return -1
            elseif b2 == nil then
                return 1
            end
            return M.poly_compare(a2, b2)
        end
    end
    print(sa, sb)
    error('invalid elements')
end

local function monomial_len(a)
    return (#a - 1) / 2
end

local function mon_terms_iter(a, k)
    k = k + 1
    if 2*k + 1 <= #a then
        return k, a[2*k], a[2*k+1]
    end
end

local function mon_terms(a)
    return mon_terms_iter, a, 0
end

local function mon_symbol(sym)
    return {1, sym, 1}
end

local function monomial_set_term_pow(a, k, new_pow)
    a[2*k+1] = new_pow
end

local function monomial_insert_term(a, k, sym, pow)
    local n = monomial_len(a)
    for k= n, k, -1 do
        a[2*k+2] = a[2*k  ]
        a[2*k+3] = a[2*k+1]
    end
    a[2*k    ] = sym
    a[2*k + 1] = pow
end

local function monomial_mult_factor(a, sym, pow)
    for k, a_sym, a_pow in mon_terms(a) do
        local scmp = mon_symbol_compare(a_sym, sym)
        if scmp == 0 then
            monomial_set_term_pow(a, k, a_pow + pow)
            return
        elseif scmp > 0 then
            monomial_insert_term(a, k, sym, pow)
            return
        end
    end
    local n = monomial_len(a)
    monomial_insert_term(a, n + 1, sym, pow)
end

local function monomial_mult(a, b)
    a[1] = a[1] * b[1]
    for k, sym, pow in mon_terms(b) do
        monomial_mult_factor(a, sym, pow)
    end
end

local function monomial_power(m, n)
    m[1] = m[1]^n
    for k, sym, pow in mon_terms(m) do
        monomial_set_term_pow(m, k, pow * n)
    end
end

local function monomial_equal(a, b)
    if a[1] ~= b[1] or #a ~= #b then return false end
    for k, asym, apow in mon_terms(a) do
        bsym, bpow = b[2*k], b[2*k+1]
        if bsym ~= asym or bpow ~= apow then return false end
    end
    return true
end

local function combine_rec(a, k)
    local n = monomial_len(a)
    if k > n then return {{1}} end
    local sym, pow = a[2*k], a[2*k+1]
    local rls = combine_rec(a, k + 1)
    local ls = {}
    for p = 0, pow do
        for _, mon in ipairs(rls) do
            local xt = p > 0 and {1, sym, p} or {1}
            monomial_mult(xt, mon)
            ls[#ls+1] = xt
        end
    end
    return ls
end

local function monomial_combine(a)
    return combine_rec(a, 1)
end

local function mon_compare(a, b)
    print('mono compare', a, b)
    if a == b then
        return 0
    else
        local n = monomial_len(a)
        for i = 1, n do
            local at, bt = a[2*i], b[2*i]
            local ad, bd = a[2*i + 1], b[2*i + 1]
            if bt == nil then return 1 end
            local cmp = mon_symbol_compare(at, bt)
            if cmp == 0 then
                if ad ~= bd then
                    print('monomial terms', at, bt, 'have different degrees', ad, bd)
                    return ad - bd
                end
            else
                if cmp ~= 0 then
                    print('monomial terms', at, bt, 'differ')
                    return cmp
                end
            end
        end
        return (monomial_len(b) > n and -1 or a[1] - b[1])
    end
end

M = {symbol=mon_symbol, mult= monomial_mult, power=monomial_power, combine= monomial_combine, terms= mon_terms, equal=monomial_equal, compare=mon_compare}

return M
