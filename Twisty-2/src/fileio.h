// Copyright 2023 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------


// file operations for Twisty 2
//  settings of encoders and switches saved and loaded in JSON format
// JSON allows adding features without breaking old settings
// using the LittleFFS filesystem in Pico Arduino 
// you have to set up an FFS partition in Arduino tools menu or file operations will fail

#define VERSION 101 // file format version in case it changes at some point

void writeConfigString(File &file, const char *value) {
  file.write('"');
  for (const char *character = value; *character != '\0'; ++character) {
    if (*character == '"' || *character == '\\') {
      file.write('\\');
    }
    file.write(static_cast<uint8_t>(*character));
  }
  file.write('"');
}

// save current settings to filesystem in JSON format

int16_t saveconfig(int16_t slot) {
  char filename[20];
  char tempfilename[24];
  char backupfilename[24];
  sprintf(filename,"slot%d.json",slot);
  sprintf(tempfilename,"slot%d.tmp",slot);
  sprintf(backupfilename,"slot%d.bak",slot);

  if (LittleFS.exists(tempfilename)) LittleFS.remove(tempfilename);

  File file = LittleFS.open(tempfilename, "w");
  if (!file) {
    Serial.println("file open failed");
    return 0;
  }

  file.printf("{ \"Version\" : %d ,\n",VERSION);
  file.printf("\"page\" :  [\n");

  for (int16_t p=0;p<CONTROLLER_PAGES;++p) {
    file.printf("  { \"control\" : [ \n");
    for (int16_t c=0; c< NUMENCODERS;c++) {
      file.printf("    {");
    //  file.printf("  { \"control\" : [ {");
      file.printf("\"EncoderType\":%d,",controls[p].encoder[c].type);
      file.printf("\"EncoderMode\":%d,",controls[p].encoder[c].mode);
      file.printf("\"EncoderChannel\":%d,",controls[p].encoder[c].channel);
      file.printf("\"EncoderCCNumber\":%d,",controls[p].encoder[c].ccnumber);
      file.printf("\"EncoderMinValue\":%d,",controls[p].encoder[c].minvalue);
      file.printf("\"EncoderMaxValue\":%d,",controls[p].encoder[c].maxvalue);
      file.printf("\"EncoderValue\":%d,",controls[p].encoder[c].value);
      file.printf("\"EncoderColorIndex\":%d,",controls[p].encoder[c].colorindex);
      file.printf("\"EncoderLabelIndex\":%d,",controls[p].encoder[c].labelindex);
      file.printf("\"EncoderOutput\":%d,",controls[p].encoder[c].output);
      file.printf("\"EncoderOSCValueType\":%d,",controls[p].encoder[c].oscValueType);
      file.printf("\"EncoderOSCRange\":%d,",controls[p].encoder[c].oscRange);
      file.printf("\"EncoderOSCValueMilli\":%d,",controls[p].encoder[c].oscValueMilli);
      file.printf("\"EncoderRandomWalkWidth\":%.3f,",controls[p].encoder[c].randomWalkWidth);
      file.print("\"EncoderOSCAddress\":");
      writeConfigString(file, controls[p].encoder[c].oscAddress);
      file.print(",");
      file.printf("\"SwitchMode\":%d,",controls[p].encswitch[c].mode);
      file.printf("\"SwitchFunction\":%d,",controls[p].encswitch[c].function);
      file.printf("\"SwitchType\":%d,",controls[p].encswitch[c].type);
      file.printf("\"SwitchChannel\":%d,",controls[p].encswitch[c].channel);
      file.printf("\"SwitchCCNumber\":%d,",controls[p].encswitch[c].ccnumber);
      file.printf("\"SwitchMinValue\":%d,",controls[p].encswitch[c].minvalue);
      file.printf("\"SwitchMaxValue\":%d,",controls[p].encswitch[c].maxvalue);
      file.printf("\"SwitchValue\":%d,",controls[p].encswitch[c].value);
      file.printf("\"SwitchColorIndex\":%d,",controls[p].encswitch[c].colorindex);
      file.printf("\"SwitchLabelIndex\":%d,",controls[p].encswitch[c].labelindex);
      file.printf("\"SwitchOutput\":%d,",controls[p].encoder[c].output);
      file.printf("\"SwitchOSCValueType\":%d,",controls[p].encswitch[c].oscValueType);
      file.print("\"SwitchOSCAddress\":");
      writeConfigString(file, controls[p].encswitch[c].oscAddress);
      if (c == (NUMENCODERS-1)) file.printf("}\n");
      else file.printf("},\n");
    }
    if (p == (CONTROLLER_PAGES-1)) file.printf("    ]\n  }\n");
    else file.printf("    ]\n  },\n");
  }
  file.printf("]\n}\n");
  file.flush();
  bool writeFailed = file.getWriteError() != 0;
  file.close();
  if (writeFailed) {
    LittleFS.remove(tempfilename);
    Serial.println("file write failed");
    return 0;
  }

  bool hadOriginal = LittleFS.exists(filename);
  if (LittleFS.exists(backupfilename)) LittleFS.remove(backupfilename);
  if (hadOriginal && !LittleFS.rename(filename, backupfilename)) {
    LittleFS.remove(tempfilename);
    Serial.println("file backup failed");
    return 0;
  }
  if (!LittleFS.rename(tempfilename, filename)) {
    if (hadOriginal) LittleFS.rename(backupfilename, filename);
    LittleFS.remove(tempfilename);
    Serial.println("file install failed");
    return 0;
  }
  if (hadOriginal) LittleFS.remove(backupfilename);
  return 1;
}


// read and deserialize JSON settings file

