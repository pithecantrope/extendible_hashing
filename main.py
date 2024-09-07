from re import compile

WORD = compile(r"\w+'?\w*")

with open("res/shakespeare.txt") as input, open("res/data.txt", "w") as output:
    output.writelines(list(map(lambda s: f"{s.lower()}\n", WORD.findall(input.read()))))
    
