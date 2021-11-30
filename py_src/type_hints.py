from asyncio.tasks import Task
from typing import Dict, List, Literal, NamedTuple, TypedDict
from enum import Enum, auto


class TargetState(Enum):
    IDLE = auto()
    PRESSED = auto()
    RELEASED = auto()


class SourceItem(TypedDict):
    source_key: int
    task: Task


class InputEvent(NamedTuple):
    sec: int
    usec: int
    type: int
    code: int
    value: int


class TargetStateMap(TypedDict):
    value: TargetState
    sources_down: Dict[int, Literal["down", "up"]]


class State(TypedDict):
    source: List[SourceItem]
    target: TargetStateMap
