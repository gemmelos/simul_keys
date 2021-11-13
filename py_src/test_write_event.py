import itertools
import struct
import sys
import time

# input_event struct format:
# FORMAT represents the format used by linux kernel input event struct
# See https://github.com/torvalds/linux/blob/v5.5-rc5/include/uapi/linux/input.h#L28
# Stands for: long int, long int, unsigned short, unsigned short, unsigned int
STRUCT_FORMAT = "llHHI"
INPUT_EVENT = struct.Struct(STRUCT_FORMAT)
EVENT_SIZE = struct.calcsize(STRUCT_FORMAT)

# Key type
EV_SYN = 0x00
EV_KEY = 0x01
EV_REL = 0x02
EV_ABS = 0x03
EV_MSC = 0x04
EV_SW = 0x05
EV_LED = 0x11
EV_SND = 0x12
EV_REP = 0x14
EV_FF = 0x15
EV_PWR = 0x16
EV_FF_STATUS = 0x17
EV_MAX = 0x1F

# input event values
VALUE_RELEASE = 0
VALUE_PRESS = 1
VALUE_REPEAT = 2

# input event key codes
KEY_X = 45
KEY_H = 35
KEY_J = 36
KEY_K = 37
KEY_L = 38


def main():
    key_value = VALUE_PRESS
    i = 0
    lst = [KEY_J, KEY_K, KEY_L]
    pool = itertools.cycle(lst)
    while True:
        key_code = next(pool)
        event = struct.pack(STRUCT_FORMAT, 0, 0, EV_KEY, key_code, key_value)
        sys.stdout.buffer.write(event)
        sys.stdout.buffer.flush()
        #
        time.sleep(0.28)  # 0.01 sec == 10 ms
        i += 1
    ##
    # event = struct.pack(FORMAT, 0, 0, EV_KEY, KEY_K, key_value)
    # sys.stdout.buffer.write(event)
    # sys.stdout.buffer.flush()
    ##
    # time.sleep(2.02)  # 0.01 sec == 10 ms


if __name__ == "__main__":
    main()
