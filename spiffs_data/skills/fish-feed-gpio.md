# Fish Feeding GPIO Schedule

Use this skill to control daily fish feeding by toggling GPIO4 on a fixed schedule.

## When to use
When the user asks for a daily 19:00 fish feeding action.

## How to use
1. Use get_current_time to get local date and time.
2. Determine whether it is time to feed:
   - target time is 19:00 local time every day
3. At 19:00, call gpio_control with pin=4 and action=on.
4. Keep GPIO4 high for 30 seconds.
5. After 30 seconds, call gpio_control with pin=4 and action=off.
6. If current time is not 19:00, report next trigger time and do not force GPIO4 on.

## Output guidance
- Report current time and whether the schedule condition is met.
- Report GPIO4 on/off execution status.
- If the user asks for immediate manual feeding, execute the same on-30s-off sequence immediately.

## Notes
- Any GPIO operation must be executed through gpio_control.
- This skill describes the feeding control logic; external scheduler/cron can be used to trigger this skill automatically each day.
