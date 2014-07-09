require "util"

hostname = util.hostname()
print("Hostname is: " .. hostname)
if hostname == "takeo" then
  print("Loading takeo.lua...")
  require "takeo"
elseif hostname == "dhcp-wlcd106227.zim.uni-duisburg-essen.de" then
  print("Loading rakeo.lua...")
  require "rakeo"
else
  error("Can't load proper host file...")
end

print("Initializing renderer")
ren = tuvok.renderer.new(tuvok.renderer.types.OpenGL_SBVR, true, false,
                         false, false)
bsize = {36, 36, 36} -- size of the bricks to use/rebrick into.
ren.loadNetDS(machine.dataset.engine, bsize, MM_PRECOMPUTE)

-- Parameters are: Framebuffer width and height, color bits, depth bits,
--                 stencil bits, double buffer, and if visible.
context = tuvok.createContext(320,240, 32,24,8, true, false)
ren.initialize(context)
ren.resize({320, 240})
ren.setRendererTarget(tuvok.renderer.types.RT_Headless)
mat = matrix.rotateY(80) * matrix.rotateZ(-20) * matrix.rotateX(30)
ren.setRotation(mat)
ren.setSampleRateModifier(8.0)
ren.paint()

ren.setRendererTarget(tuvok.renderer.types.RT_Capture)
ren.captureSingleFrame(outputDir .. '/render.png', true)
ren.cleanup()
deleteClass(ren)