int16_t loadconfig(int16_t slot) {
  char filename[20];
  sprintf(filename,"slot%d.json",slot);

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("file open failed");
    return 0;
  }
  if (!file) {
    Serial.println("Failed to open file for reading");
    return 0;
  }

  // Allocate the JSON document 
  JsonDocument doc;

  // Deserialize JSON document from file
  DeserializationError error = deserializeJson(doc, file);

  file.close();

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return 0;
  } else {
    // Data has been deserialized successfully, restore settings 

    for (int16_t p=0;p<CONTROLLER_PAGES;++p) {
      for (int16_t c=0; c< NUMENCODERS;c++) {
        controls[p].encoder[c].mode=doc["page"][p]["control"][c]["EncoderMode"];
        controls[p].encoder[c].type=doc["page"][p]["control"][c]["EncoderType"] | CCTYPE;
        if (controls[p].encoder[c].type < CCTYPE || controls[p].encoder[c].type > DELTATYPE) {
          controls[p].encoder[c].type = CCTYPE;
        }
        controls[p].encoder[c].channel=doc["page"][p]["control"][c]["EncoderChannel"];
        controls[p].encoder[c].ccnumber=doc["page"][p]["control"][c]["EncoderCCNumber"];
        controls[p].encoder[c].minvalue=doc["page"][p]["control"][c]["EncoderMinValue"];
        controls[p].encoder[c].maxvalue=doc["page"][p]["control"][c]["EncoderMaxValue"];
        controls[p].encoder[c].value=doc["page"][p]["control"][c]["EncoderValue"];
        controls[p].encoder[c].colorindex=doc["page"][p]["control"][c]["EncoderColorIndex"];
        controls[p].encoder[c].labelindex=doc["page"][p]["control"][c]["EncoderLabelIndex"];
        controls[p].encoder[c].output=doc["page"][p]["control"][c]["EncoderOutput"] | MIDI_OUTPUT;
        controls[p].encoder[c].oscValueType=doc["page"][p]["control"][c]["EncoderOSCValueType"] | OSC_FLOAT;
        controls[p].encoder[c].oscRange=doc["page"][p]["control"][c]["EncoderOSCRange"] | OSC_RANGE_0_TO_1;
        controls[p].encoder[c].oscValueMilli=doc["page"][p]["control"][c]["EncoderOSCValueMilli"] | 500;
        controls[p].encoder[c].randomWalkWidth=doc["page"][p]["control"][c]["EncoderRandomWalkWidth"] | 0.0f;
        char defaultEncoderAddress[64];
        snprintf(defaultEncoderAddress, sizeof(defaultEncoderAddress),
                 "/twisty/page/%d/encoder/%d", p + 1, c + 1);
        const char *encoderAddress =
            doc["page"][p]["control"][c]["EncoderOSCAddress"] | defaultEncoderAddress;
        snprintf(controls[p].encoder[c].oscAddress,
                 sizeof(controls[p].encoder[c].oscAddress), "%s", encoderAddress);
        controls[p].encswitch[c].mode=doc["page"][p]["control"][c]["SwitchMode"];
        controls[p].encswitch[c].function =
            doc["page"][p]["control"][c]["SwitchFunction"] | SWITCH_JUMP;
        if (controls[p].encswitch[c].function < SWITCH_JUMP ||
            controls[p].encswitch[c].function > SWITCH_WOBBLE) {
          controls[p].encswitch[c].function = SWITCH_JUMP;
        }
        controls[p].encswitch[c].type=doc["page"][p]["control"][c]["SwitchType"];
        if (controls[p].encswitch[c].type < CCMESSAGE ||
            controls[p].encswitch[c].type > NOTEMESSAGE) {
          controls[p].encswitch[c].type = CCMESSAGE;
        }
        controls[p].encswitch[c].channel=doc["page"][p]["control"][c]["SwitchChannel"];
        controls[p].encswitch[c].ccnumber=doc["page"][p]["control"][c]["SwitchCCNumber"];
        controls[p].encswitch[c].minvalue=doc["page"][p]["control"][c]["SwitchMinValue"];
        controls[p].encswitch[c].maxvalue=doc["page"][p]["control"][c]["SwitchMaxValue"];
        controls[p].encswitch[c].value=doc["page"][p]["control"][c]["SwitchValue"];
        controls[p].encswitch[c].colorindex=doc["page"][p]["control"][c]["SwitchColorIndex"];
        controls[p].encswitch[c].labelindex=doc["page"][p]["control"][c]["SwitchLabelIndex"];
        controls[p].encswitch[c].output = controls[p].encoder[c].output;
        controls[p].encswitch[c].oscValueType=doc["page"][p]["control"][c]["SwitchOSCValueType"] | OSC_INTEGER;
        char defaultSwitchAddress[64];
        snprintf(defaultSwitchAddress, sizeof(defaultSwitchAddress),
                 "/twisty/page/%d/switch/%d", p + 1, c + 1);
        const char *switchAddress =
            doc["page"][p]["control"][c]["SwitchOSCAddress"] | defaultSwitchAddress;
        snprintf(controls[p].encswitch[c].oscAddress,
                 sizeof(controls[p].encswitch[c].oscAddress), "%s", switchAddress);
      }
    }
  }
  return 1;
}

/*
// debug code - show files and contents
int16_t loadconfig(int16_t slot) {
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
      Serial.println(dir.fileName());
      if(dir.fileSize()) {
          File file = dir.openFile("r");
          while(file.available()){
        // Read one byte at a time and print to Serial Monitor
            Serial.write(file.read());
          }
          file.close();
      }
      
  }
  return 1;
}
*/
