# -*- coding: utf-8 -*-
"""Импорт ассетов Quaternius (RPG Character Pack) в проект RebornSecondLife.

Запуск:  UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript -script=<this file>
Импортирует: скелетных персонажей, оружие, текстуры; создаёт материалы; сохраняет всё.
Скрипт консервативный: каждая настройка проверяется через hasattr (совместимость версий).
"""
import os
import shutil
import unreal

SRC = 'F:/UnrealProjects/AssetsSource/RPGCharacterPack'
CHAR_FBX = SRC + '/FBX'
WEAP_FBX = SRC + '/FBX/Only Weapons'
TEX_DIR = SRC + '/Textures'
DEST_CHARS = '/Game/Characters/RPG'
DEST_WEAP = '/Game/Props/Weapons'
DEST_TEX = '/Game/Characters/RPG/Textures'

tools = unreal.AssetToolsHelpers.get_asset_tools()
log = unreal.log


def prepare_options(skeletal):
    o = unreal.FbxImportUI()
    props = [p for p in dir(o) if 'skel' in p.lower() or 'import' in p.lower()]
    log('[IMPORT] FbxImportUI props: %s' % props)
    if hasattr(o, 'import_mesh'):
        o.import_mesh = True
    if hasattr(o, 'import_as_skeletal'):
        o.import_as_skeletal = bool(skeletal)
        log('[IMPORT] set import_as_skeletal=%s' % skeletal)
    if hasattr(o, 'import_materials'):
        o.import_materials = True
    if hasattr(o, 'import_textures'):
        o.import_textures = True
    if hasattr(o, 'import_animations'):
        o.import_animations = True
    if hasattr(o, 'static_mesh_import_data') and skeletal is False:
        try:
            o.static_mesh_import_data.generate_lightmap_uvs = False
        except Exception as e:
            log('[IMPORT] skip lmuv: %s' % e)
    return o


def make_task(path, dest, options=None):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = ''
    t.automated = True
    t.save = True
    t.replace_existing = True
    if options is not None:
        t.options = options
    return t


def main():
    # --- 1. Копируем текстуры рядом с FBX, чтобы импортер нашёл их по имени из FBX
    for folder in (CHAR_FBX, WEAP_FBX):
        for f in os.listdir(TEX_DIR):
            if f.lower().endswith('.png'):
                dst = os.path.join(folder, f)
                if not os.path.exists(dst):
                    shutil.copy2(os.path.join(TEX_DIR, f), dst)
    log('[IMPORT] textures copied next to FBX')

    tasks = []

    # --- 2. Персонажи (скелетные меши)
    sk_opts = prepare_options(True)
    for f in sorted(os.listdir(CHAR_FBX)):
        if f.lower().endswith('.fbx'):
            tasks.append(make_task(os.path.join(CHAR_FBX, f), DEST_CHARS, sk_opts))

    # --- 3. Оружие (статические меши)
    st_opts = prepare_options(False)
    for f in sorted(os.listdir(WEAP_FBX)):
        if f.lower().endswith('.fbx'):
            tasks.append(make_task(os.path.join(WEAP_FBX, f), DEST_WEAP, st_opts))

    # --- 4. Текстуры отдельным слоем (на случай если FBX их не подтянул)
    for f in sorted(os.listdir(TEX_DIR)):
        if f.lower().endswith('.png'):
            tasks.append(make_task(os.path.join(TEX_DIR, f), DEST_TEX, None))

    log('[IMPORT] task count: %d' % len(tasks))
    tools.import_asset_tasks(tasks)

    # --- 5. Отчёт: что реально импортировалось
    for folder in (DEST_CHARS, DEST_WEAP, DEST_TEX):
        assets = unreal.EditorAssetLibrary.list_assets(folder, recursive=False, include_folder=False) \
            if unreal.EditorAssetLibrary.does_directory_exist(folder) else []
        log('[IMPORT] %s -> %d assets' % (folder, len(assets)))
        for a in assets:
            log('[IMPORT]    ' + str(a))

    ok = unreal.EditorAssetLibrary.save_directory('/Game', only_if_is_dirty=True, recursive=True)
    log('[IMPORT] save_directory ok=%s' % ok)
    log('[IMPORT] DONE')


main()
