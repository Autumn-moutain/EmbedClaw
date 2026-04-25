# Oxygen Pump GPIO Control

Use this skill to drive oxygen pumping when water temperature shows a slight rise.

## When to use
When the user asks to pump oxygen if water temperature rises slightly.

## How to use
1. Call water_ion_temp_uart to read the current temperature.
2. Get baseline and current temperature:
   - if historical temperature exists, use the latest previous value as baseline
   - otherwise take two measurements with a short interval (for example 30-60 seconds)
3. Evaluate slight rise with this default rule:
   - slight rise means delta_t is between 0.5 C and 2.0 C (inclusive)
   - delta_t = current_temperature_c - baseline_temperature_c
4. If slight rise is detected, call gpio_control with pin=2 and action=on.
5. Keep GPIO2 high for 10 minutes.
6. After 10 minutes, call gpio_control with pin=2 and action=off.
7. If delta_t is outside the slight-rise range, keep GPIO2 unchanged and explain the decision.

## Output guidance
- Report baseline temperature, current temperature, and delta_t.
- State whether the slight-rise condition was met.
- Report GPIO2 on/off execution status.

## Notes
- Always use measured temperature from water_ion_temp_uart.
- Any GPIO operation must be executed through gpio_control.
