# Simultaneous key presses

Mostly inspired by Karabiner-Elements, [simultaneous docs](https://karabiner-elements.pqrs.org/docs/json/complex-modifications-manipulator-definition/from/simultaneous).

## Why use timers?
(Let's assume a threshold of 50ms)

Let's say we map the 'simultaneous' press of the keys `a`, `b` and `c` to `ESC`.
When `a` is pressed we want to swallow it and NOT transmit/send/write-to-stdout the key-down event.
Because if we do and you are, for example, in a text editor, the letter "a" will show up.
Instead we need to swallow it (i.e. hold on to the `a` key-down event for a bit) and wait for the simul-threshold (say 50ms) before we actually want to transmit it, because we could press `b` and `c` in the threshold time and if so we should send a key-down event for our target key `ESC`.
For the swallowing behavior timers are used.
The timer's behavior is quite simple.
It uses `SIGEV_THREAD` to execute its handler.
There exists a timer+handler for each simul-key (i.e. each source key).
On timer expiration the handler will be executed.
The handler transmits/writes-to-stdout the simul-key linked to this timer, i.e. spit out the swallowed key xD
We terminate/stop the timer and therefore prevent the handler from being able to fire in the case where all required simul-keys, for our example `a`, `b` and `c`, are pressed within the threshold time, which we refer to as having pressed `a`, `b` and `c` 'simultaneously'.

In the case having more than two source keys this approach has the unfortunate side-effect that we do not take into account the running timer of the first source when the second source key is pressed within the threshold.
So if `a` is pressed and 30ms later `b` is pressed, we do not set the timer for `b` to (50 - 20 =) 30ms but instead set it to 50ms as well.
This means that when pressing `a` and `b` there will be the same delay for both keys before they are actually transmitted/written-to-stdout.
Currently this is mainly chosen because of laziness and keeping the timer setup/activation code simpler.
By having this logic though I suppose the following is possible: press `a` --30ms--> press `b` --30ms--> press `c` --5ms--> release `a` --5ms--> press `a` will result in the target key(s) being transmitted. (maybe an unlikely scenario)
Intuitively I think we want the second source key's timer to last for 50 - 'time elapsed on first source key's timer', so that they would end around the same time.
Technically we can get the elapsed time information from any timer and use it.
Would this additional logic cause lag or other problems?
I guess probably not because we are going to swallow the key no matter anyway, we don't need reduce the logic because we don't need to send the key back out immediately.
Worth a try for v2 of the three-simul-keys implementation.


## To build and run

```sh
sudo ./build_run.sh
```

## Test build with docker

```
docker build -t simul_keys:latest .
```

# Goal

Pressing `j` and `k` within X ms gives `ESC`.
Where X is something small like 100 or 50.

For example:

```text
                <---------100ms--------->     <---------100ms--------->
keyboard:       J↓      K↓              J↑       K↑
computer sees:          ESC↓            ESC↑
```

## Behavior - 1 Key

1.

```txt
<------ threshold ------>
jd                              ju
                     jdR        ju
```

2.

```txt
<------ threshold ------>
jd      ju
        jd,ju
```

3.

```txt
<------ threshold ------>
jd    ad
      jd,ad
```

4.

```txt
<------ threshold ------>
jd    ad     ju  au
      jd,ad  ju  au
```

5.

```txt
<------ threshold ------>
jd    ad     au  ju
      jd,ad  au  ju
```

I think 4 and 5 work, i.e. same as if we would press `m` (instead of `j`) and `a`.
Not 100% yet, let's see if something comes up later on.

## Behavior - Two keys

```txt
<------ threshold ------>
jd    kd     ku         ju
      Ed     Eu
```

> How do we catch ESC repeat and send it through?

Possibly we have a state `bool ESC_DOWN` which if true and we are receiving `j` or `k` repeat (I guess we should choose either one not both?) then we send ESC repeat instead?

### Example j+k to r

```
keyboard: jd -> kd (in threshold) -> ku -> ju
computer: -  -> rd                -> ru -> -
```

## `evtest` output

```txt
J+k to ESC
esc, j and k (down and up) in evtest:
Event: time 1614473865.823027, type 4 (EV_MSC), code 4 (MSC_SCAN), value
70029 Event: time 1614473865.823027, type 1 (EV_KEY), code 1 (KEY_ESC),
value 1 Event: time 1614473865.823027, -------------- SYN_REPORT
------------ Event: time 1614473865.886967, type 4 (EV_MSC), code 4
(MSC_SCAN), value 70029 Event: time 1614473865.886967, type 1 (EV_KEY), code
1 (KEY_ESC), value 0 Event: time 1614473865.886967, --------------
SYN_REPORT ------------ Event: time 1614473872.223033, type 4 (EV_MSC), code
4 (MSC_SCAN), value 7000d Event: time 1614473872.223033, type 1 (EV_KEY),
code 36 (KEY_J), value 1 Event: time 1614473872.223033, --------------
SYN_REPORT ------------ Event: time 1614473872.295044, type 4 (EV_MSC), code
4 (MSC_SCAN), value 7000d Event: time 1614473872.295044, type 1 (EV_KEY),
code 36 (KEY_J), value 0 Event: time 1614473872.295044, --------------
SYN_REPORT ------------ Event: time 1614473873.343037, type 4 (EV_MSC), code
4 (MSC_SCAN), value 7000e Event: time 1614473873.343037, type 1 (EV_KEY),
code 37 (KEY_K), value 1 Event: time 1614473873.343037, --------------
SYN_REPORT ------------ Event: time 1614473873.390970, type 4 (EV_MSC), code
4 (MSC_SCAN), value 7000e Event: time 1614473873.390970, type 1 (EV_KEY),
code 37 (KEY_K), value 0 Event: time 1614473873.390970, --------------
SYN_REPORT ------------

Capslock to Hyper
Hyper (shift+ctrl+alt+super) in evtest (down and up):
Event: time 1614473686.134751, type 4 (EV_MSC), code 4 (MSC_SCAN), value
700e0 Event: time 1614473686.134751, type 1 (EV_KEY), code 29
(KEY_LEFTCTRL), value 1 Event: time 1614473686.134751, --------------
SYN_REPORT ------------ Event: time 1614473686.166745, type 4 (EV_MSC), code
4 (MSC_SCAN), value 700e1 Event: time 1614473686.166745, type 1 (EV_KEY),
code 42 (KEY_LEFTSHIFT), value 1 Event: time 1614473686.166745,
-------------- SYN_REPORT ------------ Event: time 1614473686.198746, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e2 Event: time 1614473686.198746, type
1 (EV_KEY), code 56 (KEY_LEFTALT), value 1 Event: time 1614473686.198746,
-------------- SYN_REPORT ------------ Event: time 1614473686.230650, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e3 Event: time 1614473686.230650, type
1 (EV_KEY), code 125 (KEY_LEFTMETA), value 1 Event: time 1614473686.230650,
-------------- SYN_REPORT ------------ Event: time 1614473686.342747, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e0 Event: time 1614473686.342747, type
1 (EV_KEY), code 29 (KEY_LEFTCTRL), value 0 Event: time 1614473686.342747,
-------------- SYN_REPORT ------------ Event: time 1614473686.350674, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e1 Event: time 1614473686.350674, type
1 (EV_KEY), code 42 (KEY_LEFTSHIFT), value 0 Event: time 1614473686.350674,
-------------- SYN_REPORT ------------ Event: time 1614473686.358668, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e2 Event: time 1614473686.358668, type
1 (EV_KEY), code 56 (KEY_LEFTALT), value 0 Event: time 1614473686.358668,
-------------- SYN_REPORT ------------ Event: time 1614473686.374668, type 4
(EV_MSC), code 4 (MSC_SCAN), value 700e3 Event: time 1614473686.374668, type
1 (EV_KEY), code 125 (KEY_LEFTMETA), value 0 Event: time 1614473686.374668,
-------------- SYN_REPORT ------------

Emulate evtest out on j_down+j_up:
Event: time 1613304998.635310, type 4 (EV_MSC), code 4 (MSC_SCAN), value
7000d Event: time 1613304998.635310, type 1 (EV_KEY), code 36 (KEY_J), value
1 Event: time 1613304998.635310, -------------- SYN_REPORT ------------
Event: time 1613304998.683267, type 4 (EV_MSC), code 4 (MSC_SCAN), value
7000d Event: time 1613304998.683267, type 1 (EV_KEY), code 36 (KEY_J), value
0 Event: time 1613304998.683267, -------------- SYN_REPORT ------------

J held down event:
Event: time 1613316096.604709, type 4 (EV_MSC), code 4 (MSC_SCAN), value
7000d Event: time 1613316096.604709, type 1 (EV_KEY), code 36 (KEY_J), value
1 Event: time 1613316096.604709, -------------- SYN_REPORT ------------
Event: time 1613316096.875064, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316096.875064, -------------- SYN_REPORT ------------
Event: time 1613316096.911761, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316096.911761, -------------- SYN_REPORT ------------
Event: time 1613316096.948409, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316096.948409, -------------- SYN_REPORT ------------
Event: time 1613316096.985077, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316096.985077, -------------- SYN_REPORT ------------
Event: time 1613316097.021720, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316097.021720, -------------- SYN_REPORT ------------
Event: time 1613316097.058389, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316097.058389, -------------- SYN_REPORT ------------
Event: time 1613316097.095076, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316097.095076, -------------- SYN_REPORT ------------
Event: time 1613316097.131705, type 1 (EV_KEY), code 36 (KEY_J), value 2
Event: time 1613316097.131705, -------------- SYN_REPORT ------------
Event: time 1613316097.131705, type 4 (EV_MSC), code 4 (MSC_SCAN), value
7000d Event: time 1613316097.131705, type 1 (EV_KEY), code 36 (KEY_J), value
0 Event: time 1613316097.131705, -------------- SYN_REPORT ------------
```
