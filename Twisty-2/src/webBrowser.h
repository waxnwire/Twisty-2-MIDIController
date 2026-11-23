#pragma once

#include <WebServer.h>

extern WebServer fileServer;
void startWifiFileManager();
void handleClinet();
bool isSlotJsonFile(const String& name);
void handleFileManagerHome();
void handleFileDownload();
void handleFileUpload();
void registerFileManagerRoutes();