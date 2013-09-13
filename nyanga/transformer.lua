local B = require('nyanga.builder')
local util = require('nyanga.util')

local Scope = { }
Scope.__index = Scope
function Scope.new(outer)
   local self = {
      outer = outer;
      entries = { };
   }
   return setmetatable(self, Scope)
end
function Scope:define(name, info)
   self.entries[name] = info
end
function Scope:lookup(name)
   if self.entries[name] then
      return self.entries[name]
   elseif self.outer then
      return self.outer:lookup(name)
   else
      return nil
   end
end

local Context = { }
Context.__index = Context
function Context.new()
   local self = {
      scope = Scope.new()
   }
   return setmetatable(self, Context)
end
function Context:enter()
   self.scope = Scope.new(self.scope)
end
function Context:leave()
   self.scope = self.scope.outer
end
function Context:define(name, info)
   info = info or { }
   self.scope:define(name, info)
   return info
end
function Context:lookup(name)
   local info = self.scope:lookup(name)
   return info
end

local match = { }

function match:Chunk(node)
   self.hoist = { }
   self.scope = { }
   for i=1, #node.body do
      local stmt = self:get(node.body[i])
      self.scope[#self.scope + 1] = stmt
   end
   for i=#self.hoist, 1, -1 do
      table.insert(self.scope, 1, self.hoist[i])
   end
   return B.chunk(self.scope)
end
function match:Literal(node)
   return B.literal(node.value)
end
function match:Identifier(node)
   return B.identifier(node.name)
end
function match:VariableDeclaration(node)
   local inits = node.inits and self:list(node.inits) or { }
   for i=1, #node.names do
      local n = node.names[i]
      if n.type == 'Identifier' and not self.ctx:lookup(n.name) then
         self.ctx:define(n.name)
      end
   end
   return B.localDeclaration(self:list(node.names), inits)
end
function match:AssignmentExpression(node)
   return B.assignmentExpression(
      self:list(node.left), self:list(node.right)
   )
end
function match:UpdateExpression(node)
   local oper = string.sub(node.operator, 1, 1)
   return B.assignmentExpression({
      self:get(node.left)
   }, {
      B.binaryExpression(oper, self:get(node.left), self:get(node.right))
   })
end
function match:MemberExpression(node)
   return B.memberExpression(
      self:get(node.object), self:get(node.property), node.computed
   )
end
function match:SelfExpression(node)
   return B.identifier('self')
end

function match:ReturnStatement(node)
   if self.retsig then
      return B.doStatement(
         B.blockStatement{
            B.assignmentExpression(
               { self.retsig }, { B.literal(true) }
            );
            B.assignmentExpression(
               { self.retval }, self:list(node.arguments)
            );
            B.returnStatement{ self.retval }
         }
      )
   end
   return B.returnStatement(self:list(node.arguments))
end

function match:IfStatement(node)
   local test, cons, altn = self:get(node.test)
   if node.consequent then
      cons = self:get(node.consequent)
   end
   if node.alternate then
      altn = self:get(node.alternate)
   end
   local stmt = B.ifStatement(test, cons, altn)
   return stmt
end

function match:BreakStatement(node)
   return B.breakStatement()
end

function match:LogicalExpression(node)
   return B.logicalExpression(
      node.operator, self:get(node.left), self:get(node.right)
   )
end

local bitop = {
   [">>"]  = 'rshift',
   [">>>"] = 'arshift',
   ["<<"]  = 'lshift',
   ["|"]   = 'bor',
   ["&"]   = 'band',
   ["^"]   = 'bxor',
}
function match:BinaryExpression(node)
   local o = node.operator
   if bitop[o] then
      local call = B.memberExpression(
         B.identifier('bit'),
         B.identifier(bitop[o])
      )
      local args = { self:get(node.left), self:get(node.right) }
      return B.callExpression(call, args)
   end
   if o == 'is' then
      return B.callExpression(B.identifier('__is__'), {
         self:get(node.left), self:get(node.right)
      })
   end
   if o == '..' then
      return B.callExpression(B.identifier('__range__'), {
         self:get(node.left), self:get(node.right)
      })
   end
   if o == '**' then o = '^'  end
   if o == '~'  then o = '..' end
   if o == '!=' then o = '~=' end

   return B.binaryExpression(o, self:get(node.left), self:get(node.right))
end
function match:UnaryExpression(node)
   local o = node.operator
   local a = self:get(node.argument)
   if o == 'typeof' then
      return B.callExpression(B.identifier('__typeof__'), { a })
   end
   return B.unaryExpression(o, a)
end
function match:FunctionDeclaration(node)
   local name
   if not node.expression then
      name = self:get(node.id[1])
   end

   local params  = { }
   local prelude = { }
   local vararg  = false

   for i=1, #node.params do
      params[#params + 1] = self:get(node.params[i])
      if node.defaults[i] then
         local name = self:get(node.params[i])
         local test = B.binaryExpression("==", name, B.literal(nil))
         local expr = self:get(node.defaults[i])
         local cons = B.blockStatement{
            B.assignmentExpression({ name }, { expr })
         }
         prelude[#prelude + 1] = B.ifStatement(test, cons)
      end
   end

   if node.rest then
      params[#params + 1] = B.vararg()
      prelude[#prelude + 1] = B.localDeclaration(
         { B.identifier(node.rest.name) },
         { B.callExpression(B.identifier('Array'), { B.vararg() }) }
      )
   end

   local body = self:get(node.body)
   for i=#prelude, 1, -1 do
      table.insert(body.body, 1, prelude[i])
   end

   local func
   if node.generator then
      local inner = B.functionExpression({ }, body, vararg)
      func = B.functionExpression(params, B.blockStatement{
         B.returnStatement{
            B.callExpression(
               B.memberExpression(B.identifier("coroutine"), B.identifier("wrap")),
               { inner }
            )
         }
      }, vararg)
   else
      func = B.functionExpression(params, body, vararg)
   end
   if node.expression then
      return func
   end
   return B.localDeclaration({ name }, { func })
end

function match:SpreadExpression(node)
   return B.callExpression(
      B.identifier('__spread__'), { self:get(node.argument) }
   )
end
function match:NilExpression(node)
   return B.literal(nil)
end
function match:PropertyDefinition(node)
   node.value.generator = node.generator
   return self:get(node.value)
end
function match:BlockStatement(node)
   return B.blockStatement(self:list(node.body))
end
function match:ExpressionStatement(node)
   return B.expressionStatement(self:get(node.expression))
end
function match:CallExpression(node)
   local callee = node.callee
   if callee.type == 'MemberExpression' and not callee.computed then
      if callee.namespace then
         return B.callExpression(self:get(callee), self:list(node.arguments))
      else
         local recv = self:get(callee.object)
         local prop = self:get(callee.property)
         return B.sendExpression(recv, prop, self:list(node.arguments))
      end
   else
      local args = self:list(node.arguments)
      --table.insert(args, 1, B.literal(nil))
      return B.callExpression(self:get(callee), args)
   end
end
function match:NewExpression(node)
   return B.callExpression(B.identifier('new'), {
      self:get(node.callee), unpack(self:list(node.arguments))
   })
end
function match:WhileStatement(node)
   local loop = B.identifier(util.genid())
   local save = self.loop
   self.loop = loop
   local body = B.blockStatement{
      self:get(node.body);
      B.labelStatement(loop);
   }
   self.loop = save
   return B.whileStatement(self:get(node.test), body)
end
function match:ForStatement(node)
   local loop = B.identifier(util.genid())
   local save = self.loop
   self.loop = loop

   local name = self:get(node.name)
   local init = self:get(node.init)
   local last = self:get(node.last)
   local step = B.literal(node.step)
   local body = B.blockStatement{
      self:get(node.body);
      B.labelStatement(loop)
   }
   self.loop = save

   return B.forStatement(B.forInit(name, init), last, step, body)
end
function match:ForInStatement(node)
   local loop = B.identifier(util.genid())
   local save = self.loop
   self.loop = loop

   local none = B.tempnam()
   local temp = B.tempnam()
   local iter = B.callExpression(B.identifier('__each__'), { self:get(node.right) })

   local left = { }
   for i=1, #node.left do
      left[i] = self:get(node.left[i])
   end

   local body = B.blockStatement{
      self:get(node.body);
      B.labelStatement(loop);
   }
   self.loop = save

   return B.forInStatement(B.forNames(left), iter, body)
end
function match:RangeExpression(node)
   return B.callExpression(B.identifier('__range__'), {
      self:get(node.min), self:get(node.max)
   })
end
function match:TableExpression(node)
   local properties = { }
   for i=1, #node.members do
      local prop = node.members[i]

      local key, val
      if prop.key then
         if prop.key.type == 'Identifier' then
            key = prop.key.name
         elseif prop.key.type == "Literal" then
            key = prop.key.value
         end
      else
         assert(prop.type == "Identifier")
         key = prop.name
      end

      local desc
      if prop.value then
         desc = self:get(prop.value)
      else
         desc = B.identifier(key)
      end

      properties[key] = desc
   end

   return B.table(properties)
end

local function countln(src, pos, idx)
   local line = 0
   local index, limit = idx or 1, pos
   while index <= limit do
      local s, e = string.find(src, "\n", index, true)
      if s == nil or e > limit then
         break
      end
      index = e + 1
      line  = line + 1
   end
   return line 
end

local function transform(tree, src)
   local self = { }
   self.line = 1
   self.pos  = 0

   self.ctx = Context.new()

   function self:sync(node)
      local pos = node.pos
      if pos ~= nil and pos > self.pos then
         local prev = self.pos
         local line = countln(src, pos, prev + 1) + self.line
         self.line = line
         self.pos = pos
      end
   end

   function self:get(node, ...)
      if not match[node.type] then
         error("no handler for "..tostring(node.type))
      end
      self:sync(node)
      local out = match[node.type](self, node, ...)
      out.line = self.line
      return out
   end

   function self:list(nodes, ...)
      local list = { }
      for i=1, #nodes do
         list[#list + 1] = self:get(nodes[i], ...)
      end
      return list
   end

   return self:get(tree)
end

return {
   transform = transform
}
