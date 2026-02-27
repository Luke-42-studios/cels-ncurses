# Roadmap: cels-ncurses

## Milestones

- v1.0 Foundation (Phases 1-5) -- SHIPPED 2026-02-20. Window lifecycle, input, drawing primitives, panel-backed layers, frame pipeline. 39 requirements verified, archived.
- v1.1 Enhanced Rendering (Phases 6-8) -- CLOSED 2026-02-26. Mouse input, true color, sub-cell rendering. Phases 9-10 dropped for architecture rethink.
- v0.2.0 ECS Module Architecture -- planning

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

**Milestone Goal:** Restructure cels-ncurses from a monolithic Engine module into a proper CELS ECS module. One cel_module(NCurses) with components, systems, and entities. Developer configures via component data; NCurses systems react in pipeline phases.

Phases: TBD (pending requirements + roadmap)

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-5 | v1.0 | 15/15 | Complete | 2026-02-20 |
| 6-8 | v1.1 | 6/6 | Closed | 2026-02-26 |
| v0.2.0 | v0.2.0 | - | Planning | - |
