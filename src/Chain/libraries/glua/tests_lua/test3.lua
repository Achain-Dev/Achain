print("Hello, this is test3.lua\n")

local M = {}

local fname = 'abc'

function M:init() 
	pprint('test3 contract demo init')
	-- local dd = import_contract 'not_found'
	-- local dd2 = import_contract('not_found')
	local storage = {}
	storage['name'] = 123
	local abc = "\x23\xa3\u{1234}\5\6\zabc"
	print(abc)
	local a2 = [[abcdef]]
	local d = 123.45 + 2
	local e = fname
end

offline function M:start(name)
	pprint('test3 contract demo start ', name)
end

-- a=1

return M