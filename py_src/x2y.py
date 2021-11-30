import struct
import sys
import time
from threading import Timer
from typing import NamedTuple, TypedDict
from enum import Enum, auto


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
)
SYN_REPORT = 0
SYN_EVENT = InputEvent(0, 0, TYPE["EV_SYN"], SYN_REPORT, 0)

# Simul config
SOURCE_KEYS = (KEY["J"], KEY["K"], KEY["L"])
TARGET_KEY = KEY["ESC"]
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
    while True:
        event_raw = sys.stdin.buffer.read(INPUT_EVENT.size)
        event = InputEvent._make(struct.unpack(STRUCT_FORMAT, event_raw))

        if event.type == TYPE["EV_KEY"] and event.code == KEY["L"]:
            write_key_event(KEY["S"], event.value)
        else:
            write_raw_event(event_raw)


if __name__ == "__main__":
    main()
