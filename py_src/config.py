import struct
from type_hints import InputEvent

################################################################################
# CONSTANTS
################################################################################
# input_event struct format:
# see: https://docs.python.org/3/library/struct.html#format-characters
STRUCT_FORMAT = "llHHI"
INPUT_EVENT = struct.Struct(STRUCT_FORMAT)
INPUT_EVENT_SIZE = struct.calcsize(STRUCT_FORMAT)

# Event types:
TYPE = {"EV_SYN": 0, "EV_KEY": 1}
EV_SYN = 0
EV_KEY = 1
EV_MSC = 4

# Event values (i.e. actions):
VALUE = {"RELEASE": 0, "PRESS": 1, "REPEAT": 2}
RELEASE = 0
PRESS = 1
REPEAT = 2

# Event key codes:
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

# SYN event
SYN_REPORT = 0
SYN_EVENT = InputEvent(0, 0, TYPE["EV_SYN"], SYN_REPORT, 0)

################################################################################
# CONFIG
################################################################################
SOURCE_KEYS = (KEY["Q"], KEY["X"])
TARGET_KEY = KEY["S"]
THRESHOLD_SEC = 0.15  # 0.05 sec == 50 ms
