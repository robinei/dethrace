import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pickle, json
from sheets import sheet
masks = pickle.load(open('masks.pkl','rb'))
table = json.load(open('glow_table.json'))
order = sorted(table, key=lambda n: (table[n]['kind'], n))
fn = lambda name, x, y, i: (x, y) in masks[name]
per = 36
for k in range(0, len(order), per):
    sheet(order[k:k+per], 'review_%02d.png' % (k // per), fn)
print((len(order) + per - 1) // per)
