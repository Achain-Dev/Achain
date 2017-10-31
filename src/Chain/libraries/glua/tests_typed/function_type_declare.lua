local function add1(a, b) 
   return a + b
end

local add2 = function (a: number, b: number)
	return a + b
end

local function add3(a: number, b)
   return add2(a, tonumber(b))
end

let add4: (number, number) => number = add2
let add5: (number, number) => number = add3

local function return1(a: number, b)
	return a, b
end

type Gender = {
	type: int,
	text: string
}

type Person = {
	name: string,
	age: int,
	gender: Gender,
	add: (number, number) => number
}

let p1: Person = {}
p1.gender = {}
p1.gender.type = 1
p1.add = add4
p1.add = add3