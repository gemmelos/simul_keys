#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// TODO Try:
// #include <linux/input-event-codes.h>
// static const key_code SOURCE_KEYS

////////////////////////////////////////////////////////////////////////////////
// CONFIG
////////////////////////////////////////////////////////////////////////////////
// Define your source and target keys
// Currently 2 or 3 source keys are supported:
static const int SOURCE_KEYS[] = {KEY_J, KEY_K};
// Currently on single target key is supported:
static const int TARGET_KEYS[] = {KEY_ESC};
// Define threshold time
#define SIMUL_THRESHOLD 50e6  // 50ms - Same as KarabinerElements' default

////////////////////////////////////////////////////////////////////////////////
// APPLICATION CONSTANTS
////////////////////////////////////////////////////////////////////////////////
#define err_exit(msg)       \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    }
#define KEY_RELEASED 0
#define KEY_PRESSED 1
#define KEY_REPEATED 2
#define A_NON_SOURCE_KEY -1

static const struct input_event SYN_EVENT = {
    .type = EV_SYN, .code = SYN_REPORT, .value = 0};

// Some constants:
static const struct itimerspec ITS_ON = {
    .it_value.tv_sec     = 0,
    .it_value.tv_nsec    = SIMUL_THRESHOLD,
    .it_interval.tv_sec  = 0,  // no repeat
    .it_interval.tv_nsec = 0,  // no repeat
};

static const struct itimerspec ITS_OFF = {
    .it_value.tv_sec     = 0,
    .it_value.tv_nsec    = 0,
    .it_interval.tv_sec  = 0,  // no repeat
    .it_interval.tv_nsec = 0,  // no repeat
};

static const size_t NUM_SOURCE_KEYS = sizeof(SOURCE_KEYS) / sizeof(int);
static const size_t NUM_TARGET_KEYS = 1;

enum TargetState {
    TGT_INIT,
    TGT_PRESSED_WRITTEN,
    TGT_RELEASED_WRITTEN
} TARGETS_STATE = TGT_INIT;

////////////////////////////////////////////////////////////////////////////////
// INPUT EVENT UTILS
////////////////////////////////////////////////////////////////////////////////
void write_event(const struct input_event *event) {
    if (fwrite(event, sizeof(struct input_event), 1, stdout) != 1)
        err_exit("Failed on write_event");
}

void write_input_event(const struct input_event *iep, bool syn_sleep) {
    write_event(iep);
    write_event(&SYN_EVENT);
    // If we write another event after `iep` we need to sleep first
    // for syncing of events to work (I think?)
    // XXX we might be able to get rid of this?
    //      sleep not responsibility of write key event?
    if (syn_sleep)
        usleep(20000);  // 0.2 ms or 200 us
}

void write_key_event(const int key_code, char direction, bool syn_sleep) {
    const struct input_event ie = {
        .type = EV_KEY, .code = key_code, .value = direction};
    write_input_event(&ie, syn_sleep);
}

