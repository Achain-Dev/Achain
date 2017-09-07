type Person = {
	id: string,
    name: string,
   	age: int,
	is_admin: bool,
	fn: (number, number) => number
}

local function add(a: number, b: number)
	return a + b
end

let p1: Person = {
	name = "test", age=100, id='test', is_admin=true, fn=add
}

let id: string = p1.id
let name: string = p1.name
let age: number = p1['age']
let is_admin: bool = p1.is_admin
let not_found = p1.not_found
let not_found2 = p1['not_found2']
let fn: (number, number) => number = p1.fn

let as3 = {}

as3[p1]

type Array2<T> = {
	__len: () => int,
	__index: (int) => T
}

type artT = Array2<string>
let ar1: artT = artT()

let ar2: string = ar1[1]
let ar3 = p1[1]

let tbl1 = {}
let tbl1_prop1 = tbl1.name
let tbl1_prop2 = tbl1['name']
let tbl1_key1 = 'name'
let tbl1_prop3 = tbl1[tbl1_key1]
var tbl1_key2 = nil
tbl1[tbl1_key2] = 'hello'
