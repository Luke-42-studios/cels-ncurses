# Phase 0: CELS Module Registration - Research

**Researched:** 2026-02-27
**Domain:** C macro framework (cels), git workflow, release management
**Confidence:** HIGH

## Summary

This phase involves four distinct domains of work in the cels repo (`/home/cachy/workspaces/libs/cels`):

1. **CEL_Module macro fix** -- Adding a `Name_register()` alias so modules work with `cels_register()`. Currently `CEL_Module(Name)` only generates `Name_init()`, while `cels_register(Name)` expands to `Name_register()`. This is a one-line addition to the macro in `cels.h`.

2. **CELS_REGISTER phase removal** -- Fully removing the unused Register pipeline phase. The removal touches 7 files across the codebase. No tests or examples use `.phase = Register`, so removal is safe. `CELS_ERROR_LIMIT_EXCEEDED` should be KEPT because it's a general-purpose error code in the enum (only its single usage site in register callback overflow goes away).

3. **Dual-remote workflow** -- Reconfiguring the cels repo from single origin (42-Galaxies) to dual-remote: Luke-42-studios = origin (dev), 42-Galaxies = public. This requires a `scripts/release.sh` that creates a filtered push to the public remote, excluding `.planning/`, `research/`, `review.md`, and other dev artifacts.

4. **Release v0.5.1** -- Version bump in CMakeLists.txt (currently 0.5.0), tag and release on 42-Galaxies/cels.

**Primary recommendation:** Execute changes in this order: remote reconfiguration first, then code changes (macro fix + phase removal), then docs, then version bump, then release. This ensures the dev remote captures all work history.

## Standard Stack

This phase does not introduce new libraries. The relevant tools are:

### Core (already in use)
| Tool | Version | Purpose | Notes |
|------|---------|---------|-------|
| C99 macros | N/A | CEL_Module macro lives in `include/cels/cels.h` | Macro-only change |
| CMake | 3.21+ | Build system, version source of truth | `project(cels VERSION X.Y.Z)` |
| gh CLI | installed | GitHub release creation | `gh release create` |
| git | installed | Dual-remote management | Both remotes verified accessible |

### Verified Remote Access
| Remote | URL | Status |
|--------|-----|--------|
| 42-Galaxies/cels | `https://github.com/42-Galaxies/cels.git` | EXISTS - currently `origin` |
| Luke-42-studios/cels | `https://github.com/Luke-42-studios/cels` | EXISTS - not yet configured as remote |

## Architecture Patterns

### Current CEL_Module Macro (lines 606-616 of cels.h)

```c
#define CEL_Module(Name)                                                       \
    cels_entity_t Name = 0;                                                    \
    static void Name##_init_body(void);                                        \
    void Name##_init(void) {                                                   \
        if (Name != 0)                                                         \
            return;                                                            \
        Name = cels_module_register(#Name);                                    \
        Name##_init_body();                                                    \
        cels_module_init_done();                                               \
    }                                                                          \
    static void Name##_init_body(void)
```

**Problem:** `cels_register(Name)` expands via `_CELS_REG_1(Name)` to `Name_register()` (defined in `include/cels/private/cels_macros.h` line 133). But `CEL_Module` only generates `Name_init()`, not `Name_register()`. Components and systems both generate `Name_register()`. Modules are the odd one out.

### Fix Pattern (from CONTEXT.md -- locked decision)

Add a `static inline void Name_register(void)` alias that calls `Name_init()`:

```c
#define CEL_Module(Name)                                                       \
    cels_entity_t Name = 0;                                                    \
    static void Name##_init_body(void);                                        \
    void Name##_init(void) {                                                   \
        if (Name != 0)                                                         \
            return;                                                            \
        Name = cels_module_register(#Name);                                    \
        Name##_init_body();                                                    \
        cels_module_init_done();                                               \
    }                                                                          \
    static inline void Name##_register(void) { Name##_init(); }               \
    static void Name##_init_body(void)
```

**Why `static inline`:** Matches `CEL_Component`'s pattern (line 448: `static inline void Name##_register(void)`). Safe for multi-TU inclusion because `static inline` avoids multiple definition errors. The idempotency guard inside `Name_init()` (`if (Name != 0) return;`) prevents double-initialization.

### Comparison with Other _register Patterns

