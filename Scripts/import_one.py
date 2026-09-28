# -*- coding: utf-8 -*-
"""Импорт одного FBX по заданию из import_job.json.

ВАЖНО: после успешного сохранения ассетов командлет падает на Slate-ассерте
(CurrentApplication.IsValid) внутри ImportAssetTasks — это ограничение
безголового запуска. Файлы к этому моменту уже записаны на диск, поэтому
падение процесса не считается ошибкой импорта (проверяет import_runner.py).
"""
import json
import os
import time
import unreal

BASE = r'F:\UnrealProjects\RebornSecondLife\Scripts'
JOB = os.path.join(BASE, 'import_job.json')
LOGF = os.path.join(BASE, 'import_one.log')


def log(msg):
    line = '[%s] %s' % (time.strftime('%Y.%m.%d %H:%M:%S'), msg)
    with open(LOGF, 'a', encoding='utf-8') as f:
        f.write(line + '\n')
        f.flush()
    print('[IO] ' + msg, flush=True)


log('start; /Game/Characters exists = %s'
    % unreal.EditorAssetLibrary.does_directory_exist('/Game/Characters'))

with open(JOB, 'r', encoding='utf-8') as f:
    job = json.load(f)
src = job['src']
dest = job['dest']
skeletal = bool(job.get('skeletal', True))
log('job: %s -> %s skeletal=%s' % (src, dest, skeletal))

# Свежий FbxImportUI на задачу (переиспользование между задачами опасно)
o = unreal.FbxImportUI()
if hasattr(o, 'import_mesh'):
    o.import_mesh = True
if hasattr(o, 'import_as_skeletal'):
    o.import_as_skeletal = skeletal
if hasattr(o, 'import_materials'):
    o.import_materials = True
if hasattr(o, 'import_textures'):
    o.import_textures = True
if hasattr(o, 'import_animations'):
    o.import_animations = skeletal
if not skeletal and hasattr(o, 'static_mesh_import_data'):
    try:
        o.static_mesh_import_data.generate_lightmap_uvs = False
    except Exception as e:
        log('skip lmuv: %s' % e)

t = unreal.AssetImportTask()
t.filename = src
t.destination_path = dest
t.destination_name = ''
t.automated = True
t.save = True
t.replace_existing = True
t.options = o

tools = unreal.AssetToolsHelpers.get_asset_tools()
log('importing...')
tools.import_asset_tasks([t])
# Обычно сюда не доходим: падение на ассерте Slate после сохранения.
log('import returned OK')
log('DONE')
