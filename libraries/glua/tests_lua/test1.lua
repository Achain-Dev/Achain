local demo2 = import_contract_from_address 'id_demo2'

function mylua()

-- printff("mylua")

print(ADD(1,2))

print(ADD(3,4))

pprint({1,2,3,11,7})


pprint('demo2 used in test1.lua', demo2.id)

local p1 = {name="thinkyoung", age=1}
p1.nested = {nested=p1, groups="groups members"}
pprint("p1", p1)

local hello = require 'hello'

hello.say_hello('thinkyoung')

print(hello.test_error())

print("after call test_error()")

-- require('tests_lua/test2.lua')


-- _ENV['abc'] = 123

env2 = {["abc"]=1234}

-- local package = require 'package'

-- print(package.loaded)

end
