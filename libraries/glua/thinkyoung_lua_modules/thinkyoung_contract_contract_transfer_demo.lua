local M = {}


function M:init()
	print("test contract transfer init")
end

function M:start()
	print("test contract transfer start")
	transfer_from_contract_to_address('A', 'ALP', 50)
    return 123
end

return M
