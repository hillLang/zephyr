#!/usr/bin/env python3
import os
import sys

try:
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(99))
except:
    print("Warning: could not set SCHED_FIFO realtime scheduling on qemu process.")
    print("Tests on loaded machines may be unreliable when run without")
    print("CAP_SYS_NICE capability.")

os.execv(sys.argv[1], sys.argv[1:])