| Macro | Generates | Pattern |
|-------|-----------|---------|
| `CEL_Component(Name)` | `static inline void Name_register(void)` -- no-op (registration deferred to cel_has/cel_query) | line 448 |
| `CEL_System(Name, ...)` | `void Name_register(void)` -- full registration with query terms | line 158 |
| `CEL_Module(Name)` (current) | `void Name_init(void)` -- NO `Name_register` | BROKEN |
| `CEL_Module(Name)` (fixed) | `void Name_init(void)` + `static inline void Name_register(void)` -- alias | FIX |
| `cel_phase(Name, ...)` | `static inline void Name_register(void)` | line 544 |
| `cel_on_error(Name)` | Generates `Name_register(void)` | verified in cels.h |

### CELS_REGISTER Phase Removal -- Exact Locations

All locations verified by reading actual source code:

| File | What to Remove | Line(s) | Notes |
|------|---------------|---------|-------|
| `include/cels/cels.h` | `CELS_REGISTER,` enum value | 143 | Shift CELS_PHASE_COUNT down |
| `include/cels/private/cels_system_impl.h` | `#define Register cels_phase_get_entity(CELS_REGISTER)` | 584 | Phase shorthand alias |
| `include/cels/private/cels_runtime.h` | `#define CELS_Phase_Register CELS_REGISTER` | 378 | Legacy shim |
| `src/managers/phase_registry.c` | `reg->phase_entities[CELS_REGISTER] = create_phase(w, "CelsRegister", 0);` | 97 | Phase entity creation |
| `src/managers/system_registry.h` | `cels_system_registry_add_register()` and `cels_system_registry_run_register()` declarations | 118-127 | Function declarations |
| `src/managers/system_registry.c` | `#define CELS_MAX_REGISTER_CALLBACKS 32`, callback storage fields in struct, `add_register()` and `run_register()` implementations | 54, 62-64, 291-315 | Implementation + struct fields |
| `src/cels_api.c` | `cels_system_registry_run_register(cels_context->systems);` in `_cels_session_enter()` | 452 | Session enter logic |

### Enum Gap Strategy

When `CELS_REGISTER` (value 7) is removed:

```c
// BEFORE:
typedef enum cels_phase_t {
    CELS_ON_LOAD = 0,
    CELS_LIFECYCLE_EVAL,   // 1
    CELS_ON_UPDATE,        // 2
    CELS_STATE_SYNC,       // 3
    CELS_RECOMPOSE,        // 4
    CELS_PRE_RENDER,       // 5
    CELS_ON_RENDER,        // 6
    CELS_POST_RENDER,      // 7
    CELS_REGISTER,         // 8
    CELS_PHASE_COUNT       // 9
} cels_phase_t;

// AFTER:
typedef enum cels_phase_t {
    CELS_ON_LOAD = 0,
    CELS_LIFECYCLE_EVAL,   // 1
    CELS_ON_UPDATE,        // 2
    CELS_STATE_SYNC,       // 3
    CELS_RECOMPOSE,        // 4
    CELS_PRE_RENDER,       // 5
    CELS_ON_RENDER,        // 6
    CELS_POST_RENDER,      // 7
    CELS_PHASE_COUNT       // 8
} cels_phase_t;
```

No gap -- `CELS_REGISTER` was last before `CELS_PHASE_COUNT`, so removing it just shifts `CELS_PHASE_COUNT` down from 9 to 8. The `phase_entities[]` array in `phase_registry.c` is sized by `CELS_PHASE_COUNT`, so it automatically shrinks. No special handling needed.

### CELS_ERROR_LIMIT_EXCEEDED Assessment

**KEEP.** The error code is used in two places:
1. `system_registry.c` line 301 -- Register callback overflow (REMOVING -- only usage site that goes away)
2. `cels_api.c` line 406 -- `cels_error_code_to_string()` mapping (KEEP -- generic utility)
3. `test_error_handling.c` line 199 -- Tests the string mapping (KEEP -- validates the code exists)

The error code is general-purpose ("LIMIT_EXCEEDED") and could be used by future subsystems. The enum value should remain. Only the Register-specific usage in `system_registry.c` is removed.

### Dual-Remote Strategy

**Recommended approach: `git-filter-repo` in a temporary clone.**

The challenge: the cels repo has `.planning/`, `research/`, `review.md` committed in history. These should not appear on the public remote. Options:

