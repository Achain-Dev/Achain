type Person = {
	id: string,
    name: string,
   	age: int,
	fn: (number, number) => number
}

type C2<S> = {
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

type G2<T> = G1<int, T, C2<Person> >

type G3 = G2<string>

let M: C2<Person> = {}

let storage: Person = M.storage
let	age: int = storage.age
let not_found = storage.not_found
