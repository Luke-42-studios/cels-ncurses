# Testing Patterns

**Analysis Date:** 2026-02-07

## Test Framework

**Runner:**
- utest.h (sheredom/utest.h) -- single-header C test framework, vendored
- Config: vendored at `tests/vendor/utest.h` in parent CELS repo
- Tests exist only in the parent CELS framework (`/home/cachy/workspaces/libs/cels/tests/`), not in cels-ncurses itself

**Assertion Library:**
- utest.h built-in assertions: `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_GT`, `ASSERT_GE`

**Benchmark Harness:**
- Custom `bench_harness.h` using rdtsc cycle counting and `clock_gettime(CLOCK_MONOTONIC)`
- Custom `mem_track.h` for malloc/free tracking with hidden size headers
- Results output to JSON via yyjson library

**Run Commands:**
```bash
cd /home/cachy/workspaces/libs/cels/build
cmake ..                         # Configure
make test_cels                   # Build test suite
./test_cels                      # Run all tests + benchmarks
./test_cels --filter=CelsFixture # Run fixture tests only
./test_cels --filter=bench       # Run benchmarks only
ctest                            # Run all registered tests via CTest
```

## Test File Organization

**Location:**
- All tests live in the parent CELS framework repo: `/home/cachy/workspaces/libs/cels/tests/`
- cels-ncurses module has NO tests of its own (zero test files)
- The module is tested indirectly through the example app at `/home/cachy/workspaces/libs/cels/examples/app.c`

**Naming:**
- `test_cels.c` -- main test suite (utest.h framework, fixture-based)
- `test_root_composition.c` -- standalone integration test (manual printf-based, no framework)
- `test_basic.cpp`, `test_reactivity.cpp`, `test_relationships.cpp`, `test_lifecycle.cpp` -- legacy C++ tests
- `tests/dsl_design/syntax_*.c` -- DSL syntax validation/exploration tests

**Structure:**
```
/home/cachy/workspaces/libs/cels/tests/
    vendor/utest.h               # Test framework (vendored, single header)
    bench_harness.h              # Benchmark timing macros (rdtsc + clock_gettime)
    mem_track.h                  # Memory tracking wrapper
    test_cels.c                  # Primary test suite (~1885 lines)
    test_root_composition.c      # Standalone root composition test
    output/                      # JSON results (latest.json, baseline.json)
    dsl_design/                  # DSL syntax exploration tests
```

## Test Structure

**Fixture Pattern (primary, in `test_cels.c`):**
```c
/* Fixture struct holds CELS context */
struct CelsFixture {
    CELS_Context* ctx;
};

/* Setup: reset all static IDs, init fresh CELS context */
UTEST_F_SETUP(CelsFixture) {
    reset_test_ids();
    utest_fixture->ctx = cels_init();
    ASSERT_NE((void*)NULL, (void*)utest_fixture->ctx);
}

/* Teardown: shutdown CELS context */
UTEST_F_TEARDOWN(CelsFixture) {
    cels_shutdown(utest_fixture->ctx);
}

/* Test case */
UTEST_F(CelsFixture, component_register) {
    cels_entity_t id = cels_component_register("RegTestComp", 8, 4);
    ASSERT_NE((cels_entity_t)0, id);
}
```

**Standalone Test (no fixture):**
```c
UTEST(utility, version) {
    const char* v = cels_version();
    ASSERT_NE((void*)NULL, (void*)v);
    ASSERT_NE('\0', v[0]);
}
```

**Benchmark Test (standalone, own init/shutdown):**
```c
UTEST(bench, entity_create_1000) {
    reset_test_ids();
    CELS_Context* ctx = cels_init();
    ASSERT_NE((void*)NULL, (void*)ctx);

    /* Setup ... */

    BENCH_VARS();
    BENCH_START();
    /* ... code to benchmark ... */
    BENCH_STOP(&g_bench_results[g_bench_count], "entity_create_1000");
    bench_print(&g_bench_results[g_bench_count]);
    g_bench_count++;

    cels_shutdown(ctx);
}
```

**Key Patterns:**
- Every fixture test gets a fresh `cels_init()` / `cels_shutdown()` cycle
- `reset_test_ids()` clears ALL static `*ID` variables to prevent stale ECS entity references between tests
- Benchmark tests manage their own init/shutdown to isolate from fixture overhead
- Integration tests replicate app patterns headlessly (no TUI providers needed)

## ID Reset Pattern (Critical)

