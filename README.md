# AK Engine v5.0 Bootstrap

Это первый рабочий каркас AK Engine: ядро, окно, ECS, сцены, asset registry, renderer stub, редактор, player и asset compiler.

## Распаковка

Распакуй содержимое архива прямо в:

```text
D:\AKENGINE
```

После распаковки должен существовать файл:

```text
D:\AKENGINE\CMakeLists.txt
```

## Сборка через Developer PowerShell for VS 2022

```bat
cd /d D:\AKENGINE
cmake --preset windows-vs-debug
cmake --build --preset windows-vs-debug
```

После сборки Visual Studio generator обычно кладёт исполняемые файлы сюда:

```text
D:\AKENGINE\build\windows-vs-debug\bin\Debug\ak_editor.exe
D:\AKENGINE\build\windows-vs-debug\bin\Debug\ak_player.exe
D:\AKENGINE\build\windows-vs-debug\bin\Debug\ak_assetc.exe
```

Если используешь Ninja:

```bat
cd /d D:\AKENGINE
cmake --preset windows-ninja-debug
cmake --build --preset windows-ninja-debug
```

Ninja обычно кладёт исполняемые файлы сюда:

```text
D:\AKENGINE\build\windows-ninja-debug\bin\ak_editor.exe
D:\AKENGINE\build\windows-ninja-debug\bin\ak_player.exe
D:\AKENGINE\build\windows-ninja-debug\bin\ak_assetc.exe
```

## Что уже есть

```text
ak_core       — логирование, таймер, базовые типы, версия
ak_platform   — Win32 window + fallback stub для не-Windows сборки
ak_ecs        — EntityID, TransformComponent, базовый World
ak_scene      — простое сохранение/загрузка .akscene
ak_assets     — asset registry, stable GUID по пути
ak_render     — renderer stub, место для будущего Vulkan backend
ak_policy     — build/units/coordinates/GPU sync/shader permutation guard policies
ak_physics    — collision proxy, broadphase, contacts, fixed-step foundation
ak_vehicle    — raycast suspension vehicle physics foundation
ak_destruction — CSG destruction bridge into physics, bounds, world partition, generated resources
ak_remesh     — voxel surface extraction from CSG grids for generated destruction meshes
ak_editor.exe — первое окно редактора
ak_player.exe — загрузка/создание сцены Sandbox
ak_assetc.exe — сканирование ассетов
```

## Текущий этап

Последний патч добавил:

```text
Vehicle Physics Foundation
raycast suspension wheels
engine/brake/steering input
surface-friction aware tire forces
vehicle fixed-update component shell
ak_vehicleprobe
```
