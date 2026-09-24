#ifndef PATHFINDERMINIEXTREME_025_SETTINGS_H
#define PATHFINDERMINIEXTREME_025_SETTINGS_H

// Loads runtime-wide options from their own Preferences namespace. These
// settings are intentionally separate from the character save format.
void loadGameSettings();
void saveGameSettings();

bool isEncoderRotationInverted();
void setEncoderRotationInverted(bool inverted);

#endif // PATHFINDERMINIEXTREME_025_SETTINGS_H
