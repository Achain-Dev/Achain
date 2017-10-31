														   local t = require 'math'
t.hello = 123
local d = t.hello
-- local c = _G.require

local hello2 = 456

local M = {}

-- M2 = {}

function M.init2() 
   local age = 25
end


function M.b()
	-- d = c
	-- local e = _ENV
	local c = 1
end

print('this is test_wrong_contract.lua\n')

return M