| Approach | Pros | Cons |
|----------|------|------|
| **git-filter-repo temp clone** | Clean history, reusable, scriptable | Requires git-filter-repo installed, force-pushes to public |
| **Orphan branch + cherry-pick** | Simple concept | Manual, error-prone, loses commit association |
| **Separate worktree with .gitignore** | No history rewriting | Does not actually prevent committed files from pushing |

**Recommendation:** Use `git-filter-repo` in a temporary clone. The release script:

1. Clone the dev repo to a temp directory
2. Run `git-filter-repo` to remove excluded paths
3. Add public remote
4. Force-push to public

This is the standard pattern for maintaining public/private repo splits. It rewrites history once per release, which is acceptable for a v0.5.x library.

**Alternative if git-filter-repo is unavailable:** Use `git archive` to create a tarball excluding paths, then commit to an orphan branch and push. Less elegant but no extra dependencies.

```bash
# Check if git-filter-repo is available
git filter-repo --version 2>/dev/null
```

### Release Script Structure

```bash
#!/usr/bin/env bash
# scripts/release.sh -- Push filtered release to public remote
#
# Usage: ./scripts/release.sh [--dry-run]
#
# Excludes: .planning/ research/ review.md scripts/ REVIEW_FEEDBACK.md
# Pushes to: public remote (42-Galaxies)

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
VERSION="$(grep 'project.*VERSION' "$REPO_ROOT/CMakeLists.txt" | grep -oP '\d+\.\d+\.\d+')"
TAG="v${VERSION}"
PUBLIC_REMOTE="public"
EXCLUDE_PATHS=(".planning" "research" "review.md" "REVIEW_FEEDBACK.md" "scripts")

# ... implementation using temp clone + filter-repo or archive approach
```

### Remote Reconfiguration Steps

Current state:
```
origin -> https://github.com/42-Galaxies/cels.git (public)
```

Target state:
```
origin -> https://github.com/Luke-42-studios/cels.git (dev)
public -> https://github.com/42-Galaxies/cels.git (public)
```

Commands:
```bash
cd /home/cachy/workspaces/libs/cels
git remote rename origin public
git remote add origin https://github.com/Luke-42-studios/cels.git
git push -u origin main          # push all history to dev remote
git push origin --tags            # push existing tags
```

### Version Bump Locations

Only ONE place needs updating:
- `CMakeLists.txt` line 18: `project(cels VERSION 0.5.0 LANGUAGES C)` -> `project(cels VERSION 0.5.1 LANGUAGES C)`

The `cels_version.h` is auto-generated from `cels_version.h.in` by CMake's `configure_file()`. Committing the generated `cels_version.h` with the updated values is recommended so that non-CMake consumers (plain Makefile) get the right version.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Filtering git history for public push | Custom git scripts with `git rm` | `git-filter-repo` (or `git archive` fallback) | Edge cases with submodules, binary files, nested paths |
| GitHub release creation | curl + GitHub API | `gh release create v0.5.1 --title "..." --notes "..."` | Authentication, asset upload, error handling |
| Version extraction from CMakeLists.txt | Manual parsing | `grep 'project.*VERSION' CMakeLists.txt \| grep -oP '\d+\.\d+\.\d+'` | Standard pattern, handles whitespace variations |

## Common Pitfalls

### Pitfall 1: Static Inline in Multi-TU Module Headers
**What goes wrong:** If `Name_register()` were `void` (not `static inline`), including a module's header in two TUs would cause a linker "multiple definition" error.
**Why it happens:** `CEL_Module(Name)` expands at file scope in headers.
**How to avoid:** Use `static inline void Name_register(void)` -- exactly as `CEL_Component` does.
**Warning signs:** Linker errors about duplicate symbols.

### Pitfall 2: Breaking the phase_entities Array Indexing
**What goes wrong:** If `CELS_REGISTER` is removed but the array in `phase_registry.c` still has a slot for it, you get an off-by-one or uninitialized phase lookup.
**Why it happens:** The array `phase_entities[CELS_PHASE_COUNT]` is indexed by enum values. Removing an enum value shifts all subsequent values.
**How to avoid:** `CELS_REGISTER` is the LAST user-visible value before `CELS_PHASE_COUNT`. Removing it just decrements `CELS_PHASE_COUNT`. Remove the `reg->phase_entities[CELS_REGISTER]` assignment line in `phase_registry.c` and you're done.
**Warning signs:** Segfault or wrong-phase execution at startup.

