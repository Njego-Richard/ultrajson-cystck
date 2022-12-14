import ujson_cystck as u


with open("Cystck.json") as f:
    data = u.load(f)

print( type(data))