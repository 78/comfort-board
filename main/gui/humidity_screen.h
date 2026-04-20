#pragma once

namespace gui {

// Build the static comfort-board layout on the active screen. Safe to call
// exactly once after InitLvglDisplay().
void CreateHumidityScreen();

// Push new sensor values into the widgets and re-apply level-based colors.
// Values are whole degrees Celsius / whole percent RH (already truncated
// by the caller). Pass sensor_ok=false to render a red "--" fault state.
void UpdateHumidityScreen(int temp_c, int humidity_pct, bool sensor_ok);

}  // namespace gui