////////////////////////////////////////////////////////////////////////////////
// TIMER UTILS
////////////////////////////////////////////////////////////////////////////////
void timer_disarm(timer_t tid) {
    int set_timer_rt = timer_settime(tid, 0, &ITS_OFF, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

void timer_arm(timer_t tid) {
    int set_timer_rt = timer_settime(tid, 0, &ITS_ON, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

int is_timer_armed(timer_t tid) {
    // Get timer status (remaining time)
    struct itimerspec curr_its;
    if (timer_gettime(tid, &curr_its) == -1)
        err_exit("Failed on timer_gettime");
    return (curr_its.it_value.tv_sec != 0 || curr_its.it_value.tv_nsec != 0);
}

long get_timer_time_left(timer_t tid) {
    struct itimerspec curr_its;
    if (timer_gettime(tid, &curr_its) == -1)
        err_exit("Failed on timer_gettime");
    return (curr_its.it_value.tv_sec * 1e9) + curr_its.it_value.tv_nsec;
}

void timer_set_up(timer_t *tidp, void (*handler)(union sigval)) {
    union sigval sv;
    struct sigevent sev = {
        .sigev_notify          = SIGEV_THREAD,
        .sigev_value           = sv,
        .sigev_notify_function = handler,
    };
    if (timer_create(CLOCK_REALTIME, &sev, tidp) != 0)
        err_exit("Failed on timer_create");
}

void timer_handler(union sigval sv) { write_input_event(sv.sival_ptr, true); }

static inline void setup_timers(timer_t timer_ids[NUM_SOURCE_KEYS]) {
    int j;
    for (j = 0; j < NUM_SOURCE_KEYS; j++) {
        timer_t timerid;
        timer_ids[j]              = timerid;
        struct input_event src_ke = {
            .type = EV_KEY, .code = SOURCE_KEYS[j], .value = KEY_PRESSED};
        struct sigevent sev = {
            .sigev_notify          = SIGEV_THREAD,
            .sigev_value           = {.sival_ptr = &src_ke},
            .sigev_notify_function = &timer_handler,
        };
        if (timer_create(CLOCK_REALTIME, &sev, &timerid) != 0)
            err_exit("Failed on timer_create");
    }
}

static inline bool are_all_other_timers_armed(size_t exclude_idx,
                                              timer_t *timer_ids) {
    bool all_other_timers_armed = false;
    int i;
    for (i = 0; i < NUM_SOURCE_KEYS; i++) {
        if (i == exclude_idx)
            continue;
        if (!is_timer_armed(timer_ids[i])) {
            all_other_timers_armed = false;
            break;
        }
        all_other_timers_armed = true;
    }
    return all_other_timers_armed;
}

static inline void disarm_all_other_timers(size_t exclude_idx,
                                           timer_t *timer_ids) {
    int i;
    for (i = 0; i < NUM_SOURCE_KEYS; i++) {
        if (i == exclude_idx)
            continue;
        timer_disarm(timer_ids[i]);
    }
}

////////////////////////////////////////////////////////////////////////////
// GENERAL UTILS
////////////////////////////////////////////////////////////////////////////
static inline size_t find_source_key_index(const int value) {
    size_t index = 0;
    while (index < NUM_SOURCE_KEYS && SOURCE_KEYS[index] != value)
        ++index;
    return (index == NUM_SOURCE_KEYS ? -1 : index);
};

static inline void update_timer_order(char *timer_order, size_t src_key_idx) {
    if (timer_order[0] == src_key_idx)
        return;
    switch (NUM_SOURCE_KEYS) {
        case 2:
            timer_order[1] = (char)timer_order[0];
            timer_order[0] = (char)src_key_idx;
            break;
        case 3:
            if (timer_order[1] == src_key_idx)
                timer_order[1] = (char)timer_order[0];
            else {
                timer_order[2] = (char)timer_order[1];
                timer_order[1] = (char)timer_order[0];
            }
            timer_order[0] = (char)src_key_idx;
            break;
        default:
            break;  // Should never get here! >3 keys not supported
    }
}

////////////////////////////////////////////////////////////////////////////////
// EVENT HANDLERS
////////////////////////////////////////////////////////////////////////////////
static inline void handle_non_source_key_event(const struct input_event *event,
                                               char *timer_order,
                                               timer_t *timer_ids) {
    if (event->value == KEY_PRESSED) {
        // If any src key timer armed, disarm it and write appropriate event
        int i;
        for (i = NUM_SOURCE_KEYS - 1; i > -1; i--) {
            // Make sure oldest activated timer gets checked first:
            char timer_idx = timer_order[i];
            if (is_timer_armed(timer_ids[timer_idx])) {
                timer_disarm(timer_ids[timer_idx]);
                write_key_event(SOURCE_KEYS[timer_idx], KEY_PRESSED, true);
            }
        }
    }
    fwrite(event, sizeof(*event), 1, stdout);
}

static inline void handle_source_key_event(const struct input_event *event,
                                           size_t source_key_idx,
                                           char *timer_order,
                                           timer_t *timer_ids) {
    switch (event->value) {
        case KEY_PRESSED:
            if (are_all_other_timers_armed(source_key_idx, timer_ids)) {
                // Disarm all other timers:
                disarm_all_other_timers(source_key_idx, timer_ids);
                // Write target 'pressed' event:
                write_key_event(TARGET_KEYS[0], KEY_PRESSED, false);
                TARGETS_STATE = TGT_PRESSED_WRITTEN;
            } else {
                timer_arm(timer_ids[source_key_idx]);
                update_timer_order(timer_order, source_key_idx);
            }
            break;
        case KEY_RELEASED:
            if (TARGETS_STATE == TGT_RELEASED_WRITTEN)
                // Timer shouldn't be armed anymore so we don't check
                TARGETS_STATE = TGT_INIT;
            else if (TARGETS_STATE == TGT_PRESSED_WRITTEN) {
                write_key_event(TARGET_KEYS[0], KEY_RELEASED, false);
                TARGETS_STATE = TGT_RELEASED_WRITTEN;
            }
            // Source key released before threshold has been reached:
            else if (is_timer_armed(timer_ids[source_key_idx])) {
                timer_disarm(timer_ids[source_key_idx]);
                write_key_event(SOURCE_KEYS[source_key_idx], KEY_PRESSED, true);
                fwrite(event, sizeof(*event), 1, stdout);
            }
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////////////////////////
int main(void) {
    // Do not buffer, we want evdev events to be handled in
    // real-time without delays (I guess we could set the buffer to be smaller
    // then `struct input_event`, which would force it to be flushed as well?)
    setbuf(stdin, NULL), setbuf(stdout, NULL);
    struct input_event event;

    ////////////////////////////////////////////////////////////////////////////
    // Set up timers
    ////////////////////////////////////////////////////////////////////////////
    timer_t timer_ids[NUM_SOURCE_KEYS];
    setup_timers(timer_ids);

    // Feels a bit jank, but need to keep track of order of timer activation
    char timer_order[NUM_SOURCE_KEYS];
    int k;
    for (k = 0; k < NUM_SOURCE_KEYS; k++)
        timer_order[k] = k;

    ////////////////////////////////////////////////////////////////////////////
    // Run main loop, reading events from stdin
    ////////////////////////////////////////////////////////////////////////////
    while (fread(&event, sizeof(event), 1, stdin) == 1) {
        if (event.type != EV_KEY)
            fwrite(&event, sizeof(event), 1, stdout);

        // Find source key index in SOURCE_KEYS
        size_t source_key_idx = find_source_key_index(event.code);

        switch (source_key_idx) {
            case A_NON_SOURCE_KEY:
                handle_non_source_key_event(&event, timer_order, timer_ids);
                break;
            case 0:
            case 1:
            case 2:
                handle_source_key_event(&event, source_key_idx, timer_order,
                                        timer_ids);
                break;
            default:
                break;
        }
    }
}
