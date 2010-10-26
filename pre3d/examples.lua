
local Pre3d = require 'pre3d/pre3d'
local ShapeUtils = require 'pre3d/pre3d_shape_utils'

local function setTransform(ct, rx, ry, dz)
   ct:reset()
   ct:rotateZ(0)
   ct:rotateY(ry)
   ct:rotateX(rx)
   ct:translate(0, 0, dz and -dz or -80)
end

local function draw(renderer, shape)
   renderer:bufferShape(shape)
   renderer:drawBuffer()
   renderer:emptyBuffer()
end

function star(r)
   local a, ae = 54*pi/180, 72*pi/180
   local li, hi = r*cos(a), r*sin(a)
   local he = li*tan(ae)
   local xv, yv = 0, - hi - he
   local xb, yb = - li, - hi
   local p = path(xv, yv)
   p:line_to(xb, yb)
   for k=1, 4 do
      local th = 2*pi*k/5
      p:line_to(xv*cos(th) + yv*sin(th), yv*cos(th) - xv*sin(th))
      p:line_to(xb*cos(th) + yb*sin(th), yb*cos(th) - xb*sin(th))
   end
   p:close()
   return p
end

function demo1()
   local win = window()
   win:layout('v(h..).')

   p1 = plot 'sin'
   p1:addline(fxline(sin, 0, 2*pi), 'blue')
   p2 = plot 'cos'
   p2:addline(fxline(cos, 0, 2*pi), 'green')

   win:attach(p1, '2')
   win:attach(p2, '1,1')

   local plt = canvas('Pre3D')
   local a = 0.6
   plt:limits(-a, -a, a, a)
   local r = rng()
   for k = 1, 50 do
      local x, y = (2*r:get()-1)*a, (2*r:get()-1)*a
      local d = rnd.gaussian(r, 0.015*a) + 0.03*a
      local s = star(d)
      plt:add(s, rgb(0.9, 0.9, 0.9), {}, {{'translate', x=x, y=y}})
      plt:addline(s, rgb(0.6, 0.6, 0.6), {}, {{'translate', x=x, y=y}})
   end
   plt.units = false
   plt.sync = false
   plt:pushlayer()

   win:attach(plt, '1,2')

   local renderer = Pre3d.Renderer(plt)
   -- shape = ShapeUtils.makeSphere(1, 12, 12)
   local shape = ShapeUtils.makeOctahedron()

   ShapeUtils.linearSubdivideTri(shape)
   ShapeUtils.forEachVertex(shape, 
			    function(v, i, s) 
			       -- TODO(deanm): inplace.
			       s.vertices[i] = Pre3d.Math.unitVector3d(v)
			       return false
			    end
			 )
   -- We need to rebuild the normals after extruding the vertices.
   ShapeUtils.rebuildMeta(shape)

   renderer.draw_overdraw = false
   renderer.draw_backfaces = true
   renderer.fill_rgba = rgb(0x4A/255, 0x92/255, 0xBF/255)
   renderer.fill_rgba_backside = rgb(0xBF/255, 0x92/255, 0x4A/255)
   renderer.set_light_intensity = true
   renderer.fill_rgba_alpha = 0.95
   renderer.stroke_rgba = rgb(0x66/255, 0x66/255, 0x66/255)

   renderer.camera.focal_length = 30;

   local N, tour = 256, 2*pi
   for j=0, N do
      local a = tour*j/N
      setTransform(renderer.camera.transform, a, 0.15 * a)
      draw(renderer, shape)
   end
end


function demo2()
   local win = window()
   local plt = canvas 'Pre3D'
   plt:limits(-1, -1, 1, 1)
   plt.sync = false
   win:attach(plt, '')

   local renderer = Pre3d.Renderer(plt)
   local shape = ShapeUtils.makeXYFunction(|x,y| 1.2*exp(-x^2-y^2), -2, -2, 2, 2)

   renderer.draw_overdraw = false
   renderer.draw_backfaces = true
   renderer.fill_rgba = rgb(0x4A/255, 0x92/255, 0xBF/255)
   renderer.fill_rgba_backside = rgb(0xBF/255, 0x92/255, 0x4A/255)
   renderer.set_light_intensity = true
   renderer.draw_overdraw = true
