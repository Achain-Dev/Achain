type Person = {
	id: string default '123',
    name: string default "hi",
   	age: int default 24,
	age2: number,
	age3 ?: int default 1,
	fn: (number, number) => number default function (a: number, b: number) return a + b; end
}

let p11: Person = Person()

local as1, as2

as1, as2, as2, as1 = 1, 'abc', 'd'

as3[p11]

type Array2<T> = {
	__len: () => int,
	__index: (int) => T
}

let as3 = Array2<Person> {a=1, b="hi"}
pprint('as3', as3)
let as4: Array<Person> = Array<Person> { 1, 2, 3, 4 }
let as5 = Person {name="glua"}
pprint('as5', as5)

type artT = Array2<string>
let ar1: artT = artT()

let ar2: string = ar1[1]
let ar3 = p11[1]

let ar4: Array2<string> = Array2<string>()


type Contract = {
	id: string,
	name: string,
	storage: Person
}

type Contract2<S> = {
	id: string,
	name: string,
	storage: S,
	sayHi: (int) => S
	-- ,__call: (S) => S
}

type G1<T1, T2, T3> = {
	a: T1,
	b: T2,
	c: T3
	-- message: string default '123'
}

type G2<T> = G1<int, T, Contract2<Person> >

type G3 = G2<string>

-- type G4 = string

type Cat = "whiteCat" | "blackCat" | nil
type Dog = "husky" | "corgi"

type Pets = Cat | Dog       -- The same as: type Pets = "whiteCat" | "blackCat" | "husky" | "corgi" | nil

type Falsy = "" | 0 | false | nil

let M: Contract2<Person> = {}

local fname = 'abc'

let ot1 ?: string = "123"

local function testlocalfunc1(b, c, d)
	return true
end

offline function M:query(a)
	return tostring(1234)
end


function add(a: number, b: number)
	return a + b
end

function sum(a: number, b: number, ...)
    var s = a + b
	for i in ipairs(...) do
	   s = s + tonumber(i)
	end
	return s
end

function test_dots(...)
	print('test_dots\n')
	local a = {}
	local b = 0
	--[[
	for k, v in ipairs(...) do
		a[#a+1] = v
	end
	local b
	if a and #a > 0 then
		b = a[1]
	else
		b = '0'
	end
	print('b ', b, '\n')
	]]--
   return tonumber(b)
end

let td1: number = test_dots()
let td2: number = test_dots(1)
let td3: number = test_dots(1, '2')

function numberf1(f: (number, number) => number, a: number, b: number)
   return f(a, b)
end

function M:init() 
	-- 这里静态类型分析时，self如果没有特别声明类型，就用M的类型
	pprint('test3 contract demo init')
	-- local dd = import_contract 'not_found'
	-- local dd2 = import_contract('not_found')
	local storage = {}
	storage['name'] = 123
	var id = self.id
	let tf1 = storage.id(123)
	local tf2: G3 = {}
	local tf3 = tf2.b

	local tf4: Array<int> = { 1, 2, 3 }
	local tf5: int = tf4[2]

	local tf6: (int, string, bool) => string
	local tf7 = tf6(1, '1', true)
	local tf8: (number, number) => number = add
	local tf9 = self.sayHi	-- TODO

	local age = self.storage.age
	tonumber '123'
	
	local abc = "\x23\xa3\u{1234}\5\6\zabc"
	print(abc)
	local a2 = [[abcdef]]
	local a3 = #storage
	local r1: Contract2<Person> = {}
	local r2 = r1.storage
	function f1(a)
		if true then
			return 'abc'
		end
	   return tonumber(a)
	end
	let f1r1 = f1(1) 
	let f1r2 = tostring(f1r1) .. 'f1'
	let t2 : Person = {name="hello", age=20}
	t2.fn = function(a: number, b: number) 
		return a + b 
	end
	local t1: int = 1
	pprint(t2.name)
	pprint(t2.not_found)
	pprint(t2['not_found2'])
	let t3 = add(t1, t2)
	local t4 = add(t1, t1, t1, t1)
	t3 = 100
	t2.age = t2['age'] + 1
	t2 = totable({}) -- table和record可以分别作为变量类型和值使用，但是record值不能赋值给另一种record类型的variable
	t1 = 'abc'
	transfer_from_contract_to_address(123)
	local a4 = #storage.name + 1
	local d = 123.45 + 2 + #storage.name  + tonumber('123') + "aaa" + 1 	 	  
	local e = fname
	pprint 'hi'

	let t5: int | string | Person | bool | table = '123'
	let t6: int | string = '123'

	let t7: Contract2<Person> = {}
	let t8: Person = {}
	-- let t9: Person = t7(t8)   
	let t10: Function = add
	let t11 = t10(123, 456, 789)
end



function M:start()
	local a = self.id
end

offline function M:query(a: number, b: number)
	return a + b
end

return M