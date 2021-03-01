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
helpStr(v1)

print('Metatable:')
for i,v in ipairs(getmetatable(v1)) do print(i .. ': ' .. v) end

print('Contents of v2: ' .. helpStr(v2))

-----------------------------------------
-- Test scalar * vector multiplication --
-----------------------------------------
v = a * v1
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpStr(v))

v = v1 * a
if v[1] ~= 10.0 or v[2] ~= 10.0 or v[3] ~= 10.0 or v[4] ~= 10.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpStr(v))

v = b * v2
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpStr(v))

v = v2 * b
if v[1] ~= 0.0 or v[2] ~= 100.0 or v[3] ~= 200.0 or v[4] ~= 400.0 then
  error(header .. 'Failed scalar/vector multiplication.')
end
print('Passed scalar multiplication: ' .. helpStr(v))

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
print('Passed vector addition: ' .. helpStr(v))

v = v2 + v1
if v[1] ~= 1.0 or v[2] ~= 2.0 or v[3] ~= 3.0 or v[4] ~= 5.0 then
  error(header .. 'Failed vector addition.')
end
print('Passed vector addition: ' .. helpStr(v))

-----------------------------
-- Test vector subtraction --
-----------------------------
vr = v1 - v2
if vr[1] ~= 1.0 or vr[2] ~= 0.0 or vr[3] ~= -1.0 or vr[4] ~= -3.0 then
  error(header .. 'Failed vector subtraction.')
end
print('Passed vector subtraction: ' .. helpStr(vr))

vr = v2 - v1
if vr[1] ~= -1.0 or vr[2] ~= 0.0 or vr[3] ~= 1.0 or vr[4] ~= 3.0 then
  error(header .. 'Failed vector subtraction.')
end
print('Passed vector subtraction: ' .. helpStr(vr))

--v1 = math.v4(1.0, 1.0, 1.0, 1.0)
--v2 = math.v4(0.0, 1.0, 2.0, 4.0)
-------------------------
-- Test unary negation --
-------------------------
vr = -v1
if vr[1] ~= -1.0 or vr[2] ~= -1.0 or vr[3] ~= -1.0 or vr[4] ~= -1.0 then
  error(header .. 'Failed unary negation.')
end
print('Passed unary negation: ' .. helpStr(vr))

vr = -v2
if vr[1] ~= 0.0 or vr[2] ~= -1.0 or vr[3] ~= -2.0 or vr[4] ~= -4.0 then
  error(header .. 'Failed unary negation.')
end
print('Passed unary negation: ' .. helpStr(vr))







-- Test matrix metamethods
print ('Beginning matrix tests')
m1 = math.m4x4()  -- Constructs an identity matrix by default.
vt = math.v4(1.0, 2.0, 3.0, 4.0)
print('Contents of m1: ' .. helpStr(m1))

-----------------------------------------
-- Test Matrix * scalar multiplication --
-----------------------------------------
r = m1 * a
if r[1][1] ~= 10.0 or r[2][2] ~= 10.0 or r[3][3] ~= 10.0 or r[4][4] ~= 10.0 then
  error(header .. 'Failed matrix scalar multiplication.')
end
-- Perform dot products against rows to verify...
if r[1] * vt ~= 10.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 1.')
end
if r[2] * vt ~= 20.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 2.')
end
if r[3] * vt ~= 30.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 3.')
end
if r[4] * vt ~= 40.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 4.')
end
print('Passed scalar right multiplication')
print('Contents of r: ' .. helpStr(r))


r = a * m1
-- Perform dot products against rows to verify...
if r[1] * vt ~= 10.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 1.')
end
if r[2] * vt ~= 20.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 2.')
end
if r[3] * vt ~= 30.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 3.')
end
if r[4] * vt ~= 40.0 then
  error(header .. 'Failed matrix scalar multiplication; dot product 4.')
end
print('Passed scalar left multiplication')
print('Contents of r: ' .. helpStr(r))

-----------------------------------------
-- Test Matrix * vector multiplication --
-----------------------------------------
r = 2 * m1
r = r * math.v4(1.0, 2.0, 3.0, 4.0)
if r[1] ~= 2.0 or r[2] ~= 4.0 or r[3] ~= 6.0 or r[4] ~= 8.0 then
  error(header .. 'Failed matrix * vector multiplication')
