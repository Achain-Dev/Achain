print("hi, this is test load contract case\n")

local demo2 = import_contract 'demo2' -- TODO: load storage inited by demo2
-- print(demo2.storage.name)
-- demo2.set_storage(name, 'hi')

-- demo2.init()	-- this will error
demo2.start()
print(demo2)
-- demo2.name = "modified contract name"
print('demo2 contract name: ', demo2.name)

-- TODO: after called contract and close lua_State, save storage to thinkyoung

-- demo2.storage.rollback()