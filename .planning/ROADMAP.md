# Roadmap: cels-ncurses

## Milestones

- v1.0 Foundation (Phases 1-5) -- SHIPPED 2026-02-20. Window lifecycle, input, drawing primitives, panel-backed layers, frame pipeline. 39 requirements verified, archived.
- v1.1 Enhanced Rendering (Phases 6-8) -- CLOSED 2026-02-26. Mouse input, true color, sub-cell rendering. Phases 9-10 dropped for architecture rethink.
- v0.2.0 ECS Module Architecture (Phases 1-6) -- in progress

## Phases

<details>
<summary>v1.0 Foundation (Phases 1-5) - SHIPPED 2026-02-20</summary>

Archived. See git history for v1.0 phase details.
- Phase 1: Foundation Types (3 plans, complete)
- Phase 2: Drawing Primitives (4 plans, complete)
- Phase 3: Layer System (3 plans, complete)
- Phase 4: Frame Pipeline (3 plans, complete)
- Phase 5: Module Boundary (2 plans, complete)

</details>

<details>
<summary>v1.1 Enhanced Rendering (Phases 6-8) - CLOSED 2026-02-26</summary>

Closed early. Phases 9-10 dropped for v2.0 architecture rethink.
- Phase 6: Mouse Input (2 plans, complete)
- Phase 7: True Color (2 plans, complete)
- Phase 8: Sub-Cell Rendering (3 plans, complete)

</details>

### v0.2.0 ECS Module Architecture

**Milestone Goal:** Restructure cels-ncurses from a monolithic Engine module into a proper CELS ECS module. One `CEL_Module(NCurses)` that registers components, systems, and entities. Developer configures by setting component data on entities; NCurses systems react through ECS queries in pipeline phases.

- [x] **Phase 0: CELS Module Registration** - Fix CEL_Module macro, remove CELS_REGISTER phase, update docs, set up dual-remote, release v0.5.1 (work in cels repo) ✓ 2026-02-27
- [x] **Phase 1: Module Boundary** - Replace Engine + CelsNcurses with a single CEL_Module(NCurses) + working window lifecycle (absorbs Phase 2) ✓ 2026-02-28
- [x] **Phase 1.1: CELS API Purge** - Replace all direct flecs/ecs API calls with CELS abstractions (INSERTED) ✓ 2026-02-28
- [x] **Phase 1.2: Window Lifecycle Rewrite** - Replace bridge + flecs observers with CEL_Lifecycle + CEL_Observe pattern (INSERTED) ✓ 2026-03-01
- [ ] **Phase 2: Window Entity** - (Absorbed into Phase 1)
- [x] **Phase 3: Input System** - Per-frame input reading as a CELS phase system with queryable state
- [ ] **Phase 4: Layer Entities** - Panel-backed layers created from TUI_Layer components with TUI_DrawContext attachment
- [ ] **Phase 5: Frame Pipeline** - Begin/end frame as CELS pipeline phase systems orchestrating layers
- [ ] **Phase 6: Demo** - Validate the full entity-driven API with a button + box example

## Phase Details

### Phase 0: CELS Module Registration
**Goal**: Fix CELS framework for module registration, remove unused CELS_REGISTER phase, update docs, set up dual-remote workflow (Luke-42-studios = dev, 42-Galaxies = public), and release v0.5.1
**Depends on**: Nothing (prerequisite for Phase 1)
**Repo**: cels (not cels-ncurses)
**Success Criteria** (what must be TRUE):
  1. `CEL_Module(Name)` generates both `Name_init()` and `Name_register()` (register calls init)
  2. `cels_register(NCurses)` works identically to `cels_register(Position)` — uniform registration
  3. The `CELS_REGISTER` enum value, `Register` phase macro, callback storage, and `_cels_session_enter` register-phase logic are fully removed
  4. `docs/modules.md` and `docs/api.md` updated for the macro fix and phase removal
  5. Dual-remote configured: Luke-42-studios/cels = origin (dev), 42-Galaxies/cels = public
  6. `scripts/release.sh` exists and handles selective push to public remote (reusable across libs)
  7. v0.5.1 tagged and released on 42-Galaxies/cels via `gh release create`
  8. All existing tests pass, review.md and research/ excluded from public
