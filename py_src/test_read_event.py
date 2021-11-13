import struct
import sys
import time

STRUCT_FORMAT = "llHHI"
INPUT_EVENT = struct.Struct(STRUCT_FORMAT)


def main():
    while True:
        data = sys.stdin.buffer.read(INPUT_EVENT.size)
        try:
            event_in = INPUT_EVENT.unpack(data)
            print(f"Event_in: {event_in}", flush=True)
        except struct.error as e:
            print(f"struct.error: {data}\nerror: {e}", flush=True)
            time.sleep(1)
        print("*" * 30)


if __name__ == "__main__":
    main()