### Pitfall 3: Forgetting the v2 System Creation Register Detection
**What goes wrong:** `cels_system_registry_declare_v2()` (system_registry.c line 242) checks if a phase is the Register phase entity. After removal, the Register phase entity no longer exists, so this check becomes dead code -- but it references `CELS_REGISTER` indirectly through `phase_registry_get_phase`. Since the enum value is gone, this code path disappears naturally.
**How to avoid:** Verify `cels_system_registry_declare_v2` does NOT directly reference `CELS_REGISTER`. Confirmed: it checks protected phases (LifecycleEval, StateSync, Recompose) -- NOT Register. No change needed.

### Pitfall 4: Force-Pushing to Public Remote Accidentally
**What goes wrong:** Running `git push --force public main` with unfiltered history sends `.planning/` etc. to the public repo.
**Why it happens:** The release script must filter before pushing.
**How to avoid:** The release script should NEVER do a direct push. Always clone-filter-push.
**Warning signs:** `.planning/` directory visible on 42-Galaxies/cels GitHub page.

### Pitfall 5: Stale cels_version.h After Version Bump
**What goes wrong:** `cels_version.h` still says "0.5.0" even after CMakeLists.txt says "0.5.1" because CMake wasn't re-run.
**Why it happens:** `configure_file()` only runs during CMake configure step.
**How to avoid:** After bumping CMakeLists.txt, run `cmake --build build-test` (or just reconfigure), then commit the regenerated `cels_version.h`.

### Pitfall 6: Existing Test Regression from Phase Enum Renumbering
**What goes wrong:** Tests that use phase enum values by number (not name) break.
**How to avoid:** Grep for numeric phase values in tests. Confirmed: no test uses raw numeric phase values -- they all use symbolic names (`CELS_ON_LOAD`, `OnUpdate`, etc.). Safe.

## Code Examples

### CEL_Module Macro Fix (exact change)

```c
// Source: include/cels/cels.h, lines 606-616
// ADD one line before the final `static void Name##_init_body(void)`

#define CEL_Module(Name)                                                       \
    cels_entity_t Name = 0;                                                    \
    static void Name##_init_body(void);                                        \
    void Name##_init(void) {                                                   \
        if (Name != 0)                                                         \
            return;                                                            \
        Name = cels_module_register(#Name);                                    \
        Name##_init_body();                                                    \
        cels_module_init_done();                                               \
    }                                                                          \
    static inline void Name##_register(void) { Name##_init(); }               \
    static void Name##_init_body(void)
```

### Verification: cels_register(NCurses) Expansion

After the fix, `cels_register(NCurses)` expands as:
```
cels_register(NCurses)
  -> _CEL_CAT(_CELS_REG_, _CEL_SYSTEM_ARG_COUNT(NCurses))(NCurses)
  -> _CELS_REG_1(NCurses)
  -> NCurses_register()
  -> NCurses_init()      // static inline alias
  -> cels_module_register("NCurses") + init body + cels_module_init_done()
```

### Phase Enum After Removal

```c
// Source: include/cels/cels.h, line 134
typedef enum cels_phase_t {
    CELS_ON_LOAD = 0,
    CELS_LIFECYCLE_EVAL,
    CELS_ON_UPDATE,
    CELS_STATE_SYNC,
    CELS_RECOMPOSE,
    CELS_PRE_RENDER,
    CELS_ON_RENDER,
    CELS_POST_RENDER,
    CELS_PHASE_COUNT
} cels_phase_t;
```

### _cels_session_enter After Removal

```c
// Source: src/cels_api.c, line 447
void _cels_session_enter(void) {
    if (cels_context == NULL)
        return;

    /* Register phase removed -- no callbacks to run */
}
```

### system_registry struct After Removal

```c
// Source: src/managers/system_registry.c
// REMOVE: #define CELS_MAX_REGISTER_CALLBACKS 32

struct cels_system_registry {
    cels_world_manager* world;
    cels_memory_manager* memory;
    cels_phase_registry* phases;
    SysCtxPtrArray contexts;
    // REMOVED: register_callbacks array
    // REMOVED: register_callback_count
};
```

### gh Release Command

```bash
gh release create v0.5.1 \
  --repo 42-Galaxies/cels \
  --title "v0.5.1" \
  --notes "$(cat <<'NOTES'
