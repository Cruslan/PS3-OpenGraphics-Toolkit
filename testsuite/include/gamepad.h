/**
 * PS3 Cell BE & PSL1GHT Experimental Tech Suite
 * Gamepad Subsystem Header
 *
 * Provides continuous normalized axis integration (-1.0f to +1.0f)
 * and single-frame discrete edge trigger detection for DualShock 3 / Sixaxis.
 */

#ifndef __GAMEPAD_H__
#define __GAMEPAD_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* Continuous normalized axes: -1.0f to +1.0f (0.0f is exact neutral) */
    float left_x;       /* Left Stick:  -1.0 (Left/Strafe Left) to +1.0 (Right/Strafe Right) */
    float left_y;       /* Left Stick:  -1.0 (Up/Forward)      to +1.0 (Down/Backward) */
    float right_x;      /* Right Stick: -1.0 (Left/Turn Left)  to +1.0 (Right/Turn Right) */
    float right_y;      /* Right Stick: -1.0 (Up/Look Up)      to +1.0 (Down/Look Down) */
    
    /* Button Hold States (true as long as button is held down) */
    bool btn_up;
    bool btn_down;
    bool btn_left;
    bool btn_right;
    bool btn_cross;
    bool btn_circle;
    bool btn_triangle;
    bool btn_square;
    bool btn_l1;
    bool btn_r1;
    bool btn_l2;
    bool btn_r2;
    bool btn_start;
    bool btn_select;
    bool btn_l3;
    bool btn_r3;
    
    /* Single-Frame Edge Triggers (true ONLY on initial frame pressed) */
    bool pressed_up;
    bool pressed_down;
    bool pressed_left;
    bool pressed_right;
    bool pressed_cross;
    bool pressed_circle;
    bool pressed_triangle;
    bool pressed_square;
    bool pressed_l1;
    bool pressed_r1;
    bool pressed_l2;
    bool pressed_r2;
    bool pressed_start;
    bool pressed_select;
    
    /* Analog Stick Single-Frame Directional Triggers (for UI Navigation) */
    bool stick_up_pressed;
    bool stick_down_pressed;
    bool stick_left_pressed;
    bool stick_right_pressed;
    
    bool is_connected;
} GamepadInput;

void gamepad_init(void);
void gamepad_poll(void);
const GamepadInput* gamepad_get_state(void);

#endif /* __GAMEPAD_H__ */
