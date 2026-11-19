#!/usr/bin/env python3
"""Builds a function index from an IDA .lst/.txt listing of the game executable.

    python3 ida_index.py IDA-Output.txt out_dir

Writes:
  functions.tsv  one row per function: address, end, size, name, callers, callees, imports, strings
  strings.tsv    every string label with its text and the functions that use it

Only the listing's text is read, so it works on any IDA export with the default line prefixes.
"""
import collections, csv, re, sys

PROC = re.compile(r'^\.text:([0-9A-F]{8}) (\S+)\s+proc\s+near')
ENDP = re.compile(r'^\.text:([0-9A-F]{8}) (\S+)\s+endp')
CALL = re.compile(r'^\.text:[0-9A-F]{8}\s+call\s+(?:near ptr |ds:)?([A-Za-z_?@$][\w?@$]*)')
OFFSET = re.compile(r'offset (a[A-Z0-9][\w]*)')
STRDEF = re.compile(r"^\.(?:data|rdata):([0-9A-F]{8}) (a[A-Z0-9]\w*)\s+db '((?:[^']|'')*)'")
IMPORT = re.compile(r'^\.idata:([0-9A-F]{8}) ; .*?__stdcall ([A-Za-z_]\w*)\(|^\.idata:[0-9A-F]{8}\s+extrn (\w+):')

def main(path, out):
    funcs = {}            # name -> dict
    order = []
    strings = {}          # label -> (addr, text)
    imports = set()
    current = None
    with open(path, encoding='latin-1') as f:
        for line in f:
            if line.startswith('.text:'):
                m = PROC.match(line)
                if m:
                    current = {'addr': m.group(1), 'name': m.group(2), 'end': m.group(1), 'callees': [], 'strings': []}
                    funcs[current['name']] = current
                    order.append(current['name'])
                    continue
                m = ENDP.match(line)
                if m and current and m.group(2) == current['name']:
                    current['end'] = m.group(1)
                    current = None
                    continue
                if current is None:
                    continue
                m = CALL.search(line)
                if m:
                    current['callees'].append(m.group(1))
                for s in OFFSET.findall(line):
                    current['strings'].append(s)
            elif line.startswith('.data:') or line.startswith('.rdata:'):
                m = STRDEF.match(line)
                if m:
                    strings[m.group(2)] = (m.group(1), m.group(3).replace("''", "'"))
            elif line.startswith('.idata:'):
                m = IMPORT.match(line)
                if m:
                    imports.add(m.group(2) or m.group(3))

    callers = collections.Counter()
    users = collections.defaultdict(set)
    for name in order:
        fn = funcs[name]
        for c in set(fn['callees']):
            callers[c] += 1
        for s in set(fn['strings']):
            users[s].add(name)

    with open(f'{out}/functions.tsv', 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['address', 'end', 'size', 'name', 'callers', 'callees', 'imports', 'strings'])
        for name in order:
            fn = funcs[name]
            size = int(fn['end'], 16) - int(fn['addr'], 16)
            own = sorted({c for c in fn['callees'] if c in funcs})
            api = sorted({c.split('@')[0] for c in fn['callees'] if c not in funcs and c.split('@')[0] in imports})
            text = [strings[s][1][:60] for s in dict.fromkeys(fn['strings']) if s in strings]
            w.writerow([fn['addr'], fn['end'], size, name, callers[name], ' '.join(own), ' '.join(api), ' | '.join(text)])

    with open(f'{out}/strings.tsv', 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['address', 'label', 'text', 'used_by'])
        for label, (addr, text) in sorted(strings.items(), key=lambda kv: kv[1][0]):
            w.writerow([addr, label, text, ' '.join(sorted(users.get(label, ())))])

    print(f'{len(order)} functions, {len(strings)} strings, {len(imports)} imports')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
