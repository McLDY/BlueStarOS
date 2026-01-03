#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "idt.h"

void keyboard_init();
void keyboard_callback(interrupt_frame_t *frame);

#endif // KEYBOARD_H