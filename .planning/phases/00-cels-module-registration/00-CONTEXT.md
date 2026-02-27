# Phase 0: CELS Module Registration - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix the CELS framework to support `cels_register(NCurses)` for modules, remove the unused CELS_REGISTER pipeline phase, update docs, set up dual-remote workflow, and release v0.5.1 to the public 42-Galaxies/cels repo. All work happens in the cels repo — this is a prerequisite hot-patch before cels-ncurses Phase 1 can execute.

</domain>

<decisions>
## Implementation Decisions

### CEL_Module macro fix
- Add `Name_register()` as a static inline alias for `Name_init()` inside the `CEL_Module(Name)` macro
- This makes `cels_register(NCurses)` work identically to `cels_register(Position)` — uniform registration
- One-line macro addition, zero new API surface, zero new concepts for developers

### CELS_REGISTER phase removal (FULL)
- Remove `CELS_REGISTER` enum value from `cels_phase_t` in cels.h
- Remove `CelsRegister` phase entity creation from phase_registry.c
- Remove `cels_system_registry_add_register()` and `cels_system_registry_run_register()` from system_registry
- Remove `#define Register cels_phase_get_entity(CELS_REGISTER)` phase macro from cels_system_impl.h
- Remove `#define CELS_Phase_Register CELS_REGISTER` legacy shim from cels_runtime.h
- Remove `CELS_MAX_REGISTER_CALLBACKS` define and callback storage from system_registry.c
- Remove register-phase call from `_cels_session_enter()` in cels_api.c
- Remove `CELS_ERROR_LIMIT_EXCEEDED` if only used for Register callbacks (Claude checks)
- Check and clean up any Register phase tests
- Leave .planning/ doc references alone — historical context, won't be in public repo
- Note the removal in GitHub release notes only (no migration note in docs — nobody used it)

### Dual-remote workflow
- Luke-42-studios/cels = origin (dev remote, has everything: .planning/, research/, test data)
- 42-Galaxies/cels = public remote (clean repo, no dev artifacts)
- Selective push strategy — push only the code/docs that belong in the public repo
- Reconfigure local repo: rename current origin to `public`, add Luke-42-studios as new `origin`

### Release script
- Location: `scripts/release.sh` in the repo (excluded from public pushes)
- Scope: Just the public push (filtering and pushing to 42-Galaxies)
- Reusable across libs (cels, cels-ncurses, etc.) — same dual-remote pattern
- Script handles: selective file push, excluding .planning/, research/, review.md, test data, internal docs

### Documentation updates
- Update `docs/modules.md` — reflect Name_register() pattern, cels_register(Module) usage
- Update `docs/api.md` — new/removed macros (Name_register added, Register phase removed)
- Keep module examples generic (no NCurses-specific references)
- No CHANGELOG.md file — GitHub release notes only

### Release process
- Version bump in CMakeLists.txt only (that's where the version lives)
- Tag v0.5.1 on public remote (42-Galaxies) only
- GitHub release via `gh release create` with release notes
- Release notes mention: CEL_Module register fix, CELS_REGISTER removal, doc updates

### Claude's Discretion
- Exact implementation of the release script (git filter, orphan branch, subtree, etc.)
- Order of operations within the cels repo changes
- How to handle the enum value gap after CELS_REGISTER removal
- Exact wording of GitHub release notes
- Whether CELS_ERROR_LIMIT_EXCEEDED has other uses (investigate and decide)

</decisions>

<specifics>
## Specific Ideas

- "We need to make sure it's set up in a way that we can easily do these releases to public" — the release script should make pushing to public a one-command operation
- "This is a hot-patch to the public repo so we can use this for NCurses" — urgency: cels-ncurses Phase 1 is blocked on this
- The dual-remote pattern should be the standard for all libs going forward (cels, cels-ncurses, future libs)

</specifics>

<deferred>
## Deferred Ideas

- Full release automation (version bump + changelog + tag + push to both remotes) — current script is just the public push
- CI/CD pipeline for automated releases — future improvement
- Applying the same dual-remote setup to cels-ncurses — do when cels-ncurses reaches its first public release

</deferred>

---

*Phase: 00-cels-module-registration*
*Context gathered: 2026-02-27*
