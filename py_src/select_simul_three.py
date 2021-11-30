from enum import Enum, auto
from os import read
import select
import struct
import sys
from typing import List, Tuple, Optional
from dataclasses import dataclass

from config import (
    EV_MSC,
    INPUT_EVENT,
    PRESS,
    SOURCE_KEYS,
    STRUCT_FORMAT,
    THRESHOLD_SEC,
    EV_SYN,
    EV_KEY,
)
from type_hints import InputEvent


# def maybe():
#     buffered_events = []
#     timeout = None
#     while True:
#         write_list: List[InputEvent] = []
#         read_result: Tuple[bool, InputEvent] = read_event(
#             timeout, buffered_events
#         )
#         select_timed_out = read_result[0]
#         key_event = read_result[1]
#         if select_timed_out:
#             # Write any buffered events
#             for buf_ev in buffered_events:
#                 sys.stdout.buffer.write(buf_ev)
#             sys.stdout.buffer.flush()
#         elif key_event.code not in SOURCE_KEYS:
#             write_list.append(key_event)
#         elif key_event.value == PRESS:
#             buffered_events.append(key_event)
#             timeout = THRESHOLD_SEC
#         for event in write_list:
#             sys.stdout.buffer.write(INPUT_EVENT.pack(*event))
#         sys.stdout.buffer.flush()


class EventPackage:
    """
    Sequence consisting of raw read `input_event`s in bytes
    In addition we store the key event as an unpacked NamedTuple because we
    will need to access it.

    Most likely not the most efficient, but let's worry about that later
    Let's get something working first, something I understand!
    """

    def __init__(self) -> None:
        self._raw_event_package: List[bytes] = []
        self._key_event_index: int = -1
        self.key_event: Optional[InputEvent] = None

    def add_raw_event(self, raw_event: bytes):
        self._raw_event_package.append(raw_event)

    def add_key_event(self, key_event: InputEvent, raw_key_event: bytes):
        self._key_event_index = len(self._raw_event_package)
        self._raw_event_package.append(raw_key_event)
        self.key_event = key_event

    def write_event_package(self):
        for event in self._raw_event_package:
            sys.stdout.buffer.write(event)
        sys.stdout.buffer.flush()


def read_event_package(should_timeout: bool = False) -> Optional[EventPackage]:
    event: InputEvent = InputEvent(-1, -1, -1, -1, -1)
    event_package = EventPackage()

    print(
        f"Starting select call, should_timeout={should_timeout}",
        flush=True,
        file=sys.stderr,
    )
    rlist, _, _ = select.select(
        [sys.stdin],
        [],
        [],
        THRESHOLD_SEC if should_timeout else None,
    )
    print(f"Just after select, rlist={rlist}", flush=True, file=sys.stderr)

    if not rlist:
        print("Empty rlist", flush=True, file=sys.stderr)
        # Timeout reached and stdin didn't receive anything
        # So we didn't receive any event, we should write our buffered events
        # return event_package
        return None

    # Decided to include EV_SYN in the event package because of this:
    # https://gitlab.com/interception/linux/tools#correct-synthesization-of-event-sequences
    while event.type != EV_SYN:
        raw_event = sys.stdin.buffer.read(INPUT_EVENT.size)
        event = InputEvent._make(struct.unpack(STRUCT_FORMAT, raw_event))

        if event.type == EV_MSC:
            # As done in plugin from interception tools author
            # see: https://gitlab.com/interception/linux/plugins/caps2esc/-/blob/master/caps2esc.c#L93-94
            # and: https://gitlab.com/interception/linux/plugins/caps2esc/-/commit/bb09cd8d9a3f04463df5
            continue

        if event.type == EV_KEY:
            print("type ev key", flush=True, file=sys.stderr)
            event_package.add_key_event(event, raw_event)
        else:
            print("type NOT ev key", flush=True, file=sys.stderr)
            event_package.add_raw_event(raw_event)

    print("return event packagek", flush=True, file=sys.stderr)
    return event_package


def main():
    state: State = State.IDLE

    buffered_event_packages: List[EventPackage] = []

    event_packages_to_write: List[EventPackage] = []

    should_timeout = False

    while True:
        event_package = read_event_package(should_timeout)
        print(
            f"done reading event package = {event_package}",
            flush=True,
            file=sys.stderr,
        )

        if event_package is None:
            event_packages_to_write += buffered_event_packages

        elif state == State.IDLE:
            if (
                event_package is not None
                and event_package.key_event.code in SOURCE_KEYS
            ):
                print(
                    "in state idle && key in source keys",
                    flush=True,
                    file=sys.stderr,
                )
                buffered_event_packages.append(event_package)
                event_packages_to_write = []
                should_timeout = True

        # For simplicity this should be the only place to actually write
        # events to stdout.
        # (hopefully makes the code easier to understand, reason about and test)
        # (will revisit if it has a considerable negative performance impact or
        # makes writing the actual logic really awkward to write)
        for ep in event_packages_to_write:
            print("writing event package", flush=True, file=sys.stderr)
            ep.write_event_package()


class State(Enum):
    IDLE = auto()


# def write_event_packages(event_packages: List[EventPackage]):
#     for ep in event_packages:
#         ep.write_event_package()


if __name__ == "__main__":
    main()
