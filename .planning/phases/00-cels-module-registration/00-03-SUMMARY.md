---
phase: 00-cels-module-registration
plan: 03
subsystem: docs/release
tags: [docs, version-bump, release, firebase, github-release]

# Dependency graph
requires:
  - phase: 00-01
    provides: "Dual-remote workflow and scripts/release.sh"
  - phase: 00-02
    provides: "CEL_Module macro fix and CELS_REGISTER removal"
provides:
  - "docs/modules.md updated with Name_register() pattern"
  - "docs/api.md clean of Register phase references"
  - "Version 0.5.1 in CMakeLists.txt and cels_version.h"
  - "v0.5.1 released on 42-Galaxies/cels (filtered, clean)"
  - "Docs site deployed to cels-lang.web.app with v0.5.1"
  - "Version auto-injection in build-docs.sh"
affects: [01-module-boundary]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "build-docs.sh extracts version from CMakeLists.txt and injects into docs/Doxyfile, markdown files, and README badge"

key-files:
  created: []
  modified:
    - docs/modules.md
    - docs/api.md
    - docs/index.md
    - docs/getting-started.md
    - docs/Doxyfile
    - docs/stylesheets/catppuccin.css
    - site/Doxyfile
    - site/build-docs.sh
    - mkdocs.yml
    - CMakeLists.txt
    - include/cels/cels_version.h
    - README.md

key-decisions:
  - "Version auto-injection via sed in build-docs.sh rather than mkdocs-macros-plugin"
  - "Public repo branches cleaned: only main (v0.4, gh-pages deleted)"
  - "Private repo reorganized: main merged from v0.4, v0.6.0 branch created for next work"

patterns-established:
  - "build-docs.sh reads CMakeLists.txt VERSION and seds it into Doxyfiles, markdown GIT_TAG refs, and README badge"
  - "catppuccin.css referenced via mkdocs.yml extra_css for hero/feature-grid/button styles"

# Metrics
duration: ~10min
completed: 2026-02-27
---

# Phase 0 Plan 3: Doc Updates, Version Bump, Release Summary

**Documentation updated, v0.5.1 released on 42-Galaxies/cels, docs site deployed to cels-lang.web.app with version auto-injection**

## Performance

- **Duration:** ~10 min (including checkpoint verification and fixes)
- **Tasks:** 3 (2 auto + 1 checkpoint)
- **Files modified:** 12

## Accomplishments
- Updated docs/modules.md with Name_register() clarification
- Verified docs/api.md has no stale Register phase references
- Bumped CMakeLists.txt to 0.5.1, regenerated cels_version.h
- Ran release script to push filtered code to 42-Galaxies/cels
- Created GitHub release v0.5.1 with changelog
- Fixed 7 hardcoded version references (0.5.0 → 0.5.1) across README, docs, Doxyfiles
- Added version auto-injection to build-docs.sh (reads from CMakeLists.txt)
- Fixed missing extra_css reference in mkdocs.yml for catppuccin.css (hero/button styles)
- Deployed docs to cels-lang.web.app with correct layout and version
- Cleaned up repo branches: public has only main, private has main + v0.6.0

## Task Commits

1. **Task 1: Update docs and bump version** - `f169b7b` (docs)
2. **Task 2: Run release script and create GitHub release** - (release operation, no commit)
3. **Task 3: Checkpoint -- human verification** - approved

### Post-checkpoint fixes (orchestrator)
- `80fc361` - fix: sync version references to CMakeLists.txt (0.5.1)
- `8835fe0` - fix: add custom stylesheet to mkdocs config

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Hardcoded version 0.5.0 in 7 locations**
- **Found during:** Checkpoint verification
- **Issue:** README badge, docs install snippets, getting-started link, and Doxyfiles all had hardcoded 0.5.0
- **Fix:** Updated all to 0.5.1, added auto-injection to build-docs.sh
- **Committed in:** 80fc361

**2. [Rule 1 - Bug] Missing custom stylesheet reference in mkdocs.yml**
- **Found during:** Checkpoint verification
- **Issue:** catppuccin.css with hero/button/feature-grid styles existed but wasn't in mkdocs.yml extra_css
- **Fix:** Added extra_css entry, tracked stylesheet in git
- **Committed in:** 8835fe0

**3. [Rule 3 - Blocker] Public repo default branch was v0.4**
- **Found during:** Checkpoint verification
- **Issue:** User saw dev artifacts because default branch was v0.4, not main
- **Fix:** Changed default to main, deleted v0.4 and gh-pages branches

---

**Total deviations:** 3 auto-fixed
**Impact on plan:** Extended scope to fix version sync and docs site layout. No architectural changes.

## Issues Encountered
- Firebase auth expired (user re-authenticated)

---
*Phase: 00-cels-module-registration*
*Completed: 2026-02-27*