end
print('Passed matrix * vector multiplication.')

-----------------------------------------
-- Test matrix * matrix multiplication --
-----------------------------------------
m1 = math.m4x4();
m1[1] = math.v4(1.0, 2.0, 3.0, 4.0)
m1[2] = math.v4(4.0, 3.0, 2.0, 1.0)
m1[3] = math.v4(0.0, 1.0, -1.0, 0.0)
m1[4] = math.v4(-20.0, 0.0, 3.0, 4.0)
print('m1: ' .. helpStr(m1))

m2 = math.m4x4();
m2[1] = math.v4(1.0, 1.0, 1.0, 1.0)
m2[2] = math.v4(-1.0, -1.0, 1.0, 1.0)
m2[3] = math.v4(1.0, -1.0, 1.0, -1.0)
m2[4] = math.v4(1.0, 1.0, -1.0, -1.0)
print('m2: ' .. helpStr(m2))

r = m1 * m2
if r[1][1] ~= m1[1][1]*m2[1][1]+m1[1][2]*m2[2][1]+m1[1][3]*m2[3][1]+m1[1][4]*m2[4][1] or
   r[1][2] ~= m1[1][1]*m2[1][2]+m1[1][2]*m2[2][2]+m1[1][3]*m2[3][2]+m1[1][4]*m2[4][2] or 
   r[1][3] ~= m1[1][1]*m2[1][3]+m1[1][2]*m2[2][3]+m1[1][3]*m2[3][3]+m1[1][4]*m2[4][3] or
   r[1][4] ~= m1[1][1]*m2[1][4]+m1[1][2]*m2[2][4]+m1[1][3]*m2[3][4]+m1[1][4]*m2[4][4] or
   r[2][1] ~= m1[2][1]*m2[1][1]+m1[2][2]*m2[2][1]+m1[2][3]*m2[3][1]+m1[2][4]*m2[4][1] or
   r[2][2] ~= m1[2][1]*m2[1][2]+m1[2][2]*m2[2][2]+m1[2][3]*m2[3][2]+m1[2][4]*m2[4][2] or 
   r[2][3] ~= m1[2][1]*m2[1][3]+m1[2][2]*m2[2][3]+m1[2][3]*m2[3][3]+m1[2][4]*m2[4][3] or
   r[2][4] ~= m1[2][1]*m2[1][4]+m1[2][2]*m2[2][4]+m1[2][3]*m2[3][4]+m1[2][4]*m2[4][4] or
   r[3][1] ~= m1[3][1]*m2[1][1]+m1[3][2]*m2[2][1]+m1[3][3]*m2[3][1]+m1[3][4]*m2[4][1] or
   r[3][2] ~= m1[3][1]*m2[1][2]+m1[3][2]*m2[2][2]+m1[3][3]*m2[3][2]+m1[3][4]*m2[4][2] or 
   r[3][3] ~= m1[3][1]*m2[1][3]+m1[3][2]*m2[2][3]+m1[3][3]*m2[3][3]+m1[3][4]*m2[4][3] or
   r[3][4] ~= m1[3][1]*m2[1][4]+m1[3][2]*m2[2][4]+m1[3][3]*m2[3][4]+m1[3][4]*m2[4][4] or
   r[4][1] ~= m1[4][1]*m2[1][1]+m1[4][2]*m2[2][1]+m1[4][3]*m2[3][1]+m1[4][4]*m2[4][1] or
   r[4][2] ~= m1[4][1]*m2[1][2]+m1[4][2]*m2[2][2]+m1[4][3]*m2[3][2]+m1[4][4]*m2[4][2] or 
   r[4][3] ~= m1[4][1]*m2[1][3]+m1[4][2]*m2[2][3]+m1[4][3]*m2[3][3]+m1[4][4]*m2[4][3] or
   r[4][4] ~= m1[4][1]*m2[1][4]+m1[4][2]*m2[2][4]+m1[4][3]*m2[3][4]+m1[4][4]*m2[4][4] then
  error(header .. 'Failed matrix * matrix multiplication')
end
print('Passed matrix * matrix multiplication.')
print('Result of multiplication: ' .. helpStr(r))


