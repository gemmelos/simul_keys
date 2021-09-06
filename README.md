# Simultaneous key presses

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

1)

```txt
<------ delay ------>
jd                              ju
                     jdR        ju
```

2)

```txt
<------ delay ------>
jd      ju
        jd,ju
```

3)

```txt
<------ delay ------>
jd    ad
      jd,ad
```

4)

```txt
<------ delay ------>
jd    ad     ju  au
      jd,ad  ju  au
```

5)

```txt
<------ delay ------>
jd    ad     au  ju
      jd,ad  au  ju
```

I think 4 and 5 work, i.e. same as if we would press `m` (instead of `j`) and `a`.
Not 100% yet, let's see if something comes up later on.


## Behavior - Two keys

```txt
<------ delay ------>
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
