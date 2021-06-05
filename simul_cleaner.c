#include <linux/input.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define SIMUL_DELAY 50e6 // 50ms - Same as KarabinerElements' default
#define err_exit(msg)                                                          \
    {                                                                          \
        perror(msg);                                                           \
        exit(EXIT_FAILURE);                                                    \
    }

// TODO
// can we send keys without `msc_scan`?
// msc_scan is supposed to be the scancode and we're sending the same each time
// which should definitely be wrong because sometimes we want to send j and
// sometimes k. So is it really doing anything I wonder?
// RESULT: msc_scan does not seem necessary!

// Assumption: repeat events (`event.value == 2`) are handled by
// the individual program/process, not evdev? Becase the target key
// will repeat if the source keys are held down, but this code doesnt
// really handle that situtation explicitly

// Track state regarding our target key
// 0: base state, target key has been written yet
// 1: target key down event written
// 2: target key up event written (important to ignore our second source key up)

// Goal of this program:
// Press two source keys 'simultaneously' to produce target key
// ('similatenously' here means within a small threshold, 50ms by default)
// This is done by, among other things, delaying writes of simul-keys with
// timers

// clang-format off
const struct input_event
syn             = {.type = EV_SYN , .code = SYN_REPORT  , .value = 0},
source_one_down = {.type = EV_KEY , .code = KEY_J       , .value = 1},
source_one_up   = {.type = EV_KEY , .code = KEY_J       , .value = 0},
source_two_down = {.type = EV_KEY , .code = KEY_K       , .value = 1},
source_two_up   = {.type = EV_KEY , .code = KEY_K       , .value = 0},
target_down     = {.type = EV_KEY , .code = KEY_ESC     , .value = 1},
target_up       = {.type = EV_KEY , .code = KEY_ESC     , .value = 0};

#define SOURCE_ONE_CODE source_one_down.code
#define SOURCE_TWO_CODE source_two_down.code
#define TARGET_CODE target_down.code
// clang-format on

////////////////////////////////////////////////////////////////////////////////
// INPUT EVENT UTILS
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// TIMER UTILS
////////////////////////////////////////////////////////////////////////////////
void timer_thread_handler_source_one(union sigval sv) {
    write_key_event(&source_one_down, 1);
}

void timer_thread_handler_source_two(union sigval sv) {
    write_key_event(&source_two_down, 1);
}

