print("hello from lua")

env2 = {["abc"]=1234}

-- _ENV['abc'] = 1234

print(ADD(1,2))

print(ADD(3,4))


for i = 1, 6 do
    print('hello, for ' .. i)
end

local contract_demo1 = import_contract 'demo2' -- or import_contract('demo1')

contract_demo1.init()
contract_demo1.start()

print('test2 demo done')

local i = 1

::demo_goto::
if i < 5 then
	print("goto line ", i)
	i = i + 1
	-- goto demo_goto
end
