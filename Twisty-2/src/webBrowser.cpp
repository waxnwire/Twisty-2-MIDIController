#include "webBrowser.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Move all four function implementations here:
// isSlotJsonFile()
// handleFileManagerHome()
// handleFileDownload()
// registerFileManagerRoutes()

// ========================================
// TWISTY WIFI FILE MANAGER
// ========================================


static File uploadFile;

static const char* UPLOAD_TEMP = "/upload.tmp";
static const char* UPLOAD_BACKUP = "/upload.bak";

static String uploadTarget;
static String uploadResult = "No upload received";
static bool uploadError = false;
static size_t uploadBytes = 0;
const char* WIFI_SSID = "Bunk bed house";
const char* WIFI_PASSWORD = "Budd487Tiger";

WebServer fileServer(80);

// Fixed IP address
IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <title>Twisty 2</title>

    <style>
        body {
            background: #20242b;
            color: white;
            font-family: Arial;
            padding: 30px;
            text-align: center;
        }

        h1 {
            font-size: 36px;
        }

        .status {
            background: #303944;
            padding: 20px;
            border-radius: 12px;
            margin-top: 30px;
        }

        .green {
            color: #50d890;
        }
    </style>
</head>

<body>

    <h1>TWISTY 2</h1>

    <div class="status">
        <h2 class="green">Connected!</h2>
        <p>Wi-Fi file manager is running.</p>
        <p>Raspberry Pi Pico 2 W</p>
    </div>

</body>
</html>

)rawliteral";

void handleClinet() {
    fileServer.handleClient();
}
void scanWifiNetworks(){
    int networks = WiFi.scanNetworks();
    Serial.println("Available networks:");
    for (int i = 0; i < networks; i++) {
        Serial.print(WiFi.SSID(i));
        Serial.print("  RSSI: ");
        Serial.print(WiFi.RSSI(i));
        Serial.print("  Channel: ");
        Serial.println(WiFi.channel(i));
    }
}
void startWifiFileManager() {

    Serial.println("\nConnecting Twisty-2 to WiFi...");

    WiFi.mode(WIFI_STA);

    Serial.print("SSID: [");
    Serial.print(WIFI_SSID);
    Serial.println("]");

    Serial.print("SSID length: ");
    Serial.println(strlen(WIFI_SSID));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Wait up to 30 seconds
    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < 30000) {

        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());

        delay(1000);
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println("WIFI CONNECTION FAILED");

        Serial.print("Final status: ");
        Serial.println(WiFi.status());

        return;
    }

    Serial.println("WiFi connected!");

    Serial.print("Network IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI());

    registerFileManagerRoutes();

    fileServer.begin();

    Serial.println("HTTP server started");
}
bool isSlotJsonFile(const String& name) {

    if (!name.startsWith("slot") || !name.endsWith(".json")) {
        return false;
    }

    String number = name.substring(4, name.length() - 5);

    if (number.length() == 0) return false;

    for (unsigned int i = 0; i < number.length(); i++) {
        if (!isDigit(number[i])) return false;
    }

    return true;
}


// Browser homepage
void handleFileManagerHome() {

    String html;

    html += "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Twisty-2 File Manager</title>";

    html += "<style>";
    html += "body{font-family:Arial;margin:30px;background:#20242b;color:white;}";
    html += "h1{color:#70d6ff;}";
    html += ".file{background:#343b46;padding:15px;margin:10px 0;border-radius:8px;}";
    html += "a{color:#70d6ff;text-decoration:none;font-weight:bold;}";
    html += ".size{color:#aaa;font-size:13px;}";
    html += "</style></head><body>";

    html += "<h1>Twisty-2</h1>";
    html += "<h2>Settings files</h2>";
    html += "<div class='file'>";
    html += "<h3>Upload settings</h3>";
    html += "<form method='POST' action='/upload' ";
    html += "enctype='multipart/form-data'>";
    html += "<input type='file' name='file' accept='.json' required>";
    html += "<br><br>";
    html += "<button type='submit'>Upload to Twisty-2</button>";
    html += "</form>";
    html += "</div>";

    Dir dir = LittleFS.openDir("/");

    bool found = false;

    while (dir.next()) {

        String filename = dir.fileName();

        if (!isSlotJsonFile(filename)) continue;

        found = true;

        html += "<div class='file'>";
        html += filename;
        html += "<br><span class='size'>";
        html += String(dir.fileSize());
        html += " bytes</span><br><br>";

        html += "<a href='/download?name=";
        html += filename;
        html += "'>Download</a>";

        html += "</div>";
    }

    if (!found) {
        html += "<p>No settings files found.</p>";
    }

    html += "</body></html>";

    fileServer.send(200, "text/html", html);
}


