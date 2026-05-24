You are working on AK Engine.

Make exactly one tiny code patch.

Hard limits:
- Do not discover the next roadmap stage.
- Do not scan the whole repository.
- Do not create probes.
- Do not create tests.
- Do not create zip archives.
- Do not create or edit CMake targets.
- Do not edit ENGINE_ROADMAP.md.
- Do not edit ENGINE_STATE.md unless required to fix an inconsistency.
- Do not edit docs unless explicitly required.
- Change at most 1 production source file and, only if necessary, its matching header.
- Prefer fixing/polishing existing code over adding new systems.
- Use existing quality gate only after the code change.
- If the task needs broader context, stop and explain what is needed instead of expanding scope.

Patch goal:
Pick one very small improvement in the editor/runtime code that is obvious from the currently relevant files.

Result format:
- changed files
- what changed
- checks run