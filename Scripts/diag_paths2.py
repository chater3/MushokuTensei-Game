# -*- coding: utf-8 -*-
"""Диагностика2: куда смонтирован /Game/, где реально лежат файлы."""
import os
import unreal

LOG = r'F:\UnrealProjects\RebornSecondLife\Saved\diag_paths.txt'


def p(msg):
    msg = str(msg)
    print('[DIAG] ' + msg, flush=True)
    try:
        with open(LOG, 'a', encoding='utf-8') as f:
            f.write(msg + '\n')
    except Exception:
        pass


try:
    with open(LOG, 'w', encoding='utf-8') as f:
        f.write('')
except Exception:
    pass

p('cwd=' + os.getcwd())
for attr in ('project_dir', 'project_content_dir', 'project_saved_dir',
             'engine_dir', 'engine_content_dir', 'project_file'):
    try:
        v = getattr(unreal.Paths, attr)()
    except Exception as e:
        v = 'ERR %s' % e
    p('%s=%s' % (attr, v))

for a in ('/Game/Characters/RPG/Cleric.Cleric', '/Game/Diag/RlDiagTest'):
    try:
        p('exists %s = %r' % (a, unreal.EditorAssetLibrary.does_asset_exist(a)))
    except Exception as e:
        p('exists %s ERR %s' % (a, e))


def find(root, name, limit=10):
    hits = []
    try:
        for dp, dn, fn in os.walk(root):
            if name in fn:
                hits.append(os.path.join(dp, name))
                if len(hits) >= limit:
                    break
    except Exception as e:
        hits.append('walk ERR %s' % e)
    return hits


pcd = str(unreal.Paths.project_content_dir())
p('find Cleric.uasset in project: %r' % find(pcd, 'Cleric.uasset'))
p('find Cleric.uasset in engine Templates: %r' % find(r'F:\UnrealProjects\UE_5.7\Templates', 'Cleric.uasset'))

# Тест записи: дублируем материал движка в /Game/Diag/ и ищем файл на диске
try:
    ok = unreal.EditorAssetLibrary.duplicate_asset(
        '/Engine/EngineMaterials/WorldGridMaterial', '/Game/Diag/RlDiagTest')
    p('duplicate ok=%r' % (ok,))
except Exception as e:
    p('duplicate ERR %s' % e)

probe = os.path.join(pcd, 'Diag', 'RlDiagTest.uasset')
p('probe project: %s exists=%r' % (probe, os.path.exists(probe)))
p('find RlDiagTest in engine Templates: %r' % find(r'F:\UnrealProjects\UE_5.7\Templates', 'RlDiagTest.uasset'))
p('find RlDiagTest in project: %r' % find(pcd, 'RlDiagTest.uasset'))

p('Paths attrs: %r' % sorted(n for n in dir(unreal.Paths) if not n.startswith('_')))
p('DONE')
