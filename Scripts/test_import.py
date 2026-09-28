# -*- coding: utf-8 -*-
"""Тест импорта v2: где реально лежат файлы /Game/*."""
import os
import shutil
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
pcd = str(unreal.Paths.project_content_dir())
TPL = r'F:\UnrealProjects\UE_5.7\Templates'
SRC_TEX = 'F:/UnrealProjects/AssetsSource/RPGCharacterPack/Textures'
UNIQ = 'F:/UnrealProjects/AssetsSource/RlTestTex.png'


def log(msg):
    print('[T] ' + str(msg), flush=True)


def find(root, name):
    hits = []
    for dp, dn, fn in os.walk(root):
        if name in fn:
            hits.append(os.path.join(dp, name))
    return hits


log('project_content_dir=%s' % pcd)
log('/Game/Characters dir BEFORE: %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/Characters'))
log('/Game/TestImport dir BEFORE: %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/TestImport'))
log('/Game/Props dir BEFORE: %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/Props'))

# тест-материал в /Game/Diag (проверка базового /Game/)
unreal.EditorAssetLibrary.duplicate_asset(
    '/Engine/EngineMaterials/WorldGridMaterial', '/Game/Diag/RlDiagTest')
log('save /Game/Diag -> %s' % unreal.EditorAssetLibrary.save_asset('/Game/Diag/RlDiagTest'))

# уникальная текстура для двух тестовых импортов
pngs = sorted(f for f in os.listdir(SRC_TEX) if f.lower().endswith('.png'))
shutil.copy2(os.path.join(SRC_TEX, pngs[0]), UNIQ)
log('test png: %s (from %s)' % (UNIQ, pngs[0]))

for dest in ('/Game/TestImport', '/Game/Characters/RPGTest'):
    t = unreal.AssetImportTask()
    t.filename = UNIQ
    t.destination_path = dest
    t.automated = True
    t.save = True
    t.replace_existing = True
    tools.import_asset_tasks([t])
    log('import -> %s | dir exists: %s'
        % (dest, unreal.EditorAssetLibrary.does_directory_exist(dest)))

log('/Game/Characters dir AFTER: %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/Characters'))
for folder in ('/Game/TestImport', '/Game/Characters/RPGTest'):
    try:
        log('assets in %s: %r'
            % (folder, unreal.EditorAssetLibrary.list_assets(folder, True, False)))
    except Exception as e:
        log('list %s ERR %s' % (folder, e))

for name in ('RlDiagTest.uasset', 'RlTestTex.uasset'):
    log('find %s in project: %r' % (name, find(pcd, name)))
    log('find %s in Templates: %r' % (name, find(TPL, name)))
log('DONE')
