type Storage = {
	name: string
}

let M: Contract<Storage> = {}

function M:init()

end

let function abc()
	return 123
end

pprint(abc())

function M:start()
	let demo = import_contract('demo2')
	demo:init()
	demo.start(demo)
	demo:start()
	let a = demo.storage.name
	let b = demo.a.b.c:hello(1,2,3)
	let demo2 = import_contract 'not_found'
	let demo3 = import_contract("not_found2")
end

return M