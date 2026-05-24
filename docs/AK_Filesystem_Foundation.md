# AK Engine v2.3 — Filesystem Foundation

## Цель

Filesystem Foundation фиксирует базовый контракт ввода-вывода до появления asset database, cooker, shader cache, package builder и hot reload. Движок не должен расти на случайных `std::ofstream`/`std::filesystem` вызовах, разбросанных по системам.

## Какие лимиты снимает

### 1. Битые файлы после crash/power loss

Писать сцену или кэш напрямую поверх целевого файла опасно. Правило AK Engine:

```text
write target.tmp
flush/close
replace target
```

Для текстовых файлов используется `WriteTextFileAtomic`, для бинарных — `WriteBinaryFileAtomic`.

### 2. Хаос путей

Пути должны нормализоваться до единого вида перед GUID, кэшем, сравнением и записью в метаданные.

```text
D:\AKENGINE\assets\..\assets\cube.glb
D:/AKENGINE/assets/cube.glb
./assets/cube.glb
```

Эти варианты не должны становиться разными ассетами.

### 3. Windows MAX_PATH / Unicode boundary

Внутри движка путь считается нормализованным UTF-8-представлением, а на Win32-boundary позже будет строгая конвертация в UTF-16. `AssessPathRisk` уже помечает длинные пути и non-ASCII сегменты, чтобы такие места не проскочили незаметно.

### 4. Небезопасный traversal

Пути с `..` опасны для project sandbox, asset import и package extraction. `AssessPathRisk` помечает parent traversal, а `IsPathInsideRoot` нужен для будущей проверки, что importer не пишет за пределы проекта.

### 5. Неконтролируемое сканирование директорий

`ScanDirectoryTree` имеет `maxEntries` и флаг `truncated`. Это защищает editor/asset pipeline от случайного сканирования огромного диска или системной папки.

## Добавленные API

```cpp
ProjectLayout BuildProjectLayout(const std::filesystem::path& root);
Result<ProjectLayout> EnsureProjectLayout(const std::filesystem::path& root);

Result<std::string> ReadTextFile(const std::filesystem::path& path, u64 maxBytes);
Result<std::vector<u8>> ReadBinaryFile(const std::filesystem::path& path, u64 maxBytes);

Result<void> WriteTextFileAtomic(const std::filesystem::path& path, std::string_view text);
Result<void> WriteBinaryFileAtomic(const std::filesystem::path& path, const std::vector<u8>& bytes);

PathRiskReport AssessPathRisk(const std::filesystem::path& path);
FileFingerprint BuildFileFingerprint(const std::filesystem::path& path, bool hashContents);
DirectoryScanResult ScanDirectoryTree(const std::filesystem::path& root, const DirectoryScanOptions& options);
```

## Project layout

```text
D:\AKENGINE
  assets/
  projects/
  .akcache/
    shaders/
  logs/
  temp/
  intermediate/
```

Это база для следующих систем:

```text
asset database
shader cache
package cooker
import cache
logs/crash dumps
temporary extraction
build artifacts
```

## Правило для дальнейшей разработки

Новые системы не должны писать файлы напрямую через `std::ofstream`, если это проектные данные, сцены, ассеты, кэш или метаданные. Использовать Filesystem Foundation и возвращать `Result<T>`.
