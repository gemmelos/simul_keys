#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define KEY_RELEASED 0
#define KEY_PRESSED 1
#define KEY_REPEATED 2
#define SIMUL_THRESHOLD 50e6 // 50ms - Same as KarabinerElements' default
#define err_exit(msg)                                                          \
    {                                                                          \
        perror(msg);                                                           \
        exit(EXIT_FAILURE);                                                    \
    }
#define A_NON_SOURCE_KEY = -1

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
// 0: base state, target key has not been written yet
// 1: target key down event written
// 2: target key up event written (important to ignore our second source key
// up)

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
// clang-format on

/* #define SOURCE_ONE_CODE source_one_down.code */
/* #define SOURCE_TWO_CODE source_two_down.code */
/* #define TARGET_CODE target_down.code */

/* Only exactly 2 source keys are supported currently! */
/* static const unsigned  int SOURCE_KEYS[] = {KEY_J, KEY_K}; */
/* static const unsigned int TARGET_KEYS[] = {KEY_ESC}; */

// Some constants:
static const struct itimerspec ITS_ON = {
    .it_value.tv_sec = 0,
    .it_value.tv_nsec = SIMUL_THRESHOLD,
    .it_interval.tv_sec = 0,  // no repeat
    .it_interval.tv_nsec = 0, // no repeat
};

static const struct itimerspec ITS_OFF = {
    .it_value.tv_sec = 0,
    .it_value.tv_nsec = 0,
    .it_interval.tv_sec = 0,  // no repeat
    .it_interval.tv_nsec = 0, // no repeat
};

////////////////////////////////////////////////////////////////////////////////
// INPUT EVENT UTILS
////////////////////////////////////////////////////////////////////////////////
void write_event(const struct input_event *event)
{
    if (fwrite(event, sizeof(struct input_event), 1, stdout) != 1)
        err_exit("Failed on write_event");
}

void write_key_event(const struct input_event *iep, bool syn_sleep)
{
    write_event(iep);
    write_event(&syn);
    // If we write another event after `iep` we need to sleep first
    // for syncing of events to work (I think?)
    if (syn_sleep)
        usleep(20000); // 0.2 ms or 200 us
}

////////////////////////////////////////////////////////////////////////////////
// TIMER UTILS
////////////////////////////////////////////////////////////////////////////////
void timer_thread_handler_source_one(union sigval sv)
{
    write_key_event(&source_one_down, true);
}

void timer_thread_handler_source_two(union sigval sv)
{
    write_key_event(&source_two_down, true);
}

