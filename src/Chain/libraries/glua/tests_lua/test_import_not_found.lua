local M = {}

function M:init()
   pprint('init import not found')
end

function M:start()
   pprint('start import not found')
   local demo2 = import_contract 'not_found'
end

return M