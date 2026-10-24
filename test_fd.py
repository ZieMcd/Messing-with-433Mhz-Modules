import os
import lgpio


def show_fds(title):
    print(title)
    for fd in sorted(os.listdir("/proc/self/fd"), key=int):
        try:
            print(f"  fd {fd} -> {os.readlink(f'/proc/self/fd/{fd}')}")
        except OSError:
            pass


show_fds("before:")
h = lgpio.gpiochip_open(0)
print("lgpio handle:", h)
show_fds("after gpiochip_open:")
lgpio.gpio_claim_alert(h, 27, lgpio.BOTH_EDGES)
show_fds("after gpio_claim_alert:")
lgpio.gpiochip_close(h)
