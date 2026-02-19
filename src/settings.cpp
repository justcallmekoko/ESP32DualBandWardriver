#include "settings.h"

String Settings::getSettingsString() {
  return this->json_settings_string;
}

bool Settings::begin() {
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Logger::log(WARN_MSG, "Settings SPIFFS Mount Failed");
    return false;
  }

  File settingsFile;

  //SPIFFS.remove("/settings.json"); // NEED TO REMOVE THIS LINE

  if (SPIFFS.exists(WIFI_CONFIG)) {
    settingsFile = SPIFFS.open(WIFI_CONFIG, FILE_READ);
    
    if (!settingsFile) {
      settingsFile.close();
      Logger::log(WARN_MSG, "Could not find settings file");
      if (this->createDefaultSettings(SPIFFS))
        return true;
      else
        return false;    
    }
  }
  else {
    Logger::log(WARN_MSG, "Settings file does not exist");
    if (this->createDefaultSettings(SPIFFS))
      return true;
    else
      return false;
  }

  String json_string;
  DynamicJsonDocument jsonBuffer(2048);
  DeserializationError error = deserializeJson(jsonBuffer, settingsFile);
  serializeJson(jsonBuffer, json_string);

  this->json_settings_string = json_string;
  
  return true;
}

void Settings::wipeSPIFFS() {
  Logger::log(WARN_MSG, "[SPIFFS] Wiping filesystem...");

  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    Logger::log(WARN_MSG, "[SPIFFS] Failed to open root");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String path = String(file.name());

    Logger::log(WARN_MSG, "[SPIFFS] Deleting: " + (String)path);

    file.close();  // MUST close before removing

    if (SPIFFS.remove("/" + path)) {
      Logger::log(WARN_MSG, "  -> Deleted");
    } else {
      Logger::log(WARN_MSG, "  -> Delete failed");
    }

    file = root.openNextFile();
  }

  Logger::log(WARN_MSG, "[SPIFFS] Wipe complete.");
}

template <typename T>
T Settings::loadSettingMin(String name) {}

template<>
int Settings::loadSettingMin<int>(String name) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == name)
      return json["Settings"][i]["range"]["min"];
  }

  return 0;
}

template <typename T>
T Settings::loadSettingMax(String name) {}

template<>
int Settings::loadSettingMax<int>(String name) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == name)
      return json["Settings"][i]["range"]["max"];
  }

  return 0;
}

template <typename T>
T Settings::loadSetting(String key) {}

// Get type int settings
template<>
int Settings::loadSetting<int>(String key) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
  if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "Int", key))
    return 1;

  return 0;
}

// Get type string settings
template<>
String Settings::loadSetting<String>(String key) {
  //return this->json_settings_string;
  
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Serial.println("\nCould not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
  if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "String", key))
    return "";

  return "";
}

// Get type bool settings
template<>
bool Settings::loadSetting<bool>(String key) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    Logger::log(WARN_MSG, "Could not parse json to load");
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
  if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "bool", key))
    return true;

  return false;
}

//Get type uint8_t settings
template<>
uint8_t Settings::loadSetting<uint8_t>(String key) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["value"];
  }

  return 0;
}

template <typename T>
T Settings::saveSetting(String key, bool value) {}

template<>
bool Settings::saveSetting<bool>(String key, bool value) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    Logger::log(WARN_MSG, "Could not parse json to save");
  }

  String settings_string;

  bool found = false;

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      json["Settings"][i]["value"] = value;

      File settingsFile = SPIFFS.open(WIFI_CONFIG, FILE_WRITE);

      if (!settingsFile) {
        Logger::log(WARN_MSG, "Failed to create settings file");
        return false;
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }
    
      // Close the file
      settingsFile.close();
    
      this->json_settings_string = settings_string;
          
      return true;
    }
  }

  if (!found) {
    Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
    if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "bool", key))
      return true;
  }

  return false;
}

template <typename T>
T Settings::saveSetting(String key, int value, bool is_int) {}

template<>
bool Settings::saveSetting<bool>(String key, int value, bool is_int) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    Logger::log(WARN_MSG, "Could not parse json to save");
  }

  String settings_string;

  bool found = false;

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      json["Settings"][i]["value"] = value;

      File settingsFile = SPIFFS.open(WIFI_CONFIG, FILE_WRITE);

      if (!settingsFile) {
        Logger::log(WARN_MSG, "Failed to create settings file");
        return false;
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }
    
      // Close the file
      settingsFile.close();
    
      this->json_settings_string = settings_string;
          
      return true;
    }
  }

  if (!found) {
    Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
    if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "bool", key))
      return true;
  }
  return false;
}

