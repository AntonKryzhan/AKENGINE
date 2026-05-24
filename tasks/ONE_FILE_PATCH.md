You are working on AK Engine.

Goal:
Make exactly one small production-code patch.

Hard limits:
- Change at most ONE production source file.
- Do not edit ENGINE_STATE.md.
- Do not edit ENGINE_ROADMAP.md.
- Do not edit QUALITY_GATE.md.
- Do not edit AGENTS.md.
- Do not edit docs/.
- Do not create zip archives.
- Do not create probes.
- Do not create tests.
- Do not create tools/*probe folders.
- Do not add CMake targets.
- Do not edit CMake files.
- Do not scan the whole repository.
- Do not discover the next roadmap stage.
- Do not refactor unrelated code.

Allowed files:
- Exactly one existing .cpp, .hpp, .h, .cc, or .cxx file under engine/, editor/, runtime/, or tools/.
- If the change requires touching a second file, stop and report that the task is too broad.

Validation:
- Prefer a local reasoning check first.
- Run existing quality gate only if the change is safe and quick.
- Do not add new validation targets.

Patch choice:
Pick one very small obvious improvement in existing production/editor/runtime code.
Prefer:
- bug fix;
- small robustness improvement;
- clamp/sanitize fix;
- input edge case;
- UI behavior polish in an existing file;
- removal of a local inconsistency.

Result format:
- changed file;
- what changed;
- checks run;
- whether the one-file limit was respected.