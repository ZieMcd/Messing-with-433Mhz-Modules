import sys
import time
import lgpio

GPIO = 27
SECONDS = float(sys.argv[1]) if len(sys.argv) > 1 else 5

def open_rf1():
    for n in (0,4):
        try:
            h = lgpio.gpiochip_open(n)
        except lgpio.error:
            continue
                    #
        if "rp1" in lgpio.gpio_get_chip_info(h)[3]:
            return h
        lgpio.gpiochip_close(h)
    sys.exit("rp1 GPIO pin not found")

def map_pluses(edges):
    pulses = []

    for i in range(1, len(edges)):
        prev_level, prev_tick = edges[i-1]
        level, tick = edges[i]

        #if level is 1/High that means it swapped from low to High, so "current" level is low
        if level == 1:
            kind = "L"
        else:
            kind = "H"

        duration_ns = tick - prev_tick
        duration_us = duration_ns // 1000

        pulses.append((kind, duration_us))

    return pulses


h = open_rf1()

lgpio.gpio_claim_alert(h, GPIO, lgpio.BOTH_EDGES)
edges = []
cb = lgpio.callback(h, GPIO, lgpio.BOTH_EDGES, lambda chip, gpio, level,tick: edges.append((level, tick)));

print(f"capturing GPIO {GPIO} for {SECONDS}s....")
time.sleep(SECONDS)
cb.cancel()
lgpio.gpiochip_close(h)

pulses = map_pluses(edges)

with open("capture.txt", "w") as f:
    f.writelines(f"{kind} : {us}us\n" for kind, us in pulses)


print(f"{len(edges)} pulses written to capture.txt")

for label, lo, hi in [("<100us", 0, 100), ("100-200us", 100, 200), ("200-500us", 200, 500),
                      ("0.5-1.ms", 500, 1500), ("1.5ms-5ms", 1500, 5000), (">5ms", 5000, 1 << 62)]:
    n = sum(lo <= us < hi  for _, us in pulses)
    print(f"{label:>10} {n:7d}  {'#' * (50 * n // max(len(pulses), 1))}")