## Changes

### Fixed
- `CEL_Module(Name)` now generates `Name_register()` alias, making `cels_register(Module)` work uniformly for modules, components, systems, and phases

### Removed
- `CELS_REGISTER` pipeline phase (unused) -- systems should use OnLoad, OnUpdate, PreRender, OnRender, or PostRender
- `Register` phase shorthand macro
- `CELS_Phase_Register` legacy shim
- Register-phase callback storage in system registry

### Documentation
- Updated `docs/modules.md` for the new `Name_register()` pattern
- Updated `docs/api.md` to reflect phase removal
NOTES
)"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Name_init()` for modules | `Name_register()` alias (v0.5.1) | This phase | Uniform registration API |
| 8-phase pipeline with Register | 7-phase pipeline without Register | This phase | Cleaner pipeline, no unused phase |
| Single remote (42-Galaxies) | Dual-remote (dev + public) | This phase | Dev artifacts stay private |

## Open Questions

1. **git-filter-repo availability**
   - What we know: It's the recommended tool for history rewriting
   - What's unclear: Whether it's installed on this system
   - Recommendation: Check `git filter-repo --version`. If unavailable, use `git archive` + orphan branch approach instead. Claude has discretion here per CONTEXT.md.

2. **Existing public repo state**
   - What we know: 42-Galaxies/cels exists with v0.5.0 tag and commit history
   - What's unclear: Whether the public repo already has `.planning/` or `research/` in its history (it might already be clean if those were added after the last public push)
   - Recommendation: Check the public repo's file listing via `gh api repos/42-Galaxies/cels/contents` before writing the release script. If already clean, a simple filtered push from a temp clone is sufficient.

3. **Phase registry comment update**
   - What we know: `phase_registry.c` line 21 says "8-phase pipeline" and `phase_registry.h` line 22 says "8-phase pipeline"
   - What's unclear: Whether there are other doc comments referencing "8 phases"
   - Recommendation: Search for "8-phase" and "8 phases" across the codebase and update to "7-phase" / "7 phases". Minor but prevents confusion.

## Sources

### Primary (HIGH confidence)
- **cels source code** -- Direct reading of all files listed in Architecture Patterns section
  - `include/cels/cels.h` -- CEL_Module macro, cels_register dispatch, phase enum, error codes
  - `include/cels/private/cels_macros.h` -- _CELS_REG_N expansion macros
  - `include/cels/private/cels_system_impl.h` -- Register phase shorthand, system creation
  - `include/cels/private/cels_runtime.h` -- Legacy shims, _cels_session_enter declaration
  - `src/managers/system_registry.c` -- Register callback storage and execution
  - `src/managers/phase_registry.c` -- Phase entity creation including CelsRegister
  - `src/cels_api.c` -- _cels_session_enter implementation, error_code_to_string
  - `tests/test_modules.c` -- Module test suite (no Register phase tests found)
  - `CMakeLists.txt` -- Version 0.5.0, test targets
- **GitHub API (gh CLI)** -- Verified both remotes exist and are accessible

### Secondary (MEDIUM confidence)
- [git-filter-repo](https://github.com/newren/git-filter-repo) -- Recommended by Git project for history rewriting
- [Git push documentation](https://git-scm.com/docs/git-push) -- Refspec and remote management

### Tertiary (LOW confidence)
- Release script implementation details -- multiple approaches possible, exact implementation is Claude's discretion per CONTEXT.md

## Metadata

**Confidence breakdown:**
- CEL_Module macro fix: HIGH -- Read actual source, verified expansion chain, confirmed pattern matches CEL_Component
- CELS_REGISTER removal: HIGH -- Read every file containing CELS_REGISTER, confirmed no tests use .phase = Register
- CELS_ERROR_LIMIT_EXCEEDED: HIGH -- Grep confirmed only 1 usage site in system_registry.c (removing), enum value and string mapping are generic (keeping)
- Dual-remote workflow: MEDIUM -- Git operations are standard, but release script implementation has multiple valid approaches
- Version/release: HIGH -- CMakeLists.txt is the single version source, gh CLI is available and authenticated

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable codebase, no external dependency changes expected)
