#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
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

// Some constants:
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


////////////////////////////////////////////////////////////////////////////////
// INPUT EVENT UTILS
////////////////////////////////////////////////////////////////////////////////
void write_event(const struct input_event *event)
{
    if (fwrite(event, sizeof(struct input_event), 1, stdout) != 1)
        err_exit("Failed on write_event");
}

void write_key_event(const struct input_event *iep, char syn_sleep)
{
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
void timer_thread_handler_source_one(union sigval sv)
{
    write_key_event(&source_one_down, 1);
}

void timer_thread_handler_source_two(union sigval sv)
{
    write_key_event(&source_two_down, 1);
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
    /* its->it_value.tv_nsec = SIMUL_DELAY; */
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

// Define your source and target keys:
static const unsigned int SOURCE_KEYS[] = {KEY_J, KEY_K};
static const unsigned int TARGET_KEYS[] = {KEY_ESC};

void timer_handler(union sigval sv) { write_key_event(sv.sival_ptr, 1); }

size_t find_index(const int array[], const size_t size, const int value)
{
    size_t index = 0;
    while (index < size && array[index] != value)
        ++index;
    return (index == size ? -1 : index);
};

///////////////////////////////////////////////////////////////////////////

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

    // Old way:
    // timer_t timerid_source_one;
    // timer_t timerid_source_two;
    // timer_set_up(&timerid_source_one, &timer_thread_handler_source_one);
    // timer_set_up(&timerid_source_two, &timer_thread_handler_source_two);
    // struct itimerspec its;

    enum
    {
        TGT_INIT,
        TGT_PRESSED_WRITTEN,
        TGT_RELEASED_WRITTEN
    } targets_state = TGT_INIT;

    // Set up keys
    const int key_codes[] = {KEY_J, KEY_K};
    const size_t num_key_codes = sizeof(key_codes) / sizeof(int);
    struct input_event events[num_key_codes][2];
    int i;
    for (i = 0; i < num_key_codes; i++)
    {
        // Key up/released
        const struct input_event down = {
            .type = EV_KEY, .code = key_codes[i], .value = 1};
        events[i][0] = down;
        // Key down/pressed
        const struct input_event up = {
            .type = EV_KEY, .code = key_codes[i], .value = 0};
        events[i][1] = up;
    }

    // Set up timers
    timer_t timer_ids[num_key_codes];
    int j;
    for (j = 0; j < num_key_codes; j++)
    {
        timer_t timerid;
        timer_ids[j] = timerid;
        struct sigevent sev = {
            .sigev_notify = SIGEV_THREAD,
            // void   *sival_ptr;    /* Pointer value */
            .sigev_value = {.sival_ptr = &events[j][1]},
            .sigev_notify_function = &timer_handler,
        };
        if (timer_create(CLOCK_REALTIME, &sev, &timerid) != 0)
            err_exit("Failed on timer_create");
    }

    // old way:
    /* timer_t timerid_source_one = timer_ids[0]; */
    /* timer_t timerid_source_two = timer_ids[1]; */

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    while (fread(&event, sizeof(event), 1, stdin) == 1)
    {
        if (event.type == EV_KEY)
        {
            // find key code index in simul keys
            size_t index_found =
                find_index(key_codes, num_key_codes, event.code);

            if (event.value == KEY_PRESSED && index_found < 0)
            {
                int i;
                for (i = 0; i < num_key_codes; i++)
                {
                    if (is_timer_armed(timer_ids[i]))
                    {
                        timer_disarm(timer_ids[i]);
                        write_key_event(&events[(1 + i) % 2][KEY_PRESSED], 1);
                    }
                }
            }
            else if (index_found == 0 || index_found == 1)
            {
                if (event.value == KEY_PRESSED)
                {
                    if (is_timer_armed(timer_ids[(1 + index_found) % 2]))
                    {
                        timer_disarm(timer_ids[(1 + index_found) % 2]);
                        write_key_event(&target_down, 0);
                        targets_state = TGT_PRESSED_WRITTEN;
                        continue;
                    }
                    else
                    {
                        timer_arm(timer_ids[index_found]);
                        continue;
                    }
                }
                else if (event.value == KEY_RELEASED)
                {
                    if (targets_state == TGT_RELEASED_WRITTEN)
                    {
                        // timer shouldn't be armed anymore
                        // in this state so we don't check
                        targets_state = TGT_INIT;
                        continue;
                    }
                    else if (targets_state == TGT_PRESSED_WRITTEN)
                    {
                        write_key_event(&target_up, 0);
                        targets_state = TGT_RELEASED_WRITTEN;
                        continue;
                    }
                    else
                    {
                        timer_disarm(timer_ids[index_found]);
                        write_key_event(&events[i][KEY_PRESSED], 1);
                    }
                }
            }

            // Catch non-source-key down while source-key is down
            // (i.e. while timer still going)
            /* if (event.value == KEY_PRESSED && event.code != SOURCE_ONE_CODE
             * && */
            /*     event.code != SOURCE_TWO_CODE) */
            /* { */
            /*     if (timer_is_armed(timerid_source_one)) */
            /*     { */
            /*         timer_disarm(timerid_source_one, &its); */
            /*         write_key_event(&source_one_down, 1); */
            /*     } */
            /*     else if (timer_is_armed(timerid_source_two)) */
            /*     { */
            /*         timer_disarm(timerid_source_two, &its); */
            /*         write_key_event(&source_two_down, 1); */
            /*     } */
            /* } */
            /* else if (event.code == SOURCE_ONE_CODE) */
            /* { */
            /*     if (event.value == KEY_PRESSED) */
            /*     { */
            /*         if (timer_is_armed(timerid_source_two)) */
            /*         { */
            /*             timer_disarm(timerid_source_two, &its); */
            /*             write_key_event(&target_down, 0); */
            /*             targets_state = TGT_PRESSED_WRITTEN; */
            /*             continue; */
            /*         } */
            /*         else */
            /*         { */
            /*             timer_arm(timerid_source_one, &its); */
            /*             continue; */
            /*         } */
            /*     } */
            /*     else if (event.value == KEY_RELEASED) */
            /*     { */
            /*         if (targets_state == TGT_RELEASED_WRITTEN) */
            /*         { */
            /*             // timer shouldn't be armed anymore */
            /*             // in this state so we don't check them */
            /*             targets_state = TGT_INIT; */
            /*             continue; */
            /*         } */
            /*         else if (targets_state == TGT_PRESSED_WRITTEN) */
            /*         { */
            /*             targets_state = TGT_RELEASED_WRITTEN; */
            /*             write_key_event(&target_up, 0); */
            /*             continue; */
            /*         } */
            /*         else */
            /*         { */
            /*             timer_disarm(timerid_source_one, &its); */
            /*             write_key_event(&source_one_down, 1); */
            /*         } */
            /*     } */
            /* } */
            /* else if (event.code == SOURCE_TWO_CODE) */
            /* { */
            /*     if (event.value == KEY_PRESSED) */
            /*     { */
            /*         if (timer_is_armed(timerid_source_one)) */
            /*         { */
            /*             timer_disarm(timerid_source_one, &its); */
            /*             write_key_event(&target_down, 0); */
            /*             targets_state = TGT_PRESSED_WRITTEN; */
            /*             continue; */
            /*         } */
            /*         else */
            /*         { */
            /*             timer_arm(timerid_source_two, &its); */
            /*             continue; */
            /*         } */
            /*     } */
            /*     else if (event.value == KEY_RELEASED) */
            /*     { */
            /*         if (targets_state == TGT_RELEASED_WRITTEN) */
            /*         { */
            /*             targets_state = TGT_INIT; */
            /*             continue; */
            /*         } */
            /*         else if (targets_state == TGT_PRESSED_WRITTEN) */
            /*         { */
            /*             targets_state = TGT_RELEASED_WRITTEN; */
            /*             write_key_event(&target_up, 0); */
            /*             continue; */
            /*         } */
            /*         else */
            /*         { */
            /*             timer_disarm(timerid_source_two, &its); */
            /*             write_key_event(&source_two_down, 1); */
            /*         } */
            /*     } */
            /* } */
        }

        // Write the potentially modified event
        fwrite(&event, sizeof(event), 1, stdout);
    }
}