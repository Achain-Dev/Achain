local t = require 'math'
t.hello = 123
local d = t.hello
-- local c = _G.require

local hello2 = 456

local M = {}

-- M2 = {}

function M:init() 
   local age = 25
end

function M:start()
	local age = 100
end

function M.b()
	-- d = c
	-- local e = _ENV
	local c = 1
	local demo2 = import_contract 'demo2'
end

local f2 = function()
	local c = 1
	local f1 = function ()
		local n = 0
		return function()
			n = n + 1
			return function()
				return c + n
			end
		end					
	end
	local e1 = f1()
	return e1
end

pprint('this is test4.lua\n')

return M