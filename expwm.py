import time
import os

chip = "0"      # change to match your pwmchip (0 or 2)
channel = "0"

base = f"/sys/class/pwm/pwmchip{chip}"
pwm = f"{base}/pwm{channel}"

# Export (ignore error if already exported)
os.system(f"echo {channel} > {base}/export")

time.sleep(0.1)  # allow system to create files

# Set 1 kHz PWM
period_ns = 1000000     # 1,000,000 ns = 1 kHz
duty_ns   = 990000      # 50% duty

with open(f"{pwm}/period", "w") as f:
    f.write(str(period_ns))

with open(f"{pwm}/duty_cycle", "w") as f:
    f.write(str(duty_ns))

with open(f"{pwm}/enable", "w") as f:
    f.write("1")

print("PWM running at 1 kHz, 50% duty for 5 seconds...")
time.sleep(5)

# Stop PWM
with open(f"{pwm}/enable", "w") as f:
    f.write("0")
