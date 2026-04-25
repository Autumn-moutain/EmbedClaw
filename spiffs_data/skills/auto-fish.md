# Auto Fish Keeping

Use this skill when the user says auto fish keeping, automatic fish tank care, or mentions "自动养鱼".

## Fish tank profile
- Tank type: small tank
- Size: 35 x 20 x 22 cm
- Fish stock:
  - 3 goldfish
  - 3 black-tailed fish (black tail hook type)
  - 3 small fish

## Available existing skills to reuse
- water-quality
- water-filter-gpio
- oxygen-pump-gpio
- light-auto-adjust
- fish-feed-gpio

## Available tools for this orchestration
- get_current_time
- ntu_test_adc
- water_ion_temp_uart
- read_light_intensity
- fill_light
- gpio_control

## When to use
When the user asks for full automatic fish tank management, including feeding, water quality control, oxygen pumping, light compensation, and scheduled status reports.

## How to use
1. Treat this skill as a coordinator that reuses the existing fish-related workflows.
2. When the user starts automatic fish keeping, always run the startup sequence first:
   - Run oxygen pump logic for 60 seconds.
3. Every run, first call get_current_time and decide which scheduled jobs should run now.
4. Run feeding workflow:
   - At 19:00 every day, execute fish feeding logic.
   - Every feeding action must keep the feeding GPIO high for exactly 30 seconds.
   - After 30 seconds, immediately drive the corresponding feeding GPIO low.
5. Run water quality and filtration workflow:
   - Read water quality (NTU, ion, temperature).
   - If turbidity and ion concentration both rise, execute filtration logic (GPIO1 on for 1 hour, then off).
6. Run oxygen pump workflow:
   - Compare current temperature with baseline.
   - If temperature rises slightly, execute oxygen pump logic (GPIO2 on for 10 minutes, then off).
7. Run lighting workflow:
   - Read BH1750 lux via read_light_intensity.
   - Call fill_light using the existing lux-to-brightness mapping from light-auto-adjust.
8. Run background status polling every 10 minutes:
   - Every 10 minutes, read core status data (at least light intensity; also include NTU/ion/temperature if tools are available in that cycle).
   - Do not send user-facing messages for this 10-minute polling cycle.
   - Keep results as internal state/history for later trend judgment in scheduled reports.
   - If a critical threshold breach is detected, allow control actions (for safety) but still do not send routine polling messages.
9. Run scheduled tank reports:
    - Report automatically at 10:00, 14:00, and 20:00 every day.
    - Scheduled report timing remains unchanged; do not replace or shift these report times with 10-minute polling.
    - Each report should include:
       - current time
       - NTU (turbidity)
       - ion_mg_l
       - temperature_c
       - light lux and fill-light brightness decision
       - GPIO1/GPIO2/GPIO4 status if available
       - whether feeding/filtering/oxygen actions were triggered in this cycle
       - intelligent water-condition summary (\"water status\") with one-line conclusion and reasons
       - intelligent fish-condition summary (\"fish status\") with one-line conclusion and reasons
    - Summary generation rules for every scheduled report:
       - Water status must be one of: stable, watch, needs attention.
       - Water status should be derived from trend + current readings (NTU, ion_mg_l, temperature_c), not from a single metric only.
       - Fish status must be one of: normal, mild stress risk, high stress risk.
       - Fish status should be inferred from water status, temperature change, dissolved-ion/turbidity changes, and whether feeding/filtering/oxygen actions were recently triggered.
       - If direct fish-behavior sensing is unavailable, explicitly mark fish status as inferred and include confidence (high/medium/low).
       - Keep each summary concise: conclusion first, then 2-4 key evidence points.

## Output guidance
- If the user asks for plain conclusion, summarize tank status as stable or needs attention.
- If any tool fails, report the failed part explicitly and continue with other available checks.
- Always separate observed values from control actions.
- 10-minute background polling is silent by default (no routine message output).
- For scheduled reports, always output this structure in order:
   - Observed values
   - Triggered control actions
   - Water status summary (conclusion + evidence)
   - Fish status summary (conclusion + evidence + confidence)

## Notes
- Always prefer real sensor/tool results; do not guess values.
- Any GPIO operation must be executed through gpio_control.
- This skill coordinates existing capabilities; do not replace the existing workflows with invented logic.
- If no external scheduler is configured, execute due tasks whenever the user triggers this skill and the current time matches scheduled windows.
