import asyncio
import struct
import sys
import time
from typing import Tuple
from config import (
    INPUT_EVENT_SIZE,
    SOURCE_KEYS,
    STRUCT_FORMAT,
    SYN_EVENT,
    TARGET_KEY,
    THRESHOLD_SEC,
    TYPE,
    VALUE,
)
import os
import struct
import sys
import time
from asyncio.tasks import Task
from enum import Enum, auto
from typing import Dict, List, Literal, NamedTuple, TypedDict

# Set buffering to 0, i.e. turn off buffering
# ref: https://medium.com/@bramblexu/three-ways-to-close-buffer-for-stdout-stdin-stderr-in-python-8be694bd2737
# XXX does this work though? :/
sys.stdout = os.fdopen(sys.stdout.fileno(), "wb", buffering=0)
sys.stderr = os.fdopen(sys.stderr.fileno(), "wb", buffering=0)
sys.stdin = os.fdopen(sys.stdin.fileno(), "rb", buffering=0)


class TargetState(Enum):
    IDLE = auto()
    PRESSED = auto()
    RELEASED = auto()


class SourceItem(TypedDict):
    source_key: int
    timer: Task


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
from type_hints import InputEvent, SourceItem, State, TargetState
import datetime
import aiofiles


NUM_SRC_KEYS = len(SOURCE_KEYS)


def get_time():
    return "{0:%Y-%m-%d %H:%M:%S}".format(datetime.datetime.now())


async def main():
    # state: State = {
    state = {
        "source": [
            {
                "source_key": src_key,
                "task": asyncio.create_task(dummy()),
            }
            for src_key in SOURCE_KEYS
        ],
        "target": {
            "value": TargetState.IDLE,
            "sources_down": {key: "up" for key in SOURCE_KEYS},
        },
    }

    # Just to make sure the dummy coroutines are done? -_- prolly not nes
    await asyncio.sleep(0.0001)

    ss = [s["task"].done() for s in state["source"]]
    write_stderr(f"[{get_time()}] {ss}" "\n")

    # while True:
    # I think because this is blocking the async Task can never finish!!
    # asyncio read? using `select` sys call? asyncio stream reader?
    # event_raw = sys.stdin.buffer.read(INPUT_EVENT_SIZE)

    async with aiofiles.open("/dev/stdin", "rb") as f:
        while True:
            event_raw = await f.read(INPUT_EVENT_SIZE)

            event = InputEvent._make(struct.unpack(STRUCT_FORMAT, event_raw))

            if event.type != TYPE["EV_KEY"]:
                write_raw_event(event_raw)
                continue

            # Handle non-source key
            if event.code not in SOURCE_KEYS:
                handle_non_source_key_event(event_raw, event, state)
                continue

            # Handle source key:
            if event.value == VALUE["PRESS"]:
                handle_source_key_press_event(event, state)
            elif event.value == VALUE["RELEASE"]:
                handle_source_key_release_event(event_raw, event, state)


def handle_non_source_key_event(
    event_raw: bytes, event: InputEvent, state: State
) -> None:
    if event.value == VALUE["PRESS"]:
        for source in state["source"]:
            if not source["task"].done():
                source["task"].cancel()
                write_key_event(source["source_key"], VALUE["PRESS"])
    write_raw_event(event_raw)


def handle_source_key_press_event(event: InputEvent, state: State) -> None:
    other_sources = (
        item for item in state["source"] if item["source_key"] != event.code
    )
    # Check if all source keys are currently pressed
    if all((not s["task"].done() for s in other_sources)):
        write_stderr(
            f"[{get_time()}]Not all sources are 'not done', "
            "i.e. down in threshold\n"
            f"\tsource key code: {event.code} \n"
            f"\ttask {[(i['source_key'],i['task'].done()) for i in state['source']]} \n"
            "\n"
        )
        for source in other_sources:
            source["task"].cancel()
        write_key_event(TARGET_KEY, VALUE["PRESS"], False)
        # Update target state:
        state["target"]["value"] = TargetState.PRESSED
        state["target"]["sources_down"] = {
            k: "down" for k in state["target"]["sources_down"]
        }
        return
    # Not all source keys are pressed yet (or at least not within threshold)
    inc_src_idx, inc_source = get_inc_source(event, state)
    write_stderr("Creating task")
    inc_source["task"] = asyncio.create_task(
        write_event_delayed(THRESHOLD_SEC, inc_source["source_key"])
    )
    write_stderr(f"\tCreated task {inc_source['task']}, {event.code}\n")
    state["source"].insert(
        NUM_SRC_KEYS - 1,
        state["source"].pop(inc_src_idx),
    )


