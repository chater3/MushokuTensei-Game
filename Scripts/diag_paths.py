# -*- coding: utf-8 -*-
"""Диагностика: куда смонтирован /Game/, куда реально сохранятся пакеты."""
import os
import unreal


def safe(label, fn):
    try:
        v = fn()
        print('[DIAG] %s = %r' % (label, v))
        return v
    except Exception as e:
        print('[DIAG] %s ERR: %s' % (label, e))
        return None


safe('cwd', os.getcwd)
safe('project_file', lambda: unreal.Paths.get_project_file_path())
safe('project_dir', lambda: unreal.Paths.project_dir())
pcd = safe('project_content_dir', lambda: unreal.Paths.project_content_dir())
safe('project_saved_dir', lambda: unreal.Paths.project_saved_dir())
safe('engine_dir', lambda: unreal.Paths.engine_dir())

# Куда резолвятся long-пусти /Game/
safe('lpn2fn /Game/Diag/Test',
     lambda: unreal.FPackageName.long_package_name_to_filename('/Game/Diag/Test'))
safe('lpn2fn /Game/Characters/RPG/Test',
     lambda: unreal.FPackageName.long_package_name_to_filename('/Game/Characters/RPG/Test'))
safe('asset /Game/Characters/RPG/Cleric.Cleric exists',
     lambda: unreal.EditorAssetLibrary.does_asset_exist('/Game/Characters/RPG/Cleric.Cleric'))

funcs = sorted(n for n in dir(unreal.FPackageName) if not n.startswith('_'))
print('[DIAG] FPackageName funcs:', funcs)

# Пытаемся починить монтирование, если /Game/ указывает не туда
if pcd:
    root = str(pcd).replace('\\', '/')
    if not root.endswith('/'):
        root += '/'
    for mp, rp in (('/Game/', root),
                   ('/Game/Characters/', root + 'Characters/'),
                   ('/Game/Props/', root + 'Props/')):
        try:
            unreal.FPackageName.register_mount_point(mp, rp)
            print('[DIAG] registered %s -> %s' % (mp, rp))
        except Exception as e:
            print('[DIAG] register %s ERR: %s' % (mp, e))

safe('lpn2fn AFTER /Game/Diag/Test',
     lambda: unreal.FPackageName.long_package_name_to_filename('/Game/Diag/Test'))
safe('lpn2fn AFTER /Game/Characters/RPG/Test',
     lambda: unreal.FPackageName.long_package_name_to_filename('/Game/Characters/RPG/Test'))

# Настоящий тест записи: дублируем материал движка в /Game/Diag/
try:
    ok = unreal.EditorAssetLibrary.duplicate_asset(
        '/Engine/EngineMaterials/WorldGridMaterial', '/Game/Diag/RlDiagTest')
    print('[DIAG] duplicate_asset ok=%r' % (ok,))
except Exception as e:
    print('[DIAG] duplicate ERR: %s' % e)

if pcd:
    probe = os.path.join(str(pcd), 'Diag', 'RlDiagTest.uasset')
    print('[DIAG] file on disk: %s exists=%r' % (probe, os.path.exists(probe)))
print('[DIAG] DONE')
