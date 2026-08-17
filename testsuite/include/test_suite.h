/**
 * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Full Coverage Test Suite
 * Header Definition and API Specification
 *
 * Manages loading, GPU microcode binding, uniform parameter updates,
 * navigation state machine, and on-screen diagnostics across all 62 test scenes.
 */

#ifndef __TEST_SUITE_H__
#define __TEST_SUITE_H__

#include <stdint.h>
#include <stdbool.h>
#include "common.h"
#include "gamepad.h"
#include "test_suite_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all shader programs, load microcodes into RSX GDDR3 VRAM,
 * and prepare constant parameter handles.
 *
 * @return true on successful initialization, false on memory/allocation error.
 */
bool test_suite_init(void);

/**
 * Update test suite state machine, process gamepad input (D-Pad navigation),
 * and dispatch current active shader or stub scene to the RSX command buffer.
 *
 * @param delta_time Elapsed frame time in seconds.
 * @param input Current polled DualShock 3 gamepad input structure.
 * @param fps Current measured engine frame rate.
 * @return true to continue execution loop, false to request exit to XMB.
 */
bool test_suite_update_and_render(float delta_time, const GamepadInput* input, float fps);

/**
 * Switch directly to a specific test scene index (1-based, 1 to 62).
 *
 * @param test_index Target test index (1 <= test_index <= 62).
 */
void test_suite_select_test(int test_index);

/**
 * Retrieve the current active test scene index (1 to 62).
 *
 * @return Current 1-based test index.
 */
int test_suite_get_current_test(void);

/**
 * Free allocated RSX microcode memory and reset test suite state.
 */
void test_suite_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEST_SUITE_H__ */
