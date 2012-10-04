-- The following code tests the mathematical metamethods that are supplied
-- in Lua.

header = 'Metamethods test: '

-- Testing vector metamethods
v1 = math.vector4(1.0, 1.0, 1.0, 1.0)
v2 = math.vector4(0.0, 1.0, 2.0, 4.0)
a = 10
b = 100

-- Test scalar * vector multiplication
v = a * v1
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end

v = v1 * a
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end

v = b * v2
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end

v = v2 * b
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end

-- Test vector dot product
r = v1 * v2
if r ~= 7.0 then
  error(header .. 'Failed vector dot product.')
end

r = v2 * v1
if r ~= 7.0 then
  error(header .. 'Failed vector dot product.')
end

-- Test scalar * matrix multiplication
--m1 = math.matrix([[2.0 0.0 0.0 0.0],
--                  [0.0 2.0 0.0 0.0],
--                  [0.0 0.0 1.0 0.0],
--                  [0.0 0.0 0.0 1.0]])
--r = 10 * m1
--r = m1 * 10
