#include "Settings.h"

#include <Shlwapi.h>
#include <algorithm>

#include "HotkeyActions.h"
#include "Logger.h"
#include "Skin.h"
#include "StringUtils.h"

#define XML_AUDIODEV "audioDeviceID"
#define XML_LANGUAGE "language"
#define XML_NOTIFYICON "notifyIcon"
#define XML_OSD_OFFSET "osdEdgeOffset"
#define XML_OSD_POS "osdPosition"
#define XML_OSD_X "osdX"
#define XML_OSD_Y "osdY"
#define XML_SOUNDS "soundEffects"

std::wstring Settings::_appDir(L"");
Settings *Settings::instance;

Settings::Settings() {

}

Settings *Settings::Instance() {
    if (instance == NULL) {
        instance = new Settings();
        instance->Reload();
    }
    return instance;
}

void Settings::Reload() {
    _file = AppDir() + L"\\Settings.xml";
    CLOG(L"Loading settings file: %s", _file.c_str());

    std::string u8FileName = StringUtils::Narrow(_file);
    tinyxml2::XMLError result = _xml.LoadFile(u8FileName.c_str());
    if (result != tinyxml2::XMLError::XML_SUCCESS) {
        throw std::runtime_error("Failed to parse XML file");
    }

    _root = _xml.GetDocument()->FirstChildElement("settings");
    if (_root == NULL) {
        throw std::runtime_error("Could not find root XML element");
    }

    _skin = new Skin(SkinXML());
}

int Settings::Save() {
    FILE *stream;
    errno_t err = _wfopen_s(&stream, _file.c_str(), L"w");
    if (err != 0) {
        CLOG(L"Could not open settings file for writing!");
        return 100 + err;
    }
    tinyxml2::XMLError result = _xml.SaveFile(stream);
    fclose(stream);
    return result;
}

std::wstring Settings::AppDir() {
    if (_appDir.empty()) {
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        PathRemoveFileSpec(path);
        _appDir = std::wstring(path);
    }
    return _appDir;
}

std::wstring Settings::SettingsApp() {
    return Settings::AppDir() + L"\\" + SETTINGS_APP;
}

std::wstring Settings::LanguagesDir() {
    return AppDir() + L"\\" + LANG_DIR;
}


std::wstring Settings::AudioDeviceID() {
    return GetText(XML_AUDIODEV);
}

std::wstring Settings::LanguageName() {
    std::wstring lang = GetText(XML_LANGUAGE);

    if (lang == L"") {
        return DEFAULT_LANGUAGE;
    } else {
        return lang;
    }
}

int Settings::OSDEdgeOffset() {
    if (HasSetting(XML_OSD_OFFSET)) {
        return GetInt(XML_OSD_OFFSET);
    } else {
        return DEFAULT_OSD_OFFSET;
    }
}

Settings::OSDPos Settings::OSDPosition() {
    std::wstring pos = GetText(XML_OSD_POS);
    if (pos == L"") {
        return DEFAULT_OSD_POS;
    }

    std::transform(pos.begin(), pos.end(), pos.begin(), ::tolower);
    if (pos == L"top") {
        return Top;
    } else if (pos == L"bottom") {
        return Bottom;
    } else if (pos == L"left") {
        return Left;
    } else if (pos == L"right") {
        return Right;
    } else if (pos == L"center") {
        return Center;
    } else if (pos == L"custom") {
        return Custom;
    }

    /* Default: */
    return DEFAULT_OSD_POS;
}

int Settings::OSDX() {
    return GetInt(XML_OSD_X);
}

int Settings::OSDY() {
    return GetInt(XML_OSD_Y);
}

Skin *Settings::CurrentSkin() {
    return _skin;
}

std::wstring Settings::SkinName() {
    std::wstring name = GetText("skin");

    if (name == L"") {
        return DEFAULT_SKIN;
    } else {
        return name;
    }
}

std::wstring Settings::SkinXML() {
    return SkinXML(SkinName());
}

std::wstring Settings::SkinXML(std::wstring skinName) {
    std::wstring skinXML = Settings::AppDir() + L"\\" + SKINS_DIR L"\\"
        + skinName + L"\\" SKIN_XML;
    return skinXML;
}

std::unordered_map<int, int> Settings::Hotkeys() {
    std::unordered_map<int, int> keyMappings;

    tinyxml2::XMLElement *hotkeys = _root->FirstChildElement("hotkeys");
    tinyxml2::XMLElement *hotkey = hotkeys->FirstChildElement("hotkey");

    for (; hotkey != NULL; hotkey = hotkey->NextSiblingElement()) {
        int action = -1;
        hotkey->QueryIntAttribute("action", &action);
        if (action == -1) {
            CLOG(L"No action provided for hotkey; skipping");
            continue;
        }

        int combination = -1;
        hotkey->QueryIntAttribute("combination", &combination);
        if (combination == -1) {
            CLOG(L"No key combination provided for hotkey; skipping");
            continue;
        }
        
        /* Whew, we made it! */
        CLOG(L"Adding hotkey mapping: %d -> %d", combination, action);
        keyMappings[combination] = action;
    }

    return keyMappings;
}

bool Settings::NotifyIconEnabled() {
    return GetEnabled(XML_NOTIFYICON);
}

void Settings::NotifyIconEnabled(bool enable) {
    SetEnabled(XML_NOTIFYICON, enable);
}

bool Settings::SoundEffectsEnabled() {
    return GetEnabled(XML_SOUNDS);
}

bool Settings::HasSetting(std::string elementName) {
    tinyxml2::XMLElement *el = _root->FirstChildElement(elementName.c_str());
    return (el != NULL);
}

bool Settings::GetEnabled(std::string elementName) {
    tinyxml2::XMLElement *el = _root->FirstChildElement(elementName.c_str());
    if (el == NULL) {
        CLOG(L"Warning: XML element '%s' not found", elementName.c_str());
        return false;
    } else {
        bool val = false;
        el->QueryBoolText(&val);
        return val;
    }
}

void Settings::SetEnabled(std::string elementName, bool enabled) {
    tinyxml2::XMLElement *el = GetOrCreateElement(elementName);
    el->SetText(enabled ? "true" : "false");
}

std::wstring Settings::GetText(std::string elementName) {
    tinyxml2::XMLElement *el = _root->FirstChildElement(elementName.c_str());
    if (el == NULL) {
        CLOG(L"Warning: XML element '%s' not found", elementName.c_str());
        return L"";
    }

    const char* str = el->GetText();
    if (str == NULL) {
        return L"";
    } else {
        return StringUtils::Widen(str);
    }
}

void Settings::SetText(std::string elementName, std::string text) {
    tinyxml2::XMLElement *el = GetOrCreateElement(elementName);
    el->SetText(text.c_str());
}

int Settings::GetInt(std::string elementName) {
    tinyxml2::XMLElement *el = _root->FirstChildElement(elementName.c_str());
    if (el == NULL) {
        CLOG(L"Warning: XML element '%s' not found", elementName.c_str());
        return 0;
    }

    int val = 0;
    el->QueryIntText(&val);
    return val;
}

void Settings::SetInt(std::string elementName, int value) {
    tinyxml2::XMLElement *el = GetOrCreateElement(elementName);
    el->SetText(value);
}

tinyxml2::XMLElement *Settings::GetOrCreateElement(std::string elementName) {
    tinyxml2::XMLElement *el = _root->FirstChildElement(elementName.c_str());
    if (el == NULL) {
        el = _xml.NewElement(elementName.c_str());
        _root->InsertEndChild(el);
    }
    return el;
}