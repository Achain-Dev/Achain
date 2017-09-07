print('test stop vm case\n')
local thinkyoung = require 'thinkyoung'
thinkyoung.stop_vm()

local n = 0
for i = 1, 10 do
	 n = n + i
	 print("after stop vm")
end
print(n)