CELS uses static `_ensure()` functions that cache entity IDs. Between test runs (where the ECS world is destroyed and recreated), these IDs become stale. The solution:

```c
static void reset_test_ids(void) {
    TestPosID = 0;
    TestVelID = 0;
    TestCounterID = 0;
    CompTestTagID = 0;
    /* ... every static ID used in tests must be reset ... */
}
```

When adding new test components/states/lifecycles, always add their ID reset to `reset_test_ids()`.

## Mocking

**Framework:** No mocking framework. Manual mocking via:

**Patterns:**
- Static counters for call tracking: `static int g_sys_run_count = 0;`, `static int g_provider_call_count = 0;`
- DSL design tests use mock function pointers: `CELS_Context* ctx_mock_app = (CELS_Context*)999;`
- Integration tests reproduce app behavior without TUI providers (headless), avoiding the need to mock ncurses

**Headless Integration Test Pattern:**
```c
/* Mirror app.c components/state but simplified */
CEL_State(IntegMenuState, { int screen; int selected; int item_count; });
CEL_Component(IntegCanvas, { int screen; int item_count; });

/* Mirror app.c compositions */
CEL_Composition(IntegMainMenu) { ... }

/* Root composition + router (same pattern as app.c) */
CEL_RootComposition(IntegAppUI) {
    (void)_props;
    CEL_Init(IntegMenuRouter) {}
}

/* Test drives state changes + reactivity manually */
UTEST_F(CelsFixture, integration_screen_switch_to_settings) {
    integ_setup(utest_fixture->ctx);
    CEL_SetState(IntegMenuState, .screen = INTEG_SCREEN_SETTINGS, ...);
    cels_process_reactivity(utest_fixture->ctx);
    ASSERT_GE(count_integ_canvas(utest_fixture->ctx, INTEG_SCREEN_SETTINGS), 1);
}
```

**What to Mock:**
- TUI/rendering providers (test logic without ncurses terminal)
- Time-dependent behavior (use fixed delta in `Engine_Progress(0.016f)`)

**What NOT to Mock:**
- CELS runtime (`cels_init()`, `cels_shutdown()`, ECS world) -- use real instances
- Component registration, state management -- always test against real runtime

## Fixtures and Factories

**Test Data:**
```c
/* Define test-specific components inline at file scope */
CEL_Component(TestPos, { float x; float y; });
CEL_Component(SysTestComp, { int value; });

/* Define test compositions */
CEL_Composition(SimpleComp) {
    (void)props;
    CEL_Has(CompTestTag, .value = 42);
}

CEL_Composition(PropsComp, int amount;) {
    CEL_Has(CompTestTag, .value = props.amount);
}
```

**Entity Counting Helpers:**
```c
/* Pattern: iterate ECS world to count entities with specific component values */
static int count_comp_test_tags(CELS_Context* ctx, int expected_value) {
    ecs_world_t* world = cels_get_world(ctx);
    if (CompTestTagID == 0) return 0;
    int count = 0;
    ecs_iter_t it = ecs_each_id(world, CompTestTagID);
    while (ecs_each_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            CompTestTag* m = ecs_field(&it, CompTestTag, 0);
            if (m && m[i].value == expected_value) count++;
        }
    }
    return count;
}
```

**Location:**
- All test types, fixtures, and helpers defined at file scope in `tests/test_cels.c`
- No separate fixture files or test data directories
- Integration test setup via `integ_setup()` helper function

## Coverage

**Requirements:** None enforced. No coverage tooling configured.

**To add coverage (future):**
```bash
# GCC/Clang coverage flags
cmake -DCMAKE_C_FLAGS="--coverage" -DCMAKE_CXX_FLAGS="--coverage" ..
make test_cels && ./test_cels
gcov tests/test_cels.c
# Or: lcov --capture --directory . --output-file coverage.info
```

## Test Types

**Unit Tests (CelsFixture suite):**
- Component registration and deduplication
- State set/get, direct access, CEL_SetState macro
- Entity creation (named, anonymous, nested hierarchy)
- Composition instantiation (simple, with props, nested, multiple)
- Lifecycle registration, state transitions, entity binding, push/pop
- System declaration, macro registration, execution on Engine_Progress
- Relation registration, pair add/remove/has
- Feature/Provider define, declare, register, finalize
- Module define, init idempotency, CEL_UseModule macro
- Description on entity, concatenation, named entity docs
- Reactivity: observe tracking, root composition init, state change recomposition, entity reconciliation