def handle_source_key_release_event(
    event_raw: bytes, event: InputEvent, state: State
) -> None:
    if state["target"]["value"] == TargetState.PRESSED:
        write_key_event(TARGET_KEY, VALUE["RELEASE"], False)
        # Update target state:
        state["target"]["value"] = TargetState.RELEASED
        state["target"]["sources_down"][event.code] = "up"
    elif state["target"]["value"] == TargetState.RELEASED:
        state["target"]["sources_down"][event.code] = "up"
        # Only set target state to IDLE if all source keys are up again
        if all((v == "up" for v in state["target"]["sources_down"].values())):
            state["target"]["value"] = TargetState.IDLE
    else:
        _, inc_source = get_inc_source(event, state)
        # Could be releasing a source key before task expiry
        if not inc_source["task"].done():
            inc_source["task"].cancel()
            write_key_event(inc_source["source_key"], VALUE["PRESS"])
        write_raw_event(event_raw)


def get_inc_source(event: InputEvent, state: State) -> Tuple[int, SourceItem]:
    return next(
        item
        for item in enumerate(state["source"])
        if item[1]["source_key"] == event.code
    )


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


async def write_event_delayed(delay_sec: float, key: int) -> None:
    await asyncio.sleep(delay_sec)
    write_stderr(
        f"Writing delayed key: {key}. Task for this key should now be done!\n"
    )
    write_key_event(key, VALUE["PRESS"], True)


def write_stderr(msg: str):
    sys.stderr.buffer.write(msg.encode())
    sys.stderr.buffer.flush()


async def dummy():
    return


# if event.type != TYPE["EV_KEY"]:
#     write_raw_event(event_raw)
#     continue

# # Handle non-source key
# if event.code not in SOURCE_KEYS:
#     if event.value == VALUE["PRESS"]:
#         for source in sources_state:
#             if not source["timer"].done():
#                 source["timer"].cancel()
#                 write_key_event(source["source_key"], VALUE["PRESS"])
#     write_raw_event(event_raw)
#     continue

# # Incoming source key (next should always find the item here)
# # (returns reference to dict in list)
# inc_source = next(
#     item for item in sources_state if item["source_key"] == event.code
# )
# inc_src_idx = sources_state.index(inc_source)

# # Handle source key:
# if event.value == VALUE["PRESS"]:
#     other_sources = (
#         item for item in sources_state if item["source_key"] != event.code
#     )
#     if all((not s["timer"].done() for s in other_sources)):
#         for source in other_sources:
#             source["timer"].cancel()
#         write_key_event(TARGET_KEY, VALUE["PRESS"], False)
#         target_state: TargetState = TargetState.PRESSED
#         target_state_sources_down = {
#             k: "down" for k in target_state_sources_down
#         }
#     else:
#         inc_source["timer"] = asyncio.create_task(
#             write_event_delayed(THRESHOLD_SEC, inc_source["source_key"])
#         )
#         sources_state.insert(
#             num_source_keys - 1,
#             sources_state.pop(inc_src_idx),
#         )
# elif event.value == VALUE["RELEASE"]:
#     if target_state == TargetState.PRESSED:
#         write_key_event(TARGET_KEY, VALUE["RELEASE"], False)
#         target_state = TargetState.RELEASED
#         target_state_sources_down[event.code] = "up"
#     elif target_state == TargetState.RELEASED:
#         target_state_sources_down[event.code] = "up"
#         # Only set target state to IDLE if all source keys are up again
#         if all((v == "up" for v in target_state_sources_down.values())):
#             target_state = TargetState.IDLE
#     else:
#         # Could be releasing a source key (before task expiry)
#         if not inc_source["timer"].done():
#             inc_source["timer"].cancel()
#             write_key_event(inc_source["source_key"], VALUE["PRESS"])
#         write_raw_event(event_raw)


if __name__ == "__main__":
    asyncio.run(main())
