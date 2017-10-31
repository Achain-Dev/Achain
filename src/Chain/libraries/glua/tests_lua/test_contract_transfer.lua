local M = {}


function M:init()
	print("test contract transfer caller")
	
	local contract_transfer_demo = import_contract 'contract_transfer_demo'
	local res = contract_transfer_demo:start()
    pprint("transfer demo start response is ", res)
end

function M:start()

end

return M
