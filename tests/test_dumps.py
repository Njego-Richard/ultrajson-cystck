import ujson_cystck as u
import json


data = {'firstName': 'Joe', 'lastName': 'Jackson', 'gender': 'male', 'age': 28, 'address': {'streetAddress': '101', 'city': 'San Diego', 'state': 'CA'}, 'phoneNumbers': [{'type': 'home', 'number': '7349282382'}]}


jsDumps = u.dumps(data, indent =4)
jsLoads = u.loads(jsDumps)

#print(type(data))
print(jsDumps)