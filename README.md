# Simultaneous key presses

## To build and run

```sh
sudo ./build_run.sh
```

## Goal

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

