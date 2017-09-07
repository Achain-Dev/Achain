print("Hi, this is test c module inject caes\n")

local hello = require 'hello'
local thinkyoung = require 'thinkyoung'

hello.say_hello('thinkyoung')

print(hello.test_error())

print("after call test_error()")