void timer_disarm(timer_t tid, struct itimerspec *its) {
    its->it_value.tv_sec = 0;
    its->it_value.tv_nsec = 0;
    its->it_interval.tv_sec = 0;  // no repeat
    its->it_interval.tv_nsec = 0; // no repeat
    // Set timer
    int set_timer_rt = timer_settime(tid, 0, its, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

void timer_arm(timer_t tid, struct itimerspec *its) {
    its->it_value.tv_sec = 0;
    its->it_value.tv_nsec = SIMUL_DELAY;
    its->it_interval.tv_sec = 0;  // no repeat
    its->it_interval.tv_nsec = 0; // no repeat
    // Set timer
    int set_timer_rt = timer_settime(tid, 0, its, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

int timer_is_armed(timer_t tid) {
    // Get timer status (remaining time)
    struct itimerspec curr_its;
    if (timer_gettime(tid, &curr_its) == -1)
        err_exit("Failed on timer_gettime");
    return (curr_its.it_value.tv_sec != 0 || curr_its.it_value.tv_nsec != 0);
}

void timer_set_up(timer_t *tidp, void (*handler)(union sigval)) {
    union sigval sv;
    struct sigevent sev = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = sv,
        .sigev_notify_function = handler,
    };
    if (timer_create(CLOCK_REALTIME, &sev, tidp) != 0)
        err_exit("Failed on timer_create");
}

////////////////////////////////////////////////////////////////////////////

// Define your source and target keys:
static const unsigned int SOURCE_KEYS[] = {KEY_J, KEY_K};
static const unsigned int TARGET_KEYS[] = {KEY_ESC};

// Some other constants:
static const struct itimerspec ITS_ON = {
    .it_value.tv_sec = 0,
    .it_value.tv_nsec = SIMUL_DELAY,
    .it_interval.tv_sec = 0,  // no repeat
    .it_interval.tv_nsec = 0, // no repeat
};
static const struct itimerspec ITS_OFF = {
    .it_value.tv_sec = 0,
    .it_value.tv_nsec = 0,
    .it_interval.tv_sec = 0,  // no repeat
    .it_interval.tv_nsec = 0, // no repeat
};

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////////////////////////
int main(void) {
    // Do not buffer, we want evdev events to be handled in
    // real-time without delays (I guess we could set the buffer to be smaller
    // then `struct input_event`, which would force it to be flushed as well?)
    setbuf(stdin, NULL), setbuf(stdout, NULL);
    struct input_event event;

    timer_t timerid_source_one;
    timer_t timerid_source_two;
    timer_set_up(&timerid_source_one, &timer_thread_handler_source_one);
    timer_set_up(&timerid_source_two, &timer_thread_handler_source_two);
    struct itimerspec its;
    enum {
        TGT_INIT,
        TGT_PRESSED_WRITTEN,
        TGT_RELEASED_WRITTEN
    } targets_state = TGT_INIT;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // // Set up stuff
    // struct SimulKeys {
    //     // Source
    //     const struct SourceKey *source_keys;
    //     // Target
    //     const unsigned int *target_keys;
    //     enum targets_state target_state;
    // };

    // struct SourceKey {
    //     const unsigned int key_code;
    //     const timer_t timer_id;
    // };

    // struct SimulKeys simul_keys = {.source_keys = source_keys, };

    // Initialize timers for each source key
    size_t num_source_keys = sizeof(SOURCE_KEYS) / sizeof(unsigned int);
    const timer_t timer_ids[num_source_keys];
    for (size_t i = 0; i < num_source_keys; i++) {
        // timer_ids[i] = timer_t timerid_source_one;

        // XXX how do we create a thread handler function
        // and then pass it to 'timer_set_up' as a reference !!??

        void timer_thread_handler_source_one(union sigval sv) {
            const struct input_event source_one_down = {
                .type = EV_KEY, .code = SOURCE_KEYS[i], .value = 1};
            write_key_event(&source_one_down, 1);
        }

        timer_set_up(&timer_ids[i], &timer_thread_handler_source_one);
    }

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    while (fread(&event, sizeof(event), 1, stdin) == 1) {

        if (event.type == EV_KEY) {
            // Catch non-source-key down while source-key is down
            // (i.e. while timer still going)
            if (event.value == KEY_PRESSED && event.code != SOURCE_ONE_CODE &&
                event.code != SOURCE_TWO_CODE) {
                if (timer_is_armed(timerid_source_one)) {
                    timer_disarm(timerid_source_one, &its);
                    write_key_event(&source_one_down, 1);
                } else if (timer_is_armed(timerid_source_two)) {
                    timer_disarm(timerid_source_two, &its);
                    write_key_event(&source_two_down, 1);
                }
            } else if (event.code == SOURCE_ONE_CODE) {
                if (event.value == KEY_PRESSED) {
                    if (timer_is_armed(timerid_source_two)) {
                        timer_disarm(timerid_source_two, &its);
                        write_key_event(&target_down, 0);
                        targets_state = TGT_PRESSED_WRITTEN;
                        continue;
                    } else {
                        timer_arm(timerid_source_one, &its);
                        continue;
                    }
                } else if (event.value == KEY_RELEASED) {
                    if (targets_state == TGT_RELEASED_WRITTEN) {
                        // timer shouldn't be armed anymore in this state
                        // so we don't check them
                        targets_state = TGT_INIT;
                        continue;
                    } else if (targets_state == TGT_PRESSED_WRITTEN) {
                        targets_state = TGT_RELEASED_WRITTEN;
                        write_key_event(&target_up, 0);
                        continue;
                    } else {
                        timer_disarm(timerid_source_one, &its);
                        write_key_event(&source_one_down, 1);
                    }
                }
            } else if (event.code == SOURCE_TWO_CODE) {
                if (event.value == KEY_PRESSED) {
                    if (timer_is_armed(timerid_source_one)) {
                        timer_disarm(timerid_source_one, &its);
                        write_key_event(&target_down, 0);
                        targets_state = TGT_PRESSED_WRITTEN;
                        continue;
                    } else {
                        timer_arm(timerid_source_two, &its);
                        continue;
                    }
                } else if (event.value == KEY_RELEASED) {
                    if (targets_state == TGT_RELEASED_WRITTEN) {
                        targets_state = TGT_INIT;
                        continue;
                    } else if (targets_state == TGT_PRESSED_WRITTEN) {
                        targets_state = TGT_RELEASED_WRITTEN;
                        write_key_event(&target_up, 0);
                        continue;
                    } else {
                        timer_disarm(timerid_source_two, &its);
                        write_key_event(&source_two_down, 1);
                    }
                }
            }
        }

        // Write the potentially modified event
        fwrite(&event, sizeof(event), 1, stdout);
    }
}

// J+k to ESC
// esc, j and k (down and up) in evtest:
// Event: time 1614473865.823027, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 70029 Event: time 1614473865.823027, type 1 (EV_KEY), code 1 (KEY_ESC), value
// 1 Event: time 1614473865.823027, -------------- SYN_REPORT ------------
// Event: time 1614473865.886967, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 70029 Event: time 1614473865.886967, type 1 (EV_KEY), code 1 (KEY_ESC), value
// 0 Event: time 1614473865.886967, -------------- SYN_REPORT ------------
// Event: time 1614473872.223033, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1614473872.223033, type 1 (EV_KEY), code 36 (KEY_J), value
// 1 Event: time 1614473872.223033, -------------- SYN_REPORT ------------
// Event: time 1614473872.295044, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1614473872.295044, type 1 (EV_KEY), code 36 (KEY_J), value
// 0 Event: time 1614473872.295044, -------------- SYN_REPORT ------------
// Event: time 1614473873.343037, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000e Event: time 1614473873.343037, type 1 (EV_KEY), code 37 (KEY_K), value
// 1 Event: time 1614473873.343037, -------------- SYN_REPORT ------------
// Event: time 1614473873.390970, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000e Event: time 1614473873.390970, type 1 (EV_KEY), code 37 (KEY_K), value
// 0 Event: time 1614473873.390970, -------------- SYN_REPORT ------------

// Capslock to Hyper
// Hyper (shift+ctrl+alt+super) in evtest (down and up):
// Event: time 1614473686.134751, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 700e0 Event: time 1614473686.134751, type 1 (EV_KEY), code 29 (KEY_LEFTCTRL),
// value 1 Event: time 1614473686.134751, -------------- SYN_REPORT ------------
// Event: time 1614473686.166745, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 700e1 Event: time 1614473686.166745, type 1 (EV_KEY), code 42
// (KEY_LEFTSHIFT), value 1 Event: time 1614473686.166745, --------------
// SYN_REPORT ------------ Event: time 1614473686.198746, type 4 (EV_MSC), code
// 4 (MSC_SCAN), value 700e2 Event: time 1614473686.198746, type 1 (EV_KEY),
// code 56 (KEY_LEFTALT), value 1 Event: time 1614473686.198746, --------------
// SYN_REPORT ------------ Event: time 1614473686.230650, type 4 (EV_MSC), code
// 4 (MSC_SCAN), value 700e3 Event: time 1614473686.230650, type 1 (EV_KEY),
// code 125 (KEY_LEFTMETA), value 1 Event: time 1614473686.230650,
// -------------- SYN_REPORT ------------ Event: time 1614473686.342747, type 4
// (EV_MSC), code 4 (MSC_SCAN), value 700e0 Event: time 1614473686.342747, type
// 1 (EV_KEY), code 29 (KEY_LEFTCTRL), value 0 Event: time 1614473686.342747,
// -------------- SYN_REPORT ------------ Event: time 1614473686.350674, type 4
// (EV_MSC), code 4 (MSC_SCAN), value 700e1 Event: time 1614473686.350674, type
// 1 (EV_KEY), code 42 (KEY_LEFTSHIFT), value 0 Event: time 1614473686.350674,
// -------------- SYN_REPORT ------------ Event: time 1614473686.358668, type 4
// (EV_MSC), code 4 (MSC_SCAN), value 700e2 Event: time 1614473686.358668, type
// 1 (EV_KEY), code 56 (KEY_LEFTALT), value 0 Event: time 1614473686.358668,
// -------------- SYN_REPORT ------------ Event: time 1614473686.374668, type 4
// (EV_MSC), code 4 (MSC_SCAN), value 700e3 Event: time 1614473686.374668, type
// 1 (EV_KEY), code 125 (KEY_LEFTMETA), value 0 Event: time 1614473686.374668,
// -------------- SYN_REPORT ------------

// Emulate evtest out on j_down+j_up:
// Event: time 1613304998.635310, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1613304998.635310, type 1 (EV_KEY), code 36 (KEY_J), value
// 1 Event: time 1613304998.635310, -------------- SYN_REPORT ------------
// Event: time 1613304998.683267, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1613304998.683267, type 1 (EV_KEY), code 36 (KEY_J), value
// 0 Event: time 1613304998.683267, -------------- SYN_REPORT ------------

// J held down event:
// Event: time 1613316096.604709, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1613316096.604709, type 1 (EV_KEY), code 36 (KEY_J), value
// 1 Event: time 1613316096.604709, -------------- SYN_REPORT ------------
// Event: time 1613316096.875064, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316096.875064, -------------- SYN_REPORT ------------
// Event: time 1613316096.911761, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316096.911761, -------------- SYN_REPORT ------------
// Event: time 1613316096.948409, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316096.948409, -------------- SYN_REPORT ------------
// Event: time 1613316096.985077, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316096.985077, -------------- SYN_REPORT ------------
// Event: time 1613316097.021720, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316097.021720, -------------- SYN_REPORT ------------
// Event: time 1613316097.058389, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316097.058389, -------------- SYN_REPORT ------------
// Event: time 1613316097.095076, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316097.095076, -------------- SYN_REPORT ------------
// Event: time 1613316097.131705, type 1 (EV_KEY), code 36 (KEY_J), value 2
// Event: time 1613316097.131705, -------------- SYN_REPORT ------------
// Event: time 1613316097.131705, type 4 (EV_MSC), code 4 (MSC_SCAN), value
// 7000d Event: time 1613316097.131705, type 1 (EV_KEY), code 36 (KEY_J), value
// 0 Event: time 1613316097.131705, -------------- SYN_REPORT ------------
