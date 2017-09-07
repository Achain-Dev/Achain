type Person = {
	id: string,
    name: string,
   	age: int,
	age2 ?: number default 1,
	age3: int = 24,
	fn: (number, number) => number
}

type Contract3 = {
	id: string,
	name: string,
	storage: Person
}

type Contract2<S> = {
	id: string,
	name: string,
	storage: S,
	sayHi: (int) => S
}

type G1<T1, T2, T3> = {
	a: T1,
	b: T2,
	c: T3
}

local r1: Contract2<Person> = {}

let t2 : Person = {name="hello", age=20}

t2.fn = function(a: number, b: number) 
	return a + b 
end

let r3: G1<int, int> = {}

pprint(t2.name)
pprint(t2.not_found)
pprint(t2['not_found2'])

t2.age = t2['age'] + 1

let as3 = Contract2<Person> {a=1, b="hi"}
pprint('as3', as3)
let as4: Array<Person> = Array<Person> ([ 1, 2, 3, 4 ])
let as5 = Person {name="glua"}
pprint('as5', as5)