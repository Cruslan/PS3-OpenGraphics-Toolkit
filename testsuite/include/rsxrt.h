/**
 * PS3 Reality Synthesizer (RSX) - GPU Hardware Ray Tracing Engine
 * Header Definitions & Public API
 */

#ifndef __RSXRT_H__
#define __RSXRT_H__

#include <stdbool.h>
#include <stdint.h>
#include "gamepad.h"

/**
 * Initialize RSX GPU Ray Tracing shader resources and GCM contexts.
 */
bool rsxrt_init(void);

/**
 * Reset and initialize camera and ray tracing scene parameters.
 */
void rsxrt_init_scene(void);

/**
 * Check if RSX Ray Tracing resources are currently initialized.
 */
bool rsxrt_is_initialized(void);

/**
 * Main per-frame update, execution, and rendering pass on RSX GPU.
 * Returns false if the user requested exit back to the main menu.
 */
bool rsxrt_update_and_render(float delta_time, const GamepadInput* input, float fps);

/**
 * Clean up allocated RSX resources.
 */
void rsxrt_cleanup(void);

#endif /* __RSXRT_H__ */
