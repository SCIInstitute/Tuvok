-- The following regression test expects regress_c60Dir to be a global
-- variable that contains a valid path to the c60 dataset.

-- Available variables:
-- regress_c60Dir

-- Stress testing variable. Increase or decrease the amount as needed.
local stress = 2

for i=1,stress do

   local r0 = iv3d.renderer.new(regress_c60Dir)
   deleteClass(r0)

   -- Test successive creation/deletion of a render window.
   local r1 = iv3d.renderer.new(regress_c60Dir)
   local r1id = getClassUNID(r1)
   print('r1id: ' .. r1id)
   local r1rawid = getClassUNID(r1.getRawRenderer())
   print('r1rawid: ' .. r1rawid)
   deleteClass(r1)

   local r2 = iv3d.renderer.new(regress_c60Dir)
   local r2id = getClassUNID(r2)
   print('r2id: ' .. r2id)
   local r2rawid = getClassUNID(r2.getRawRenderer())
   print('r2rawid: ' .. r2rawid)
   deleteClass(r2)

   provenance.undo()
   r2 = getClassWithUNID(r2id)
   -- By issuing the next call, we clear the redo we made with the prior undo.
   r2.setBGColors({1,0,0},{1,1,0}) -- Set annoying BG colors
   if r2id ~= getClassUNID(r2) then
      error('Id\'s not the same.')
   end

   provenance.undo() -- Undoes set background colors
   provenance.undo() -- Undoes creation of r2

   provenance.undo() -- Undoes deletion of r1
   r1 = getClassWithUNID(r1id)
   print('r1raw: ' .. getClassUNID(r1))
   r1.setBGColors({0,1,0},{1,1,0})
   if r1id ~= getClassUNID(r1) then
      error('Id\'s not the same.')
   end

   provenance.undo() -- Undoes set background colors
   provenance.undo() -- Undoes creation of r1

   provenance.undo() -- Undo the deletion of the first renderer
   provenance.undo() -- Undo the creation of the first renderer

   provenance.redo() -- Redo the creation of the first renderer
   provenance.redo() -- Redo the deletion of the first renderer

   provenance.redo() -- Redo the creation of r1
   provenance.redo() -- Redo setting the BG color (at the top of the stack)

   provenance.undo() -- Undo setting bg color
   provenance.undo() -- Undo creation of r1

   provenance.undo() -- Undo deletion of first renderer
   provenance.undo() -- Undo creation of first renderer

   provenance.redo() -- Redo the creation of the first renderer
   provenance.redo() -- Redo the deletion of the first renderer

   provenance.redo() -- Redo the creation of r1
   provenance.redo() -- Redo setting the BG color (at the top of the stack)

   -- Now actually delete the class so we don't have a stack of windows.
   -- We will have to mind undo/redoing this.
   deleteClass(getClassWithUNID(r1id))

end

print('Attempting ' .. stress*5 .. ' undos.')

-- Now we should be able to undo stress * 5 times
for i=1,stress*5 do
   provenance.undo()
end

print('Attempting ' .. stress*5 .. ' redos.')

-- And redo...
for i=1,stress*5 do
   provenance.redo()
end

print('Attempting ' .. stress*5 .. ' undos.')

-- And undo once more to place us where we were first located on the undo stack
for i=1,stress*5 do
   provenance.undo()
end