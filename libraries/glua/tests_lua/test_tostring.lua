local a={name=1,b=print,c={a=1,b="hi"}}
b = tostring(a)
c = tojsonstring(a)
print(b)
pprint(c)