type Person = {
	id: string,
    name: string,
   	age: int
	, fn: (number, number) => number
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

type G2<T> = G1<int, T, Contract2<Person> >

type G3 = G2<string>

let r1: G2<Person> = {}

let r2: G2<string> = G2<string>()

let t2 : Person = {name="hello", age=20}

--t2.fn = function(a: number, b: number) 
--	return a + b 
--end

let r3: G1<int, int> = {}

pprint(t2.name)
pprint(t2.not_found)
pprint(t2['not_found2'])

t2.age = t2['age'] + 1