// void timer_disarm(timer_t tid, struct itimerspec *its)
void timer_disarm(timer_t tid)
{
    /* its->it_value.tv_sec = 0; */
    /* its->it_value.tv_nsec = 0; */
    /* its->it_interval.tv_sec = 0;  // no repeat */
    /* its->it_interval.tv_nsec = 0; // no repeat */
    /* // Set timer */
    /* int set_timer_rt = timer_settime(tid, 0, its, NULL); */
    int set_timer_rt = timer_settime(tid, 0, &ITS_OFF, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

// void timer_arm(timer_t tid, struct itimerspec *its)
void timer_arm(timer_t tid)
{
    /* its->it_value.tv_sec = 0; */
    /* its->it_value.tv_nsec = SIMUL_THRESHOLD; */
    /* its->it_interval.tv_sec = 0;  // no repeat */
    /* its->it_interval.tv_nsec = 0; // no repeat */
    /* // Set timer */
    /* int set_timer_rt = timer_settime(tid, 0, its, NULL); */
    int set_timer_rt = timer_settime(tid, 0, &ITS_ON, NULL);
    if (set_timer_rt == -1)
        err_exit("Failed on timer_settime");
}

int is_timer_armed(timer_t tid)
{
    // Get timer status (remaining time)
    struct itimerspec curr_its;
    if (timer_gettime(tid, &curr_its) == -1)
        err_exit("Failed on timer_gettime");
    return (curr_its.it_value.tv_sec != 0 || curr_its.it_value.tv_nsec != 0);
}

void timer_set_up(timer_t *tidp, void (*handler)(union sigval))
{
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

// Define your source and target keys
// Currently supported: only length 2 source_keys and length 1 target_keys
static const unsigned int SOURCE_KEYS[] = {KEY_J, KEY_K};
static const unsigned int TARGET_KEYS[] = {KEY_ESC};
static const size_t NUM_SOURCE_KEYS = sizeof(SOURCE_KEYS) / sizeof(int);

void timer_handler(union sigval sv) { write_key_event(sv.sival_ptr, true); }

inline size_t find_index(const int array[], const size_t size, const int value)
{
    size_t index = 0;
    while (index < size && array[index] != value)
        ++index;
    return (index == size ? -1 : index);
};

/**
 * Get 'opposite'/'inverse' index by XOR-ing with 1
 * 0 -> 1
 * 1 -> 0
 */
inline size_t goi(size_t index) { return index ^ 0x1; }

////////////////////////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    // Do not buffer, we want evdev events to be handled in
    // real-time without delays (I guess we could set the buffer to be smaller
    // then `struct input_event`, which would force it to be flushed as well?)
    setbuf(stdin, NULL), setbuf(stdout, NULL);
    struct input_event event;

    enum
    {
        TGT_INIT,
        TGT_PRESSED_WRITTEN,
        TGT_RELEASED_WRITTEN
    } targets_state = TGT_INIT;

    // Set up keys
    /* const int SOURCE_KEYS[] = {KEY_J, KEY_K}; */
    /* const size_t NUM_SOURCE_KEYS = sizeof(key_codes) / sizeof(int); */
    struct input_event source_key_events[NUM_SOURCE_KEYS][2];
    int i;
    for (i = 0; i < NUM_SOURCE_KEYS; i++)
    {
        // Key up/released
        const struct input_event down = {
            .type = EV_KEY, .code = SOURCE_KEYS[i], .value = 1};
        source_key_events[i][0] = down;
        // Key down/pressed
        const struct input_event up = {
            .type = EV_KEY, .code = SOURCE_KEYS[i], .value = 0};
        source_key_events[i][1] = up;
    }

    // Set up timers
    timer_t timer_ids[NUM_SOURCE_KEYS];
    int j;
    for (j = 0; j < NUM_SOURCE_KEYS; j++)
    {
        timer_t timerid;
        timer_ids[j] = timerid;
        struct sigevent sev = {
            .sigev_notify = SIGEV_THREAD,
            // void   *sival_ptr;    /* Pointer value */
            .sigev_value = {.sival_ptr = &source_key_events[j][1]},
            .sigev_notify_function = &timer_handler,
        };
        if (timer_create(CLOCK_REALTIME, &sev, &timerid) != 0)
            err_exit("Failed on timer_create");
    }

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    while (fread(&event, sizeof(event), 1, stdin) == 1)
    {
        if (event.type != EV_KEY)
            fwrite(&event, sizeof(event), 1, stdout);

        // find key code index in simul keys
        size_t source_key_found =
            find_index(SOURCE_KEYS, NUM_SOURCE_KEYS, event.code);

        /* switch (source_key_found) */
        /* { */
        /* // Event is not a source key */
        /* case A_NON_SOURCE_KEY: */
        /*     switch (event.value) */
        /*     { */
        /*     case KEY_PRESSED: */
        /*         int i; */
        /*         for (i = 0; i < NUM_SOURCE_KEYS; i++) */
        /*         { */
        /*             if (is_timer_armed(timer_ids[i])) */
        /*             { */
        /*                 timer_disarm(timer_ids[i]); */
        /*                 write_key_event(&source_key_events[goi(i)][KEY_PRESSED], */
        /*                                 true); */
        /*             } */
        /*         } */
        /*         // Fall through 👇 */
        /*     default: */
        /*         // Write actual incoming key event */
        /*         fwrite(&event, sizeof(event), 1, stdout); */
        /*         break; */
        /*     } */
        /*     break; */
        /* // Event is a source key */
        /* case 0: */
        /* case 1: */
        /*     switch (event.value) */
        /*     { */
        /*     case KEY_PRESSED: */
        /*         if (is_timer_armed(timer_ids[goi(source_key_found)])) */
        /*         { */
        /*             // Disarm all simul-key-timers */
        /*             timer_disarm(timer_ids[goi(source_key_found)]); */
        /*             write_key_event(&target_down, false); */
        /*             targets_state = TGT_PRESSED_WRITTEN; */
        /*         } */
        /*         else */
        /*             timer_arm(timer_ids[source_key_found]); */
        /*         break; */
        /*     case KEY_RELEASED: */
        /*         if (targets_state == TGT_RELEASED_WRITTEN) */
        /*             // Timer shouldn't be armed anymore so we don't check */
        /*             targets_state = TGT_INIT; */
        /*         else if (targets_state == TGT_PRESSED_WRITTEN) */
        /*         { */
        /*             write_key_event(&target_up, false); */
        /*             targets_state = TGT_RELEASED_WRITTEN; */
        /*         } */
        /*         else if (is_timer_armed(timer_ids[source_key_found])) */
        /*         { */
        /*             timer_disarm(timer_ids[source_key_found]); */
        /*             write_key_event( */
        /*                 &source_key_events[source_key_found][KEY_PRESSED], */
        /*                 true); */
        /*             fwrite(&event, sizeof(event), 1, stdout); */
        /*         } */
        /*         break; */
        /*     default: */
        /*         break; */
        /*     } */
        /*     break; */
        /* default: */
        /*     break; */
        /* } */
        /* // We don't need `fwrite(&event, sizeof(event), 1, stdout)` anymore here */

        if (event.value == KEY_PRESSED && source_key_found < 0)
        {
            int i;
            for (i = 0; i < NUM_SOURCE_KEYS; i++)
            {
                if (is_timer_armed(timer_ids[i]))
                {
                    timer_disarm(timer_ids[i]);
                    write_key_event(&source_key_events[goi(i)][KEY_PRESSED],
                                    true);
                }
            }
        }
        else if (source_key_found == 0 || source_key_found == 1)
        {
            // Swallow input event:
            if (event.value == KEY_PRESSED)
            {
                if (is_timer_armed(timer_ids[goi(source_key_found)]))
                {
                    // Disarm all simul-timers
                    timer_disarm(timer_ids[goi(source_key_found)]);
                    write_key_event(&target_down, false);
                    targets_state = TGT_PRESSED_WRITTEN;
                    continue;
                }
                else
                {
                    timer_arm(timer_ids[source_key_found]);
                    continue;
                }
            }
            else if (event.value == KEY_RELEASED)
            {
                if (targets_state == TGT_RELEASED_WRITTEN)
                {
                    // Timer shouldn't be armed anymore
                    // in this state so we don't check
                    targets_state = TGT_INIT;
                    continue;
                }
                else if (targets_state == TGT_PRESSED_WRITTEN)
                {
                    write_key_event(&target_up, false);
                    targets_state = TGT_RELEASED_WRITTEN;
                    continue;
                }
                // We don't want to do this if the timer isn't actually
                // going anymore, i.e. when we've pressed `j` down for
                // longer than the threshold and the timer handler has
                // already written `j` down-event, right?
                // Not sure why original code isn't doing this but seems
                // to be working fine? Maybe because this case never happens
                // I never hold down `j` for >=50ms (threshold ms) to
                // trigger the handler?
                else if (is_timer_armed(timer_ids[source_key_found]))
                {
                    // So for example an `j`-up event
                    // Do we know `k` is down/being-swallowed rn?
                    // - if it is then the even after this one will determine
                    // what should happen with `k` down
                    // - if it is not then we don't really care at all.
                    //
                    // So in case `j` down-event timer is running then we
                    // know we have no release `j` key before the threshold
                    // so we should disable the timer and before the action
                    // the timer handler would perform, i.e. send/transmit/
                    // write-to-stdout the `j` key-down event
                    // Afterwards we fall through the rest of the code block
                    // to arrive at the `fwrite` call that will write the
                    // actual `j` up-event.
                    timer_disarm(timer_ids[source_key_found]);
                    write_key_event(
                        &source_key_events[source_key_found][KEY_PRESSED],
                        true);
                }
            }
        }

        // Write the potentially modified event
        fwrite(&event, sizeof(event), 1, stdout);
    }
}
