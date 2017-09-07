function f1(a: number, b: string)
	if a > 100 then
		return a
	else
		return b
	end
end

let a = f1(100, 'abc')
let b: number = f1(100, 'abc')