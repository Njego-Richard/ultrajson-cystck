import ujson_cystck as u


data = {'firstName': 'Joe', 'lastName': 'Jackson', 'gender': 'male', 'age': 28, 'address': {'streetAddress': '101', 'city': 'San Diego', 'state': 'CA'}, 'phoneNumbers': [{'type': 'home', 'number': '7349282382'}]}
with open("Cystck.json",'w') as f:
    u.dump(data,f)

