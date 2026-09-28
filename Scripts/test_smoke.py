# -*- coding: utf-8 -*-
"""Смоук-тест: 1 FBX со свежим FbxImportUI в /Game/RS/Characters + поиск файлов."""
import os
import unreal

SRC = 'F:/UnrealProjects/AssetsSource/RPGCharacterPack/FBX/Cleric.fbx'
DEST = '/Game/RS/Characters'
TPL = r'F:\UnrealProjects\UE_5.7\Templates'
PROJ = r'F:\UnrealProjects\RebornSecondLife\Content'


def log(m):
    print('[S] ' + str(m), flush=True)


def find(root, name, limit=5):
    hits = []
    for dp, dn, fn in os.walk(root):
        if name in fn:
            hits.append(os.path.join(dp, name))
            if len(hits) >= limit:
                break
    return hits


log('/Game/RS/Characters BEFORE: %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/RS/Characters'))
log('/Game/Characters BEFORE: %s (список: %r)'
    % (unreal.EditorAssetLibrary.does_directory_exist('/Game/Characters'),
       unreal.EditorAssetLibrary.list_assets('/Game/Characters', True, False)))
log('old /Game/Characters/RPG/Cleric exists: %s'
    % unreal.EditorAssetLibrary.does_asset_exist('/Game/Characters/RPG/Cleric.Cleric'))

# СВЕЖИЙ FbxImportUI на задачу — защита от краша при переиспользовании
o = unreal.FbxImportUI()
if hasattr(o, 'import_mesh'):
    o.import_mesh = True
if hasattr(o, 'import_as_skeletal'):
    o.import_as_skeletal = True
if hasattr(o, 'import_materials'):
    o.import_materials = True
if hasattr(o, 'import_textures'):
    o.import_textures = True
if hasattr(o, 'import_animations'):
    o.import_animations = True

t = unreal.AssetImportTask()
t.filename = SRC
t.destination_path = DEST
t.destination_name = ''
t.automated = True
t.save = True
t.replace_existing = True
t.options = o
log('importing Cleric.fbx -> %s ...' % DEST)
tools = unreal.AssetToolsHelpers.get_asset_tools()
tools.import_asset_tasks([t])
log('import done. dir exists: %s'
    % unreal.EditorAssetLibrary.does_directory_exist(DEST))
try:
    log('assets: %r' % unreal.EditorAssetLibrary.list_assets(DEST, True, False))
except Exception as e:
    log('list ERR %s' % e)

# где физически лежат Cleric.uasset и RlDiagTest.uasset
log('find Cleric.uasset in project: %r' % find(PROJ, 'Cleric.uasset'))
log('find Cleric.uasset in Templates: %r' % find(TPL, 'Cleric.uasset'))
log('find RlDiagTest in project: %r' % find(PROJ, 'RlDiagTest.uasset'))
log('find RlDiagTest in Templates: %r' % find(TPL, 'RlDiagTest.uasset'))
log('DONE')
