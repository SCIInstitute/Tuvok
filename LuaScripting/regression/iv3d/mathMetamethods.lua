-- The following code tests the mathematical metamethods that are supplied
-- in Lua.

header = 'Metamethods test: '

-- Test vector metamethods
v1 = math.v4(1.0, 1.0, 1.0, 1.0)
v2 = math.v4(0.0, 1.0, 2.0, 4.0)
a = 10
b = 100

print('Contents of v1.')
print('Using ipairs.')
for i,v in ipairs(v1) do print(i .. ': ' .. v) end

print('Using # length unary operator. Valid because we use integer indices.')
for i=1,#v1 do
  print(i .. ': ' .. v1[i])
end

print('Pretty printed vector:')
helpstr(v1)

print('Metatable:')
for i,v in ipairs(getmetatable(v1)) do print(i .. ': ' .. v) end

print('Contents of v2: ' .. helpstr(v2))

-----------------------------------------
-- Test scalar * vector multiplication --
-----------------------------------------
v = a * v1
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpstr(v))

v = v1 * a
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpstr(v))

v = b * v2
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpstr(v))

v = v2 * b
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpstr(v))

-----------------------------
-- Test vector dot product --
-----------------------------
r = v1 * v2
if r < 6.999 or r > 7.001 then
  error(header .. 'Failed vector dot product.')
end
print('Passed dot product: ' .. r)

r = v2 * v1
if r < 6.999 or r > 7.001 then
  error(header .. 'Failed vector dot product.')
end
print('Passed dot product: ' .. r)

--------------------------
-- Test vector addition --
--------------------------
v = v1 + v2
if v[1] ~= 1.0 or v[2] ~= 2.0 or v[3] ~= 3.0 or v[4] ~= 5.0 then
  error(header .. 'Failed vector addition.')
end
print('Passed vector addition: ' .. helpstr(v))

v = v2 + v1
if v[1] ~= 1.0 or v[2] ~= 2.0 or v[3] ~= 3.0 or v[4] ~= 5.0 then
  error(header .. 'Failed vector addition.')
end
print('Passed vector addition: ' .. helpstr(v))

-----------------------------
-- Test vector subtraction --
-----------------------------
vr = v1 - v2
if vr[1] ~= 1.0 or vr[2] ~= 0.0 or vr[3] ~= -1.0 or vr[4] ~= -3.0 then
  error(header .. 'Failed vector subtraction.')
end
print('Passed vector subtraction: ' .. helpstr(vr))

vr = v2 - v1
if vr[1] ~= -1.0 or vr[2] ~= 0.0 or vr[3] ~= 1.0 or vr[4] ~= 3.0 then
  error(header .. 'Failed vector subtraction.')
end
print('Passed vector subtraction: ' .. helpstr(vr))

--v1 = math.v4(1.0, 1.0, 1.0, 1.0)
--v2 = math.v4(0.0, 1.0, 2.0, 4.0)
-------------------------
-- Test unary negation --
-------------------------
vr = -v1
if vr[1] ~= -1.0 or vr[2] ~= -1.0 or vr[3] ~= -1.0 or vr[4] ~= -1.0 then
  error(header .. 'Failed unary negation.')
end
print('Passed unary negation: ' .. helpstr(vr))

vr = -v2
if vr[1] ~= 0.0 or vr[2] ~= -1.0 or vr[3] ~= -2.0 or vr[4] ~= -4.0 then
  error(header .. 'Failed unary negation.')
end
print('Passed unary negation: ' .. helpstr(vr))


-- Test scalar * matrix multiplication
--m1 = math.matrix([[2.0 0.0 0.0 0.0],
--                  [0.0 2.0 0.0 0.0],
--                  [0.0 0.0 1.0 0.0],
--                  [0.0 0.0 0.0 1.0]])
--r = 10 * m1
--r = m1 * 10
