-- The following code tests mathematical methods exposed via the __index
-- metamethod.

header = 'Indexmethods test: '

-------------------------------------
-- Test matrix __index metamethods --
-------------------------------------
m1 = math.m4x4()
m1[1] = math.v4(2.0, 0.0, 0.0, 0.0)
m1[2] = math.v4(0.0, 4.0, 0.0, 0.0)
m1[3] = math.v4(0.0, 0.0, 8.0, 0.0)
m1[4] = math.v4(0.0, 0.0, 0.0, 16.0)

-- Super simplified inversion test
r = m1:inverse()
if r[1][1] ~= 0.5 or r[2][2] ~= 0.25 or r[3][3] ~= 0.125 or r[4][4] ~= 0.0625 then
  error(header .. 'Failed 4x4 matrix inversion test.')
end
print('Passed matrix inversion test.')

-- Transpose test
m1[1] = math.v4(0.0, 0.0, 0.0, 4.0)
m1[2] = math.v4(0.0, 0.0, 3.0, 0.0)
m1[3] = math.v4(0.0, 2.0, 0.0, 0.0)
m1[4] = math.v4(1.0, 0.0, 0.0, 0.0)

r = m1:transpose()
if r[4][1] ~= 4.0 or r[3][2] ~= 3.0 or r[2][3] ~= 2.0 or r[1][4] ~= 1.0 then
  error(header .. 'Failed 4x4 matrix transpose test.')
end
print('Passed matrix transpose test.')
