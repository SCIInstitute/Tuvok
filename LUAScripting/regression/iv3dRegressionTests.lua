-- Be sure to update the paths below to valid paths.
-- These tests should be executed with a fresh instance of iv3d.
-- But it's not absolutely mandatory.

local homeDir = os.getenv('HOME')
local c60Dir = homeDir .. '/sci/datasets/c60.uvf'

local rTest = 1

  
local function regressed ()
  error('Regression test ' .. rTest .. ' failed')
end

-- Test successive creation/deletion of a render window.
local r1 = iv3d.renderer.new(c60Dir)
local r1id = getClassUNID(r1)
print('r1id: ' .. r1id)
deleteClass(r1)

local r2 = iv3d.renderer.new(c60Dir)
local r2id = getClassUNID(r2)
print('r2id: ' .. r2id)
deleteClass(r2)

if r2id ~= (r1id + 1) then
  regressed()
end

provenance.undo()
r2 = getClassUNID(r2id)
r2.setBGColors({1,0,0},{1,1,0}) -- Set annoying BG colors
if r2id ~= getClassUNID(r2) then
  regressed()
end
