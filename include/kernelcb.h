#ifndef KERNELCB_H
#define KERNELCB_H

void on_keyboard_pressed(uint8_t scancode, uint8_t final_char);
void on_mouse_update(int32_t x_rel, int32_t y_rel, uint8_t left_button, uint8_t middle_button, uint8_t right_button);

// 参数结构体定义
#pragma pack(push, 1)
typedef struct {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
    uint64_t framebuffer_size;

    uint64_t memory_map_addr;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
} boot_params_t;
#pragma pack(pop)

#endif // KERNELCB_H