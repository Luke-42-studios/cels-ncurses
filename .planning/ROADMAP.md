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

- [ ] **Phase 0: CELS Module Registration** - Fix CEL_Module macro, remove CELS_REGISTER phase, update docs, set up dual-remote, release v0.5.1 (work in cels repo)
- [ ] **Phase 1: Module Boundary** - Replace Engine + CelsNcurses with a single CEL_Module(NCurses) + working window lifecycle (absorbs Phase 2)
- [ ] **Phase 2: Window Entity** - (Absorbed into Phase 1)
- [ ] **Phase 3: Input System** - Per-frame input reading as a CELS phase system with queryable state
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
**Plans**: TBD

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
- [ ] 01-01-PLAN.md -- New public API headers + module skeleton + CMake update
- [ ] 01-02-PLAN.md -- Refactor window/input/frame to observer and module-registered systems
- [ ] 01-03-PLAN.md -- Delete legacy files, rebuild example, verify build

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
**Plans**: TBD

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
Phases execute in numeric order: 0 (cels repo) -> 1 -> 3 -> 4 -> 5 -> 6 (Phase 2 absorbed into Phase 1)

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-5 | v1.0 | 15/15 | Complete | 2026-02-20 |
| 6-8 | v1.1 | 6/6 | Closed | 2026-02-26 |
| 1. Module Boundary | v0.2.0 | 0/3 | Planned | - |
| 2. Window Entity | v0.2.0 | -- | Absorbed into P1 | - |
| 3. Input System | v0.2.0 | 0/TBD | Not started | - |
| 4. Layer Entities | v0.2.0 | 0/TBD | Not started | - |
| 5. Frame Pipeline | v0.2.0 | 0/TBD | Not started | - |
| 6. Demo | v0.2.0 | 0/TBD | Not started | - |
