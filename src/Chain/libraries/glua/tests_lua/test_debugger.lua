print("Hello, this is test_debugger.lua")

local M = {}

local fname = 'abc'

function M:init() 
	pprint('test3 contract demo init')
end

function M:start(name)
	pprint('test3 contract demo start ', name)
	local storage = {}
	storage['name'] = 123
	debugger()
	local abc = "\x23\xa3\u{1234}\5\6\z
	abc"
	print(abc)
	local a2 = [[abcdef]]
	debugger()
	local d = 123.45
	local e = fname
	debugger()
end

return M