#pragma once

enum GlobalSettingsAction {
  GLOBAL_SETTINGS_NONE,
  GLOBAL_SETTINGS_OPEN_SAVELOAD,
  GLOBAL_SETTINGS_WIFI_CONNECTED,
  GLOBAL_SETTINGS_OSC_SETTINGS_UPDATED,
  GLOBAL_SETTINGS_RETURN_TO_CONTROLLER
};

void beginGlobalSettings(void);
GlobalSettingsAction handleGlobalSettings(void);
