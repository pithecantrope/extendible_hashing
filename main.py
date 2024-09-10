from collections import Counter

with open("res/data.txt") as f:
    print(len([item for item, count in Counter(f.readlines()).items() if count >= 1024]))
