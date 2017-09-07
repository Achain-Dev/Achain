let a1 = 1
let a2 = '123'
let a3 = true
let a4 = {}

type Person = {
	name: string,
	age: int,
	sayHi: (number, number) => number
	-- ,__call: (number, number) => number
}

let a5: Person = {}

local function add(a: number, b: number)
    return a + b
end

let a6: (number, number) => number = add
a5.sayHi = a6

let a7: number = tonumber(a2)
let a8: table = a5
-- let a9: number = a5(1, 2)
let a10: string = tostring(a5)

-- let a11 = a5(1)