// Download an individual JSON file
void handleFileDownload() {

    String filename = fileServer.arg("name");

    if (!isSlotJsonFile(filename)) {
        fileServer.send(400, "text/plain", "Invalid filename");
        return;
    }

    String path = "/" + filename;

    if (!LittleFS.exists(path)) {
        fileServer.send(404, "text/plain", "File not found");
        return;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        fileServer.send(500, "text/plain", "Could not open file");
        return;
    }

    fileServer.sendHeader(
        "Content-Disposition",
        "attachment; filename=\"" + filename + "\""
    );

    fileServer.streamFile(file, "application/json");

    file.close();
}


// Register browser routes
void registerFileManagerRoutes() {

    fileServer.on("/", HTTP_GET, handleFileManagerHome);

    fileServer.on("/download", HTTP_GET, handleFileDownload);

    fileServer.on(
    "/upload",
    HTTP_POST,

    // Called when the upload finishes
    []() {

        if (uploadError) {
            fileServer.send(400, "text/plain", uploadResult);
        }
        else if (uploadResult == "Upload in progress" ||
                 uploadResult == "No upload received") {
            fileServer.send(400, "text/plain", "Upload did not complete");
        }
        else {
            fileServer.send(200, "text/plain", uploadResult);
        }

        uploadResult = "No upload received";
    },

    // Receives the actual file data
    handleFileUpload
);
}
void handleFileUpload() {

    HTTPUpload& upload = fileServer.upload();

    if (upload.status == UPLOAD_FILE_START) {

        uploadError = false;
        uploadBytes = 0;
        uploadTarget = "";
        uploadResult = "Upload in progress";

        // Only accept slotN.json filenames
        if (!isSlotJsonFile(upload.filename)) {
            uploadError = true;
            uploadResult = "Invalid filename. Use slotN.json";
            return;
        }

        uploadTarget = "/" + upload.filename;

        // Remove any leftover temporary upload
        if (LittleFS.exists(UPLOAD_TEMP)) {
            LittleFS.remove(UPLOAD_TEMP);
        }

        uploadFile = LittleFS.open(UPLOAD_TEMP, "w");

        if (!uploadFile) {
            uploadError = true;
            uploadResult = "Could not create temporary file";
            return;
        }

        Serial.print("Receiving: ");
        Serial.println(upload.filename);
    }

    else if (upload.status == UPLOAD_FILE_WRITE) {

        if (!uploadError && uploadFile) {

            size_t written = uploadFile.write(
                upload.buf,
                upload.currentSize
            );

            if (written != upload.currentSize) {
                uploadError = true;
                uploadResult = "Filesystem write error";
            }
            else {
                uploadBytes += written;
            }
        }
    }

    else if (upload.status == UPLOAD_FILE_ABORTED) {

        uploadError = true;
        uploadResult = "Upload aborted";

        if (uploadFile) uploadFile.close();

        LittleFS.remove(UPLOAD_TEMP);
    }

    else if (upload.status == UPLOAD_FILE_END) {

        if (uploadFile) uploadFile.close();

        if (uploadError) {
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        if (uploadBytes == 0) {
            uploadError = true;
            uploadResult = "Empty file";
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        // Validate uploaded JSON before replacing anything
        File checkFile = LittleFS.open(UPLOAD_TEMP, "r");

        if (!checkFile) {
            uploadError = true;
            uploadResult = "Could not reopen uploaded file";
            return;
        }

        JsonDocument doc;

        DeserializationError error =
            deserializeJson(doc, checkFile);

        checkFile.close();

        if (error ||
            !doc["Version"].is<int>() ||
            !doc["page"].is<JsonArray>()) {

            uploadError = true;
            uploadResult = "Invalid Twisty JSON file";
            LittleFS.remove(UPLOAD_TEMP);
            return;
        }

        // Preserve the existing file until the new file is ready
        bool hadOriginal = LittleFS.exists(uploadTarget);

        if (LittleFS.exists(UPLOAD_BACKUP)) {
            LittleFS.remove(UPLOAD_BACKUP);
        }

        if (hadOriginal) {
            if (!LittleFS.rename(uploadTarget, UPLOAD_BACKUP)) {
                uploadError = true;
                uploadResult = "Could not back up existing preset";
                LittleFS.remove(UPLOAD_TEMP);
                return;
            }
        }

        // Move the validated upload into its final location
        if (!LittleFS.rename(UPLOAD_TEMP, uploadTarget)) {

            if (hadOriginal) {
                LittleFS.rename(UPLOAD_BACKUP, uploadTarget);
            }

            uploadError = true;
            uploadResult = "Could not install uploaded file";
            return;
        }

        // Successful replacement
        if (hadOriginal) {
            LittleFS.remove(UPLOAD_BACKUP);
        }

        uploadResult = "Successfully uploaded " +
                       uploadTarget.substring(1);

        Serial.println(uploadResult);
    }
}