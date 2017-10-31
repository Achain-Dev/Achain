local M = {}

function M:init()

end

function M:start()
	pprint("test exit contract start")
	local a = 1 > 'a'
	exit('hello')
	local thinkyoung = require 'thinkyoung'
	thinkyoung.error("error")
	pprint("after exit")
end

pprint("test exit contract loaded")

return M