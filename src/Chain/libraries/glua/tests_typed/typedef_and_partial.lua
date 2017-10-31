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

type G4 = G1<int, T, Person>

type G5 = G1<int, int, int, int>

type G6 = G1<int, int>

type G7 = string

type G8 = G3

type G9 = {
	a: T
}