template <typename T>
T Settings::saveSetting(String key, String value) {}

template<>
bool Settings::saveSetting<bool>(String key, String value) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  if (deserializeJson(json, this->json_settings_string)) {
    Logger::log(WARN_MSG, "Could not parse json to save");
  }

  String settings_string;

  bool found = false;

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      json["Settings"][i]["value"] = value;

      File settingsFile = SPIFFS.open(WIFI_CONFIG, FILE_WRITE);

      if (!settingsFile) {
        Logger::log(WARN_MSG, "Failed to create settings file");
        return false;
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }
    
      // Close the file
      settingsFile.close();
    
      this->json_settings_string = settings_string;
          
      return true;
    }
  }

  if (!found) {
    Logger::log(WARN_MSG, "Did not find setting named " + (String)key + ". Creating...");
    if (this->createDefaultSettings(SPIFFS, true, json["Settings"].size(), "bool", key))
      return true;
  }
  return false;
}

bool Settings::toggleSetting(String key) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key) {
      if (json["Settings"][i]["value"]) {
        saveSetting<bool>(key, false);
        Logger::log(STD_MSG, "Setting value to false");
        return false;
      }
      else {
        saveSetting<bool>(key, true);
        Logger::log(STD_MSG, "Setting value to true");
        return true;
      }

      return false;
    }
  }
}

String Settings::setting_index_to_name(int i) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  return json["Settings"][i]["name"];
}

int Settings::getNumberSettings() {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }

  return json["Settings"].size();
}

String Settings::getSettingType(String key) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, this->json_settings_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json: ");
    Serial.println(error.f_str());
  }
  
  for (int i = 0; i < json["Settings"].size(); i++) {
    if (json["Settings"][i]["name"].as<String>() == key)
      return json["Settings"][i]["type"];
  }

  return "";
}

void Settings::printJsonSettings(String json_string) {
  DynamicJsonDocument json(2048); // ArduinoJson v6

  DeserializationError error = deserializeJson(json, json_string);

  if (error) {
    Logger::log(WARN_MSG, "Could not parse json to print: ");
    Serial.println(error.f_str());
  }

  Logger::log(WARN_MSG, "DynamicJsonDocument Capacity: " + (String)json.capacity() + " vs json_string Len: " + (String)json_string.length());
  
  Serial.println("Settings\n----------------------------------------------");
  for (int i = 0; i < json["Settings"].size(); i++) {
    Serial.println("Name: " + json["Settings"][i]["name"].as<String>());
    Serial.println("Type: " + json["Settings"][i]["type"].as<String>());
    Serial.println("Value: " + json["Settings"][i]["value"].as<String>());
    Serial.println("----------------------------------------------");
  }
}