**Plans:** 3 plans
Plans:
- [x] 00-01-PLAN.md -- Dual-remote setup + release script
- [x] 00-02-PLAN.md -- CEL_Module macro fix + CELS_REGISTER phase removal
- [x] 00-03-PLAN.md -- Doc updates, version bump, release to public

### Phase 1: Module Boundary
**Goal**: Developer imports a single NCurses module that owns all terminal components and systems -- no more Engine facade or sub-module registration. Absorbs Phase 2 (Window Entity) -- module skeleton AND working window lifecycle delivered together.
**Depends on**: Phase 0 (CELS Module Registration)
**Requirements**: MOD-01, MOD-02, MOD-03, WIN-01, WIN-02, WIN-03
**Success Criteria** (what must be TRUE):
  1. A single `CEL_Module(NCurses)` exists and can be imported by a consumer application
  2. The old Engine module and CelsNcurses module are removed -- no `TUI_Engine_use()` or `Engine_Progress()` calls remain
  3. Developer creates entities with NCurses components to configure behavior (no config structs passed to init functions)
  4. Public API surface is entity-component based: headers expose component types, not provider registration functions
  5. Window entity with NCurses_WindowConfig triggers terminal init via observer
  6. NCurses_WindowState attached to window entity after init (width, height, running, fps, delta_time)
**Plans:** 3 plans
Plans:
- [x] 01-01-PLAN.md -- New public API headers + module skeleton + CMake update
- [x] 01-02-PLAN.md -- Refactor window/input/frame to observer and module-registered systems
- [x] 01-03-PLAN.md -- Delete legacy files, rebuild example, verify build

### Phase 1.1: CELS API Purge (INSERTED)
**Goal**: Eliminate all direct flecs/ecs API usage from cels-ncurses source files. All observer registration, system registration, component manipulation, and world access must go through CELS abstractions (CEL_System, cel_query, cels_ensure_component, cels_component_add, etc.). No `#include <flecs.h>` in any cels-ncurses source file outside of `#ifdef` guards for features CELS doesn't yet wrap.
**Depends on**: Phase 1 (Module Boundary)
**Success Criteria** (what must be TRUE):
  1. No cels-ncurses .c file includes `<flecs.h>` directly (except behind feature guards if CELS lacks an abstraction)
  2. Observer registration uses CELS abstractions or a thin wrapper -- no raw `ecs_observer_init` calls
  3. System registration uses `CEL_System()` macro -- no raw `ecs_system_init` calls
  4. Component manipulation on entities uses CELS functions -- no raw `ecs_set_id` or `ecs_field` calls
  5. World access uses `cels_get_world(cels_get_context())` pattern only where absolutely unavoidable
  6. Minimal example still builds and runs correctly after the purge
  7. draw_test standalone target still compiles
**Plans:** 2 plans
Plans:
- [x] 01.1-01-PLAN.md -- Bridge file creation + window flecs purge
- [x] 01.1-02-PLAN.md -- System purge (input + frame) + final build verification

### Phase 1.2: Window Lifecycle Rewrite (INSERTED)
**Goal**: Replace the ECS bridge file and raw flecs observers with the new CEL_Lifecycle + CEL_Observe pattern. Window init/shutdown becomes lifecycle-driven. Composition declares components, observers handle side effects, systems update per-frame state. No flecs.h in any cels-ncurses file.
**Depends on**: Phase 1.1 (CELS API Purge), CELS Phase 30.1 (Lifecycle Hooks)
**Success Criteria** (what must be TRUE):
  1. `ncurses_ecs_bridge.c` and `ncurses_ecs_bridge.h` deleted -- no bridge file exists
  2. `NCursesWindowLC` lifecycle defined with `.active` condition driving window active/suspended state
  3. `CEL_Observe(NCursesWindowLC, on_create)` calls `ncurses_terminal_init()` -- no component setting in observer
  4. `CEL_Observe(NCursesWindowLC, on_destroy)` calls `ncurses_terminal_shutdown()`
  5. `NCursesWindow` composition declares both `NCurses_WindowConfig` and `NCurses_WindowState` via `cel_has` and attaches lifecycle via `cels_lifecycle_bind_entity(NCursesWindowLC_id, ...)`
  6. Frame system updates `NCurses_WindowState` per-frame; every `cels_entity_set_component` call is paired with `cels_component_notify_change`; `cel_watch` in application composition triggers recomposition only on actual state changes (resize, quit)
  7. No `#include <flecs.h>` in any cels-ncurses source file
  8. Minimal example uses `cel_watch(win, NCurses_WindowState)` for reactive window state
  9. Both minimal and draw_test targets build successfully
