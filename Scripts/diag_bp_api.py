# -*- coding: utf-8 -*-
"""Диагностика: можно ли создавать графы Blueprint из Python в UE 5.7."""
import sys
import unreal

out = []
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception:
    pass


def say(msg):
    line = str(msg)
    out.append(line)
    print('[BA] ' + line, flush=True)


# 1. Классы K2Node доступны в Python?
for cls in ('K2Node_CallFunction', 'K2Node_Event', 'K2Node_IfThenElse',
            'K2Node_VariableGet', 'K2Node_VariableSet', 'EdGraph',
            'EdGraphNode', 'BlueprintEditorLibrary',
            'JsonObjectGraphFunctionLibrary'):
    say('hasattr(unreal.%s) = %s' % (cls, hasattr(unreal, cls)))

# 2. Функции создания чего-либо в Python
create_like = [n for n in dir(unreal)
               if ('create' in n.lower() or 'new_object' in n.lower()
                   or 'spawn' in n.lower() or 'instantiate' in n.lower())]
say('create-like globals: %s' % sorted(create_like))

# 3. BlueprintEditorLibrary: полный список методов
bel = getattr(unreal, 'BlueprintEditorLibrary', None)
if bel:
    say('BlueprintEditorLibrary: %s' % sorted(dir(bel)))

# 4. Попытка создать пустой Blueprint с родителем Actor
try:
    bp = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
        '/Game/RS/TestBP_Diag', unreal.Actor)
    say('create_blueprint_asset_with_parent -> %s' % bp)
    if bp:
        # добавляем переменную
        ok = unreal.BlueprintEditorLibrary.add_member_variable(
            bp, 'Hp', unreal.BlueprintEditorLibrary.get_basic_type_by_name('real'))
        say('add_member_variable Hp -> %s' % ok)
        graph = unreal.BlueprintEditorLibrary.add_function_graph(
            bp, 'TestFunc')
        say('add_function_graph -> %s' % graph)
        # есть ли в Python методы у самого графа?
        if graph:
            say('graph type: %s' % type(graph))
            say('graph dir: %s' % sorted(n for n in dir(graph)
                                         if not n.startswith('__'))[:60])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        say('compile OK')
except Exception as e:
    say('EXC: %s' % e)

# 5. Проверим, есть ли у EdGraph публичные методы add_node и т.п.
try:
    g = unreal.load_object(None, '/Game/RS/TestBP_Diag.TestBP_Diag')
    say('reload: %s' % g)
except Exception as e:
    say('reload EXC: %s' % e)

path = r'F:\UnrealProjects\RebornSecondLife\Scripts\bp_api_diag.txt'
with open(path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))
print('[BA] wrote ' + path, flush=True)
