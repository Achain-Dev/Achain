type Person = {
	name: string,
	age: int,
	sayHi: (string) => string
}

type ErrorRecord = {
	name,
	age: int
}

let a1: int
let a2:int, b2, c2: string


let a3: Person, a4: (number, number) => number
let a41: number, b4: Person = 3, {name='test', age=100}

function sayHi2(name: string)
	return name
end

local a5: Array<int> = [ 1, 2, 3 ]
local a6: int = a5[2]
local a7: Array<Person> = [ {name="hi", age=123, sayHi=sayHi2} ]
local a8: int = a7[1].age
local a9: string = a7[1].sayHi('123')

let a10 ?: string = 'abc'

type Gender = "Male" | "Female" | true | 123 | 1.23

let m1: Gender = "Male"
let m2: Gender = 1.23
let m3: Gender = true
let m4: Gender = "Chinese"

let m5: string = m1 -- Literal Type类型的变量可以显式降级到string类型

function fm1(p: true)
end

function fm2(p: 'Chinese')
end

fm1(true)
fm2(false)

type Cat = "whiteCat" | "blackCat" | nil
type Dog = "husky" | "corgi"

type Pets = Cat | Dog       -- The same as: type Pets = "whiteCat" | "blackCat" | "husky" | "corgi" | nil

type Falsy = "" | 0 | false | nil

var t101: object = 1
t101 = 'name'
print(t101)
print(t101.gg)