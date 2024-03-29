import struct
import sys
import time
from enum import Enum, auto
from functools import partial
from threading import Timer
from typing import Callable, Dict, List, Literal, NamedTuple, TypedDict


class TargetState(Enum):
    IDLE = auto()
    PRESSED = auto()
    RELEASED = auto()


class SourceItem(TypedDict):
    source_key: int
    timer: Timer


class InputEvent(NamedTuple):
    sec: int
    usec: int
    type: int
    code: int
    value: int


# input_event struct format:
# see: https://docs.python.org/3/library/struct.html#format-characters
STRUCT_FORMAT = "llHHI"
INPUT_EVENT = struct.Struct(STRUCT_FORMAT)
TYPE = {"EV_SYN": 0, "EV_KEY": 1}
VALUE = {"RELEASE": 0, "PRESS": 1, "REPEAT": 2}
# See: https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
KEY = dict(
    ESC=1,
    S=31,
    D=32,
    H=35,
    J=36,
    K=37,
    L=38,
    UP=103,
    LEFT=105,
    RIGHT=106,
    DOWN=108,
    Q=16,
    X=45,
)
SYN_REPORT = 0
SYN_EVENT = InputEvent(0, 0, TYPE["EV_SYN"], SYN_REPORT, 0)

# Simul config
SOURCE_KEYS = (KEY["Q"], KEY["X"])
TARGET_KEY = KEY["S"]
THRESHOLD_SEC = 0.05  # 0.05 sec == 50 ms


def write_raw_event(raw_event: bytes):
    sys.stdout.buffer.write(raw_event)
    sys.stdout.buffer.flush()


def write_event(event: InputEvent):
    data = struct.pack(STRUCT_FORMAT, *event)
    write_raw_event(data)


def write_key_event(key_code: int, key_action: int, syn_sleep: bool = True):
    event = InputEvent(0, 0, TYPE["EV_KEY"], key_code, key_action)
    write_event(event)
    write_event(SYN_EVENT)  # syn pause
    if syn_sleep:
        time.sleep(0.0002)  # 0.2 ms or 200 us


def main():
    # Target state:
    target_state: TargetState = TargetState.IDLE
    target_state_sources_down: Dict[int, Literal["down", "up"]] = {
        key: "up" for key in SOURCE_KEYS
    }

    # Sources state:
    sources_state: List[SourceItem] = [
        {
            "source_key": src_key,
            "timer": Timer(
                interval=THRESHOLD_SEC,
                function=write_key_event,
                args=(src_key, VALUE["PRESS"], True),
            ),
        }
        for src_key in SOURCE_KEYS
    ]
    num_source_keys = len(SOURCE_KEYS)

    while True:
        event_raw = sys.stdin.buffer.read(INPUT_EVENT.size)
        event = InputEvent._make(struct.unpack(STRUCT_FORMAT, event_raw))

        if event.type != TYPE["EV_KEY"]:
            write_raw_event(event_raw)
            continue

        # Handle non-source key
        if event.code not in SOURCE_KEYS:
            if event.value == VALUE["PRESS"]:
                for source in sources_state:
                    if source["timer"].is_alive():
                        source["timer"].cancel()
                        write_key_event(source["source_key"], VALUE["PRESS"])
            write_raw_event(event_raw)
            continue

        # Incoming source key (next should always find the item here)
        # (returns reference to dict in list)
        inc_source = next(
            item for item in sources_state if item["source_key"] == event.code
        )
        inc_src_idx = sources_state.index(inc_source)

        # Handle source key:
        if event.value == VALUE["PRESS"]:
            other_sources = (
                item for item in sources_state if item["source_key"] != event.code
            )
            if all((s["timer"].is_alive() for s in other_sources)):
                for source in other_sources:
                    source["timer"].cancel()
                write_key_event(TARGET_KEY, VALUE["PRESS"], False)
                target_state: TargetState = TargetState.PRESSED
                target_state_sources_down = {
                    k: "down" for k in target_state_sources_down
                }
            else:
                # Cancel to be save
                # (can do on already finished timer or
                # if already called cancel)
                inc_source["timer"].cancel()
                inc_source["timer"] = Timer(
                    interval=THRESHOLD_SEC,
                    function=write_key_event,
                    args=(source["source_key"], VALUE["PRESS"], True),
                )
                inc_source["timer"].start()
                # Move to back of state list:
                sources_state.insert(
                    num_source_keys - 1,
                    sources_state.pop(inc_src_idx),
                )
        elif event.value == VALUE["RELEASE"]:
            if target_state == TargetState.PRESSED:
                write_key_event(TARGET_KEY, VALUE["RELEASE"], False)
                target_state = TargetState.RELEASED
                target_state_sources_down[event.code] = "up"
            elif target_state == TargetState.RELEASED:
                target_state_sources_down[event.code] = "up"
                # Only set target state to IDLE if all source keys are up again
                if all((v == "up" for v in target_state_sources_down.values())):
                    target_state = TargetState.IDLE
            else:
                # Could be releasing a source key (before task expiry)
                if inc_source["timer"].is_alive():
                    inc_source["timer"].cancel()
                    write_key_event(inc_source["source_key"], VALUE["PRESS"])
                write_raw_event(event_raw)


if __name__ == "__main__":
    main()
