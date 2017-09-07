local function f1()
	let a1: number = 1
	let b1: string = '123'
	local function f2()
		 let c1: number = a1
		 let a1: string = '123'
		 let a2: string = a1
		 let b1: number = 123
	end
	let c1: string = b1
end