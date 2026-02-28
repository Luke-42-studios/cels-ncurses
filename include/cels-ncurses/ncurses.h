/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * CELS NCurses Module - Umbrella Header
 *
 * Single include for applications using the NCurses module:
 *
 *   #include <cels/cels.h>
 *   #include <cels-ncurses/ncurses.h>
 *
 * Provides:
 *   - NCurses module registration (cels_register(NCurses))
 *   - Component types (NCurses_WindowConfig, NCurses_WindowState)
 *   - Composition (NCursesWindow call macro)
 *   - Drawing primitives (tui_draw_rect, tui_draw_text, etc.)
 *   - Color/style system
 *   - Scissor/clipping regions
 *   - Sub-cell rendering
 *   - Layer management
 *   - Frame pipeline
 *
 * Does NOT re-export <cels/cels.h> -- include that separately.
 */

#ifndef CELS_NCURSES_H
#define CELS_NCURSES_H

/* Public component types, module declaration, compositions */
#include <cels-ncurses/tui_ncurses.h>

/* Drawing API (unchanged from v1.0/v1.1) */
#include <cels-ncurses/tui_types.h>
#include <cels-ncurses/tui_color.h>
#include <cels-ncurses/tui_draw_context.h>
#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_scissor.h>
#include <cels-ncurses/tui_subcell.h>

/* Layer and frame API */
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_frame.h>

#endif /* CELS_NCURSES_H */