bool Settings::createDefaultSettings(fs::FS &fs, bool spec, uint8_t index, String typeStr, String name) {
  Logger::log(STD_MSG, "Creating default settings file: settings.json");

  if (!spec)
    this->wipeSPIFFS();
      
  File settingsFile = fs.open(WIFI_CONFIG, FILE_WRITE);

  if (!settingsFile) {
    Logger::log(WARN_MSG, "Failed to create settings file");
    return false;
  }

  String settings_string;

  if (!spec) {

    DynamicJsonDocument jsonBuffer(1024);

    jsonBuffer["Settings"][0]["name"] = "SavePCAP";
    jsonBuffer["Settings"][0]["type"] = "bool";
    jsonBuffer["Settings"][0]["value"] = true;
    jsonBuffer["Settings"][0]["range"]["min"] = false;
    jsonBuffer["Settings"][0]["range"]["max"] = true;

    jsonBuffer["Settings"][1]["name"] = "UpdateFile";
    jsonBuffer["Settings"][1]["type"] = "String";
    jsonBuffer["Settings"][1]["value"] = "";
    jsonBuffer["Settings"][1]["range"]["min"] = "";
    jsonBuffer["Settings"][1]["range"]["max"] = "";

    jsonBuffer["Settings"][2]["name"] = "e";
    jsonBuffer["Settings"][2]["type"] = "bool";
    jsonBuffer["Settings"][2]["value"] = false;
    jsonBuffer["Settings"][2]["range"]["min"] = false;
    jsonBuffer["Settings"][2]["range"]["max"] = true;

    jsonBuffer["Settings"][3]["name"] = "m";
    jsonBuffer["Settings"][3]["type"] = "Int";
    jsonBuffer["Settings"][3]["value"] = 1;
    jsonBuffer["Settings"][3]["range"]["min"] = 1;
    jsonBuffer["Settings"][3]["range"]["max"] = 10;

    jsonBuffer["Settings"][4]["name"] = "ek";
    jsonBuffer["Settings"][4]["type"] = "String";
    jsonBuffer["Settings"][4]["value"] = "";
    jsonBuffer["Settings"][4]["range"]["min"] = "";
    jsonBuffer["Settings"][4]["range"]["max"] = "";

    jsonBuffer["Settings"][5]["name"] = "wu";
    jsonBuffer["Settings"][5]["type"] = "String";
    jsonBuffer["Settings"][5]["value"] = "";
    jsonBuffer["Settings"][5]["range"]["min"] = "";
    jsonBuffer["Settings"][5]["range"]["max"] = "";

    jsonBuffer["Settings"][6]["name"] = "wt";
    jsonBuffer["Settings"][6]["type"] = "String";
    jsonBuffer["Settings"][6]["value"] = "";
    jsonBuffer["Settings"][6]["range"]["min"] = "";
    jsonBuffer["Settings"][6]["range"]["max"] = "";

    jsonBuffer["Settings"][7]["name"] = "p";
    jsonBuffer["Settings"][7]["type"] = "String";
    jsonBuffer["Settings"][7]["value"] = "";
    jsonBuffer["Settings"][7]["range"]["min"] = "";
    jsonBuffer["Settings"][7]["range"]["max"] = "";

    jsonBuffer["Settings"][8]["name"] = "s";
    jsonBuffer["Settings"][8]["type"] = "String";
    jsonBuffer["Settings"][8]["value"] = "";
    jsonBuffer["Settings"][8]["range"]["min"] = "";
    jsonBuffer["Settings"][8]["range"]["max"] = "";

    if (serializeJson(jsonBuffer, settingsFile) == 0) {
      Logger::log(WARN_MSG, "Failed to write to file");
    }
    if (serializeJson(jsonBuffer, settings_string) == 0) {
      Logger::log(WARN_MSG, "Failed to write to string");
    }
  }

  else {
    DynamicJsonDocument json(2048); // ArduinoJson v6

    if (deserializeJson(json, this->json_settings_string)) {
      Logger::log(WARN_MSG, "Could not parse json to create new setting");
      return false;
    }

    if (typeStr == "bool") {
      Logger::log(WARN_MSG, "Creating bool setting...");
      json["Settings"][index]["name"] = name;
      json["Settings"][index]["type"] = typeStr;
      json["Settings"][index]["value"] = false;
      json["Settings"][index]["range"]["min"] = false;
      json["Settings"][index]["range"]["max"] = true;

      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
    }
    else if (typeStr == "String") {
      Logger::log(WARN_MSG, "Creating String setting...");
      json["Settings"][index]["name"] = name;
      json["Settings"][index]["type"] = typeStr;
      json["Settings"][index]["value"] = "";
      json["Settings"][index]["range"]["min"] = "";
      json["Settings"][index]["range"]["max"] = "";

      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
    }
    else if (typeStr == "Int") {
      Logger::log(WARN_MSG, "Creating Int setting...");
      json["Settings"][index]["name"] = name;
      json["Settings"][index]["type"] = typeStr;
      json["Settings"][index]["value"] = 1;
      json["Settings"][index]["range"]["min"] = 1;
      json["Settings"][index]["range"]["max"] = 10;

      if (serializeJson(json, settings_string) == 0) {
        Logger::log(WARN_MSG, "Failed to write to string");
      }

      if (serializeJson(json, settingsFile) == 0) {
        Logger::log(WARN_MSG, "Failed to write to file");
      }
    }
  }

  // Close the file
  settingsFile.close();

  if (settings_string != "") {
    this->json_settings_string = settings_string;

    if (!spec)
      this->printJsonSettings(settings_string);
  }

  return true;
}

void Settings::main(uint32_t currentTime) {
  
}