--   renderer.stroke_rgba = rgb(0x66/255, 0x66/255, 0x66/255)

   renderer.camera.focal_length = 30;

   local N, tour = 256, 2*pi
   for j=0, N do
      local a = tour*j/N
      setTransform(renderer.camera.transform, -a, -0.15*a)
      draw(renderer, shape)
   end
end


function demo2bis()
   local function fnorm(x, y, s)
      local x0, y0 = s[2], s[3]
      local sx, sy = s[4], s[5]
      return exp(-(x-x0)^2/(2*sx^2) - (y-y0)^2/(2*sy^2))
   end

   local makepeaks = function(ls)
			return function(x, y)
				  local z = 0
				  for j, s in ipairs(ls) do
				     z = z + s[1] * fnorm(x, y, s)
				  end
				  return z
			       end
		     end

   local f3ps = makepeaks {{6, 0, 0, 1, 1}, 
			   {-5, 1.5, 1, 1.45, 1.15}, 
			   {4, 2, -2, 0.8, 0.8}}

   local win = window('white')
   win:setview(-1, -1, 1, 1)

   local renderer = Pre3d.Renderer(win)
   local shape = ShapeUtils.makeXYFunction(f3ps, -4, -6, 6, 6, 40, 40)

   renderer.draw_overdraw = true
   renderer.draw_backfaces = true
   renderer.fill_rgba = rgb(0x4A/255, 0x92/255, 0xBF/255)
   renderer.fill_rgba_backside = rgb(0xBF/255, 0x92/255, 0x4A/255)
   renderer.set_light_intensity = true
   renderer.draw_overdraw = true
--   renderer.stroke_rgba = rgb(0x66/255, 0x66/255, 0x66/255)

   renderer.camera.focal_length = 10;

   local N, tour = 256, 2*pi
   for j=0, N do
      local a = tour*j/N
      setTransform(renderer.camera.transform, -a, -0.15*a, 100)
      draw(renderer, shape)
   end
--   local N, tour = 256, 2*pi
--   for j=0, N do
--      local a = tour*j/N
--   local a = 2*pi
--   setTransform(renderer.camera.transform, -a, -0.15*a, 100)
--   draw(renderer, shape)
end


function demo3()
   local win = window()
   local plt = canvas 'Pre3D'
   plt:limits(-1, -1, 1, 1)
   plt.sync = false
   win:attach(plt, '')

   local renderer = Pre3d.Renderer(plt)
   local shape = ShapeUtils.makeSphere(1, 12, 12)

   renderer.draw_overdraw = true
   renderer.draw_backfaces = false
   renderer.fill_rgba = rgb(0x4A/255, 0x92/255, 0xBF/255)
   renderer.fill_rgba_backside = rgb(0xBF/255, 0x92/255, 0x4A/255)
   renderer.set_light_intensity = true
--   renderer.fill_rgba_alpha = 0.95
--   renderer.stroke_rgba = rgb(0x66/255, 0x66/255, 0x66/255)

   renderer.camera.focal_length = 40;

   local N, tour = 256, 2*pi
   for j=0, N do
      local a = tour*j/N
      setTransform(renderer.camera.transform, a, 0.15 * a)
      draw(renderer, shape)
   end
end

function demo4()
   local win = window('black')
   win:setview(-1, -1, 1, 1)

   local renderer = Pre3d.Renderer(win)
   local x = |u,v| (1 + 1/2 * v *cos(u/2))*cos(u)
   local y = |u,v| (1 + 1/2 * v *cos(u/2))*sin(u)
   local z = |u,v| 1/2 * v * sin(u/2)
   local shape = ShapeUtils.makeUVSurface(y, z, x, 0, -1, 2*pi, 1, 60, 4)

   renderer.draw_overdraw = false
   renderer.draw_backfaces = true
   renderer.fill_rgba = rgb(0x4A/255, 0x92/255, 0xBF/255)
   renderer.fill_rgba_backside = rgb(0xBF/255, 0x92/255, 0x4A/255)
   renderer.set_light_intensity = true
   renderer.draw_overdraw = true
--   renderer.stroke_rgba = rgb(0x66/255, 0x66/255, 0x66/255)

   renderer.camera.focal_length = 40;

   local N, tour = 256, 2*pi
   for j=0, N do
      local a = tour*j/N
      setTransform(renderer.camera.transform, -a, -0.15*a)
      draw(renderer, shape)
   end
end