**Integration Tests (Headless app.c reproduction):**
- `integration_menu_initial_state` -- verifies correct entities on startup
- `integration_screen_switch_to_settings` -- state change creates settings entities, removes main menu
- `integration_screen_switch_back` -- round-trip state transitions with entity reconciliation
- `integration_settings_persist_across_switches` -- state persistence across screen transitions
- `integration_menu_navigation` -- selection index management
- `integration_lifecycle_visibility` -- lifecycle active/inactive tracks screen state

**Benchmark Tests (bench suite):**
- `entity_create_1000` -- bulk entity creation timing
- `entity_create_destroy_1000` -- bulk deletion timing
- `composition_create_100` -- composition instantiation with memory tracking
- `state_propagation` -- 100 state changes with recomposition
- `lifecycle_evaluation` -- 10 lifecycle evaluations via Engine_Progress
- `component_register_100` -- component registration throughput
- `memory_entity_footprint` -- per-entity RSS delta for 10000 entities
- `recomposition_10_roots` -- 10 root compositions all recomposing simultaneously

**E2E Tests:**
- Not used. The example app (`examples/app.c`) serves as manual E2E validation.
- No automated TUI testing (no headless ncurses driver).

## JSON Results Output

Tests produce structured JSON output for CI/regression tracking:

```bash
# Results written to:
/home/cachy/workspaces/libs/cels/tests/output/latest.json    # Full test + bench results
/home/cachy/workspaces/libs/cels/tests/output/baseline.json   # Benchmark baseline for comparison
```

The custom `main()` in `test_cels.c` captures per-test results (suite, name, status, duration_ns) and benchmark results (name, cycles, wall_ns, memory_bytes). Baseline comparison flags regressions > 10%.

## Common Patterns

**Async Testing:**
- Not applicable. All tests are synchronous single-threaded C code.
- `Engine_Progress(0.016f)` simulates one frame tick synchronously.

**State Change + Reactivity Testing:**
```c
/* Pattern: Modify state, notify, process, assert */
ReactTestState.screen = 1;
cels_state_notify_change(ReactTestStateID);
cels_process_reactivity(utest_fixture->ctx);
ASSERT_GE(count_react_markers(utest_fixture->ctx, 200), 1);
ASSERT_EQ(0, count_react_markers(utest_fixture->ctx, 100));
```

**Error/Crash Testing:**
```c
/* Pattern: Verify operation doesn't crash, assert trivially */
cels_state_notify_change(TestCounterID);
ASSERT_TRUE(1); /* If we got here, no crash */
```

**ECS World Inspection:**
```c
/* Pattern: Use flecs API to verify ECS state after operations */
ecs_world_t* world = cels_get_world(utest_fixture->ctx);
ecs_entity_t e = ecs_lookup(world, "test_entity");
ASSERT_NE((ecs_entity_t)0, e);
ASSERT_TRUE(ecs_has_id(world, e, (ecs_id_t)TestPosID));
const SysTestComp* comp = ecs_get_id(world, e, SysTestCompID);
ASSERT_GE(comp->value, 1);
```

## cels-ncurses Testing Gap

The cels-ncurses module has **zero dedicated tests**. Testing is only achieved through:

1. **Integration tests in parent CELS** (`tests/test_cels.c`) that reproduce app patterns headlessly without TUI providers
2. **Manual verification** via the example app (`examples/app.c`) which requires a terminal

**To test cels-ncurses directly, consider:**
- Mocking ncurses functions (`initscr`, `getch`, `mvprintw`) to test provider logic
- Testing `TUI_Window_use()` registration without ncurses init (mock `cels_register_startup`/`cels_register_frame_loop`)
- Testing input mapping (`tui_read_input_ncurses`) by mocking `wgetch()` return values
- Unit testing color pair setup and window state transitions

## CTest Integration

```cmake
# In /home/cachy/workspaces/libs/cels/CMakeLists.txt:
enable_testing()
add_test(NAME test_basic COMMAND test_basic)
add_test(NAME test_reactivity COMMAND test_reactivity)
add_test(NAME test_relationships COMMAND test_relationships)
add_test(NAME test_lifecycle COMMAND test_lifecycle)
add_test(NAME test_cels COMMAND test_cels)
```

```bash
cd /home/cachy/workspaces/libs/cels/build && ctest --output-on-failure
```

---

*Testing analysis: 2026-02-07*
