#!/usr/bin/env python3
"""Build a resident ARM core and dependency-safe external actor modules.

This produces development modules, not an executable app. Unimplemented core
imports remain explicit; no zero-return stubs are generated.
"""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from pack_module import pack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/modules'
sys.path.insert(0, str(ROOT))
from build_core import build_inputs


def run(args):
    return subprocess.check_output(list(map(str, args)), text=True)


def symbols(path, undefined=False):
    text = run(['arm-none-eabi-nm', '-P', '-g', '-u' if undefined else '--defined-only', path])
    return {line.split()[0] for line in text.splitlines() if line.split()}


def reachable(graph, start):
    seen, todo = set(), list(start)
    while todo:
        node = todo.pop()
        if node in seen:
            continue
        seen.add(node)
        todo.extend(graph[node] - seen)
    return seen


def components(graph):
    # Deterministic Tarjan SCCs: mutually dependent code must unload together.
    index, low, stack, active, groups = {}, {}, [], set(), []

    def visit(node):
        index[node] = low[node] = len(index)
        stack.append(node)
        active.add(node)
        for nxt in sorted(graph[node]):
            if nxt not in index:
                visit(nxt)
                low[node] = min(low[node], low[nxt])
            elif nxt in active:
                low[node] = min(low[node], index[nxt])
        if low[node] == index[node]:
            group = []
            while True:
                nxt = stack.pop()
                active.remove(nxt)
                group.append(nxt)
                if nxt == node:
                    break
            groups.append(sorted(group))

    for node in sorted(graph):
        if node not in index:
            visit(node)
    return sorted(groups)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    compiled = json.loads((ROOT/'build/core/compile-report.json').read_text())
    if not compiled or not all(item['ok'] for item in compiled):
        raise ValueError('a successful complete native build is required')
    inputs_file = ROOT/'build/core/build-inputs.json'
    if not inputs_file.exists() or json.loads(inputs_file.read_text()) != build_inputs():
        raise ValueError('stale headers or build flags; rebuild core first')
    flags = [f.replace('@ROOT@', str(ROOT)) for f in
             json.loads((ROOT/'platform/compile_flags.json').read_text())]
    replacements = {}
    for source, relative in [
        (ROOT/'platform/nano_actor_db.c', 'platform/nano_actor_db.c'),
        (ROOT/'Shipwright/soh/src/code/z_actor.c', 'src/code/z_actor.c'),
        (ROOT/'platform/nano_module.c', 'platform/nano_module.c'),
        (ROOT/'platform/nano_modules.c', 'platform/nano_modules.c'),
        (ROOT/'platform/nano_module_pages.c', 'platform/nano_module_pages.c'),
        (ROOT/'platform/nano_actor_loader.c', 'platform/nano_actor_loader.c')]:
        obj = OUT/(source.stem + '.o')
        own_flags = flags
        if 'platform/' in relative:
            own_flags = [f for f in flags if not f.startswith('-Wno-error=')]
            own_flags += ['-Wall', '-Wextra', '-Werror']
        subprocess.run(['arm-none-eabi-gcc', *own_flags, '-DSHIP_NANO_OVERLAYS',
                        '-c', str(source), '-o', str(obj)], check=True,
                       stdout=subprocess.DEVNULL, stderr=(OUT/(source.stem+'.log')).open('w'))
        replacements[relative] = obj
    objects = defaultdict(list)
    sources = {}
    for item in compiled:
        source = item['source']
        path = ROOT/source if source.startswith('platform/') else ROOT/'Shipwright/soh'/source
        if source not in replacements and hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('stale object; rebuild core first: ' + source)
        group = source.split('/')[3] if source.startswith('src/overlays/actors/') else 'core'
        obj = replacements.get(source, ROOT/item['object'])
        objects[group].append(obj)
        sources[str(obj.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    objects['core'].append(replacements['platform/nano_module.c'])
    objects['core'].append(replacements['platform/nano_modules.c'])
    objects['core'].append(replacements['platform/nano_module_pages.c'])
    objects['core'].append(replacements['platform/nano_actor_loader.c'])
    definitions, undefined, owners = {}, {}, {}
    for group, paths in sorted(objects.items()):
        obj = OUT/(group+'.rel.o')
        subprocess.run(['arm-none-eabi-ld', '-r', '-o', str(obj), *map(str, paths)], check=True)
        definitions[group], undefined[group] = symbols(obj), symbols(obj, True)
        for name in definitions[group]:
            if name in owners:
                raise ValueError('duplicate global symbol: ' + name)
            owners[name] = group
    graph = {g: {owners[n] for n in undefined[g] if n in owners and owners[n] != g}
             for g in objects}
    resident = reachable(graph, ['core'])
    external_graph = {g: graph[g] - resident for g in graph if g not in resident}
    merged = {'resident': sorted(resident)}
    for i, group in enumerate(components(external_graph)):
        merged[f'actor-{i:03d}'] = group
    module_for = {g: m for m, group in merged.items() for g in group}
    reports, global_hashes = {}, {}
    for name, members in merged.items():
        obj = OUT/(name+'.rel.o')
        subprocess.run(['arm-none-eabi-ld', '-r', '-o', str(obj),
                        *[str(OUT/(g+'.rel.o')) for g in members]], check=True)
        binary, info = pack(obj, OUT/(name+'.nsm'))
        # A NanoApps prefix-only filesystem can read these bounded files into
        # one reusable 64 KiB workspace and satisfy the loader's range reads.
        pages = OUT/name
        pages.mkdir(exist_ok=True)
        for i in range(0, len(binary), 65536):
            (pages/f'{i//65536:06x}.nmp').write_bytes(binary[i:i+65536])
        deps = sorted({module_for[d] for g in members for d in graph[g]} - {name})
        missing = sorted({n for g in members for n in undefined[g] if n not in owners})
        for symbol, entry in info['exports'].items():
            previous = global_hashes.setdefault(entry['hash'], symbol)
            if previous != symbol:
                raise ValueError('cross-module symbol hash collision')
        reports[name] = dict(info, members=members, dependencies=deps,
                             missing_services=missing,
                             sha256=hashlib.sha256(binary).hexdigest())
    deps = {name: set(r['dependencies']) for name, r in reports.items()}
    for name, info in reports.items():
        closure = sorted(reachable(deps, [name]) - {'resident'})
        info['external_closure'] = closure
        info['closure_memory_bytes'] = sum(reports[n]['memory_bytes'] for n in closure)
    # Map actor identifiers to the real ARM InitVars export, never to a stub.
    actors = {}
    table = (ROOT/'Shipwright/soh/include/tables/actor_table.h').read_text()
    for line in table.splitlines():
        match = re.match(r'/\* (0x[0-9A-Fa-f]+) \*/ DEFINE_ACTOR(?:_INTERNAL)?\((\w+),', line)
        if match:
            ident, actor = match.groups()
            symbol = actor + '_InitVars'
            owner = module_for[owners[symbol]]
            actors[int(ident, 16)] = {'name': actor, 'module': owner, 'symbol': symbol,
                                    **reports[owner]['exports'][symbol]}
    external = [r for n, r in reports.items() if n != 'resident']
    summary = {'resident_memory_bytes': reports['resident']['memory_bytes'],
               'external_module_count': len(external),
               'external_all_memory_bytes': sum(r['memory_bytes'] for r in external),
               'largest_external_closure_bytes': max((r['closure_memory_bytes'] for r in external), default=0),
               'resident_actor_groups': sorted(resident - {'core'}),
               'actor_count': len(actors), 'runnable': False}
    # Small immutable catalog for the thin loader. It stays outside the
    # resident module, avoiding a checksum dependency on its own contents.
    names = ['resident'] + sorted(n for n in reports if n != 'resident')
    ids = {name: i for i, name in enumerate(names)}
    catalog = ['/* Generated from this build only. */', '#include "nano_actor_loader.h"']
    for name in names:
        deps = reports[name]['dependencies']
        if deps:
            catalog.append(f'static const uint16_t deps_{ids[name]}[] = {{'+
                           ','.join(str(ids[d]) for d in deps)+'};')
    catalog.append('const struct nano_module_desc nano_module_catalog[] = {')
    for name in names:
        info = reports[name]
        dep = f'deps_{ids[name]}' if info['dependencies'] else '0'
        catalog.append(f'{{{info["memory_bytes"]}u, {info["crc32"]}u, {dep}, {len(info["dependencies"])} }},')
    catalog.extend(['};', 'const char *const nano_module_names[] = {'])
    catalog.extend(json.dumps(name)+',' for name in names)
    catalog.extend(['};', 'const struct nano_actor_location nano_actor_catalog[] = {'])
    for ident, info in actors.items():
        catalog.append(f'[{ident}] = {{{info["offset"]}u, {ids[info["module"]]}, 1}},')
    catalog.extend(['};', f'const unsigned nano_module_count = {len(names)};',
                    f'const unsigned nano_actor_catalog_count = {max(actors)+1};'])
    (OUT/'catalog.c').write_text('\n'.join(catalog)+'\n')
    subprocess.run(['arm-none-eabi-gcc', *flags, '-Wall', '-Wextra', '-Werror',
                    '-c', str(OUT/'catalog.c'), '-o', str(OUT/'catalog.o')], check=True)
    sizes = run(['arm-none-eabi-size', OUT/'catalog.o']).splitlines()[1].split()
    summary['loader_catalog_bytes'] = sum(map(int, sizes[:3]))
    for relative, obj in replacements.items():
        source = ROOT/relative if relative.startswith('platform/') else ROOT/'Shipwright/soh'/relative
        sources[str(obj.relative_to(ROOT))] = hashlib.sha256(source.read_bytes()).hexdigest()
    report = {'summary': summary, 'actors': actors, 'modules': reports, 'source_sha256': sources}
    (OUT/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(summary, indent=2))
    print('Checked ARM development modules only; external imports and device adapter are still required.')


if __name__ == '__main__':
    main()
