# Light Auto Adjust

Automatically test ambient light with BH1750 and adjust the fill light.

## When to use
When the user asks to test light intensity, measure ambient brightness, or automatically tune the fill light.

## How to use
1. First call `read_light_intensity` to read the current lux value from the BH1750 sensor.
2. Use the returned lux value as the only source of truth. Do not guess brightness without reading the sensor first.
3. Map lux to brightness with this rule:
   - `< 50 lx` -> `100`
   - `50-200 lx` -> `80`
   - `200-500 lx` -> `60`
   - `500-1000 lx` -> `40`
   - `> 1000 lx` -> `10`
4. Then call `fill_light` with the computed `brightness` value.
5. Wait until the tool returns `{"status": "success"}` before giving the final response to the user.

## Notes
- Always run the BH1750 reading step before controlling the fill light.
- Use `fill_light` as the actual tool name; do not invent alternate names.
- This workflow is intended for automatic light testing and LEDC-driven brightness control.