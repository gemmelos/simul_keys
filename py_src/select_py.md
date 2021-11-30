# Evdev event

Output from `sudo evtest`.

Let's deal with packages like this:
```
Event: time 1636923867.838091, type 4 (EV_MSC), code 4 (MSC_SCAN), value 70028
Event: time 1636923867.838091, type 1 (EV_KEY), code 28 (KEY_ENTER), value 0
Event: time 1636923867.838091, -------------- SYN_REPORT ------------
```
Let's call all events read upto and including SYN_REPORT an `event_package`.

Ignoring `EV_MSC` events as done by interception tools auther here:
https://gitlab.com/interception/linux/plugins/caps2esc/-/blob/master/caps2esc.c#L93-94
Probably a good idea, if these represent raw scan codes but we only modify the
key event and write `EV_MSC` event as is then that would be conflicting.
So, if possible, we remove it from the event package that could prevent 
potential incorrect event package from causing a problem later on (I hope :/)

Let's now assume we are reading until SYN_REPORT, but no key event has been
detected... I think we would:
* Note this in some state object.
* Although reading is done (bc we reached the SYN_REPORT event) because there
    was no key event, we simply write the whole event package back out.
* Does it matter if one of our simul keys was being buffered?
    * YES
    * if we have detected no key event or a key event that is one of the other source keys
    * then we should first write our buffered event package and afterwards
    * write the read event package
* Let's say we see multiple events (ones we don't expect, or even knew could happen) before
    we read `SYN_REPORT` but we did pick up one key event for one of our source keys
    * Like so:
    ```txt
    Event: time 1636923867.838091, type 4 (EV_MSC), code 4 (MSC_SCAN), value 70028
    Event: time 1636923867.838091, type 0 (EV_IDK), code 4 (IDK), value 70028
    Event: time 1636923867.838091, type 2 (EV_JK),  code 4 (JK), value 70028
    Event: time 1636923867.838091, type 3 (EV_LOL), code 4 (LOL), value 70028
    Event: time 1636923867.838091, type 0 (EV_IDK), code 4 (IDK), value 70028
    Event: time 1636923867.838091, type 1 (EV_KEY), code 28 (KEY_ENTER), value 0
    Event: time 1636923867.838091, -------------- SYN_REPORT ------------
    ```
    * I think we would in this case buffer the whole package, bc it contains a source simul key down event, EXCEPT FOR THE `EV_MSC` event!! That won't ever be put in an event package in our program!!

    * 


# Some more examples
```
Event: time 1636923883.134423, type 4 (EV_MSC), code 4 (MSC_SCAN), value 7001e
Event: time 1636923883.134423, type 1 (EV_KEY), code 2 (KEY_1), value 1
Event: time 1636923883.134423, -------------- SYN_REPORT ------------

Event: time 1636923883.166260, type 4 (EV_MSC), code 4 (MSC_SCAN), value 7001e
Event: time 1636923883.166260, type 1 (EV_KEY), code 2 (KEY_1), value 0
Event: time 1636923883.166260, -------------- SYN_REPORT ------------
```