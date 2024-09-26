from re import compile

WORD = compile(r"\w+'?\w*")

with open("shakespeare.txt") as input, open("data.txt", "w") as output:
    output.writelines(list(map(lambda s: f"{s.lower()}\n", WORD.findall(input.read()))))
    
