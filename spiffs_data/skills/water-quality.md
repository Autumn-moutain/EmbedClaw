# Water Quality Monitoring

Use this skill when the conversation is about water quality, water testing, turbidity, water ion concentration, or water temperature.

## When to use
When the user asks about water quality inspection, water quality analysis, water testing, or related sensor readings.

## How to use
1. First call ntu_test_adc to read the turbidity sensor result.
2. Then call water_ion_temp_uart to read ion concentration and temperature.
3. Treat ntu_test_adc as the turbidity source and water_ion_temp_uart as the ion/temperature source.
4. Combine both results into one water quality summary.
5. If either tool returns an error, report the failed sensor separately and still return the other valid result when available.
6. Do not guess water quality values without reading the tools first.

## Output guidance
- Use the NTU value from ntu_test_adc for turbidity judgment.
- Use ion_mg_l and temperature_c from water_ion_temp_uart for auxiliary water quality context.
- If the user asks for a plain conclusion, summarize whether the water looks clear, moderately turbid, or highly turbid, then include ion concentration and temperature if available.

## Notes
- Always prioritize actual sensor readings over assumptions.
- This skill is intended to make the agent automatically enable both tools whenever water quality is discussed.
