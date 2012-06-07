-- The following regression test expects regress_c60Dir to be a global
-- variable that contains a valid path to the c60 dataset.

-- Available variables:
-- regress_c60Dir

-- Test successive creation/deletion of a render window.
local r1 = iv3d.renderer.new(regress_c60Dir)
local r1id = getClassUNID(r1)
local r1rawid = getClassUNID(r1.getRawRenderer())
print('r1id: ' .. r1id)
print('r1rawid: ' .. r1rawid)
deleteClass(r1)

local r2 = iv3d.renderer.new(regress_c60Dir)
local r2id = getClassUNID(r2)
local r2rawid = getClassUNID(r2.getRawRenderer())
print('r2id: ' .. r2id)
print('r2rawid: ' .. r2rawid)
deleteClass(r2)

provenance.undo()
r2 = getClassWithUNID(r2id)
r2.setBGColors({1,0,0},{1,1,0}) -- Set annoying BG colors
if r2id ~= getClassUNID(r2) then
   error('Id\'s not the same.')
end
