type Person = {
	id: string,
    name: string,
   	age: int,
	fn: (number, number) => number
}

type PersonWithoutFn = {
	id: string,
    name: string,
   	age: int
}

let M: Contract<PersonWithoutFn> = {}

function M:init()

end

function M:func1()
	let id: string = self.id
	let name: string = self.name
	let storage: PersonWithoutFn = self.storage
	let age: int = storage.age
	let func2 = self.func2
end

return M