**Plans:** 2 plans
Plans:
- [x] 01.2-01-PLAN.md -- Add cels_entity_set/get_component API to CELS (prerequisite)
- [x] 01.2-02-PLAN.md -- Lifecycle rewrite, bridge deletion, build verification

### Phase 2: Window Entity
**Goal**: (Absorbed into Phase 1)
**Status**: Merged with Phase 1 per CONTEXT.md decision

### Phase 3: Input System
**Goal**: Developer can read keyboard and mouse input state that NCurses fills each frame in a dedicated CELS phase
**Depends on**: Phase 1
**Requirements**: INP-01, INP-02
**Success Criteria** (what must be TRUE):
  1. An NCurses input system runs in a CELS input phase, reading getch/mouse events without developer intervention
  2. Developer can read input state (key presses, mouse position, button events) from a component after the input phase completes
  3. Input state is fresh each frame -- previous frame state does not leak into the current frame
**Plans:** 2 plans
Plans:
- [x] 03-01-PLAN.md -- NCurses_InputState component + input system entity rewrite
- [x] 03-02-PLAN.md -- Remove running pointer bridge, update example, build verification

### Phase 4: Layer Entities
**Goal**: Developer can create drawable layers by adding TUI_Layer components to entities, with NCurses managing panels internally
**Depends on**: Phase 1
**Requirements**: LAYR-01, LAYR-02, LAYR-03
**Success Criteria** (what must be TRUE):
  1. Developer creates an entity with TUI_Layer (z_order, visible, dimensions) and a panel/WINDOW is created internally by NCurses
  2. NCurses attaches a TUI_DrawContext component to each layer entity so the developer can retrieve it
  3. Developer can get TUI_DrawContext from a layer entity and draw into it using existing draw primitives (tui_draw_rect, tui_draw_text, etc.)
  4. Multiple layer entities stack correctly according to z_order
**Plans**: TBD

### Phase 5: Frame Pipeline
**Goal**: NCurses owns the frame lifecycle through CELS pipeline phases, with developer systems running naturally between begin and end frame
**Depends on**: Phase 1, Phase 4
**Requirements**: FRAM-01, FRAM-02, FRAM-03
**Success Criteria** (what must be TRUE):
  1. NCurses registers begin-frame and end-frame systems in CELS pipeline phases
  2. Begin-frame system clears layers; end-frame system composites panels and calls doupdate()
  3. Developer-defined systems that draw into layers run between begin-frame and end-frame without explicit ordering code
**Plans**: TBD

### Phase 6: Demo
**Goal**: A working example proves the full entity-driven API by rendering interactive content across multiple layers
**Depends on**: Phase 1, Phase 3, Phase 4, Phase 5
**Requirements**: DEMO-01
**Success Criteria** (what must be TRUE):
  1. Example app creates a window entity, creates layer entities at different z-orders, and renders a button and box using draw primitives
  2. The example uses only the entity-component API -- no legacy Engine_use(), no config structs, no direct ncurses calls
  3. The demo compiles and runs, showing layered content that responds to the frame pipeline
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 0 (cels repo) -> 1 -> 1.1 -> 1.2 -> 3 -> 4 -> 5 -> 6 (Phase 2 absorbed into Phase 1)

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-5 | v1.0 | 15/15 | Complete | 2026-02-20 |
| 6-8 | v1.1 | 6/6 | Closed | 2026-02-26 |
| 0. CELS Module Registration | v0.2.0 | 3/3 | Complete | 2026-02-27 |
| 1. Module Boundary | v0.2.0 | 3/3 | Complete | 2026-02-28 |
| 1.1 CELS API Purge | v0.2.0 | 2/2 | Complete | 2026-02-28 |
| 1.2 Window Lifecycle Rewrite | v0.2.0 | 2/2 | Complete | 2026-03-01 |
| 2. Window Entity | v0.2.0 | -- | Absorbed into P1 | - |
| 3. Input System | v0.2.0 | 2/2 | Complete | 2026-03-02 |
| 4. Layer Entities | v0.2.0 | 0/TBD | Not started | - |
| 5. Frame Pipeline | v0.2.0 | 0/TBD | Not started | - |
| 6. Demo | v0.2.0 | 0/TBD | Not started | - |
