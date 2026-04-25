# Water Filter GPIO Control

Use this skill to combine water quality detection and GPIO-based filtration control.

## When to use
When the user asks to auto-enable filtration after water ion concentration and turbidity rise.
This skill should be used together with Water Quality Monitoring.

## How to use
1. First run the Water Quality Monitoring workflow:
   - call ntu_test_adc for turbidity
   - call water_ion_temp_uart for ion concentration and temperature
2. Get a baseline reading and a current reading:
   - if historical readings are available, use the latest previous reading as baseline
   - otherwise take two readings with a short interval (for example 30-60 seconds)
3. Determine whether both indicators increased:
   - turbidity increased: current_ntu > baseline_ntu
   - ion concentration increased: current_ion_mg_l > baseline_ion_mg_l
4. If both increased, call gpio_control with pin=1 and action=on.
5. Keep GPIO1 high for 1 hour.
6. After 1 hour, call gpio_control with pin=1 and action=off.
7. If either increase condition is not met, do not enable GPIO1 and explain why.

## Output guidance
- Clearly report baseline vs current values for NTU and ion concentration.
- Explicitly state whether the trigger condition was met.
- Report GPIO1 action result for both on and off operations.
- If GPIO control fails, return the tool error and keep water quality conclusion.

## Notes
- Always use real tool readings. Do not guess values.
- Any GPIO operation must be executed through gpio_control.
