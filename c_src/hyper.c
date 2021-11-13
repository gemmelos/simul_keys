#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define err_exit(msg)                                                          \
    {                                                                          \
        perror(msg);                                                           \
        exit(EXIT_FAILURE);                                                    \
    }

// clang-format off
const struct input_event
syn             = {.type = EV_SYN , .code = SYN_REPORT  , .value = 0},
// source_one_down = {.type = EV_KEY , .code = KEY_J       , .value = 1},
// source_one_up   = {.type = EV_KEY , .code = KEY_J       , .value = 0},
// source_two_down = {.type = EV_KEY , .code = KEY_K       , .value = 1},
// source_two_up   = {.type = EV_KEY , .code = KEY_K       , .value = 0},
// target_down     = {.type = EV_KEY , .code = KEY_ESC     , .value = 1},
// target_up       = {.type = EV_KEY , .code = KEY_ESC     , .value = 0},
// target_down     = {.type = EV_KEY , .code = KEY_ESC     , .value = 1},
// target_down     = {.type = EV_KEY , .code = KEY_ESC     , .value = 1},
caps_down           = {.type = EV_KEY, .code = KEY_CAPSLOCK, .value = 1},
caps_up             = {.type = EV_KEY, .code = KEY_CAPSLOCK, .value = 0},
left_ctrl_down      = {.type = EV_KEY, .code = KEY_LEFTCTRL, .value = 1},
left_ctrl_up        = {.type = EV_KEY, .code = KEY_LEFTCTRL, .value = 0},
left_shift_down     = {.type = EV_KEY, .code = KEY_LEFTSHIFT, .value = 1},
left_shift_up       = {.type = EV_KEY, .code = KEY_LEFTSHIFT, .value = 0},
left_alt_down       = {.type = EV_KEY, .code = KEY_LEFTALT, .value = 1},
left_alt_up         = {.type = EV_KEY, .code = KEY_LEFTALT, .value = 0},
left_meta_down      = {.type = EV_KEY, .code = KEY_LEFTMETA, .value = 1},
left_meta_up        = {.type = EV_KEY, .code = KEY_LEFTMETA, .value = 0};
// clang-format on

void write_event(const struct input_event *event) {
    if (fwrite(event, sizeof(struct input_event), 1, stdout) != 1)
        err_exit("Failed on write_event");
}

void write_key_event(const struct input_event *iep, char syn_sleep) {
    write_event(iep);
    write_event(&syn);
    // If we write another event after `iep` we need to sleep first
    // for syncing of events to work (I think?)
    if (syn_sleep)
        usleep(20000); // 0.2 ms
}

int main(void) {
    setbuf(stdin, NULL), setbuf(stdout, NULL);

    struct input_event event;
    while (fread(&event, sizeof(event), 1, stdin) == 1) {

        // KEY_CAPSLOCK
        // to
        // KEY_LEFTCTRL
        // KEY_LEFTSHIFT
        // KEY_LEFTALT
        // KEY_LEFTMETA // super

        if (event.type == EV_KEY && event.code == KEY_CAPSLOCK) {
            if (event.value == KEY_PRESSED) {
                write_key_event(&left_ctrl_down, 1);
                write_key_event(&left_shift_down, 1);
                write_key_event(&left_alt_down, 1);
                write_key_event(&left_meta_down, 1);
            } else if (event.value == KEY_RELEASED) {
                write_key_event(&left_ctrl_up, 1);
                write_key_event(&left_shift_up, 1);
                write_key_event(&left_alt_up, 1);
                write_key_event(&left_meta_up, 1);
            }
        } else {
            fwrite(&event, sizeof(event), 1, stdout);
        }
    }
}
