 local M = {}

 function M:init()
	 pprint('test error use api case init')
	 local demo2 = import_contract 'demo2'
	 -- demo2:not_found('hi')
	 -- transfer_from_contract_to_address(100, 50.5)
 end

 function M:start()
	 pprint("test error use api case start")
	 local b = string.find('a')
	 local b = {}
	 local c = b.hi()
	 local a = nil
	 a.hi()
	 local d = import_contract()
	 pprint(d)
	 d.hi()
 end

 return M