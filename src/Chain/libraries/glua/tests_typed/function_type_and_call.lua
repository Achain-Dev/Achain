local add1 = function (a: number, b: number)
	return a + b
end

local function add2(a: number, b)
   return add1(a, tonumber(b))
end

let add3: (number, number) => number

add3 = add1

let a1 = add3(1, 2)
let a2 = add1(1)
let a3 = add1('1')
let a4 = add1('1', '2')
let a5 = add1(1, 2, 3, 4)
let a6 = add1('1', '2', '3', '4') 

local function plus1(n)
   return tonumber(n) + 1
end

let a7 = plus1(123)
let a8 = plus1('123')
let a81 = plus1 '123'

let a9: Function = add1
let a10 = a9(123, 456, 789)