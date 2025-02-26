#ifndef CONFIGHELPER_H
#define CONFIGHELPER_H

#include <string>
#include <fstream>
#include <windows.h>
#include <shlwapi.h>

class ConfigHelper {
private:
    std::string iniFilePath;

public:
    ConfigHelper(const std::string& pathToIniFile) {
        if (PathIsRelativeA(pathToIniFile.c_str())) {
            // Get current directory and append
            char currentDir[MAX_PATH];
            GetCurrentDirectoryA(MAX_PATH, currentDir);

            // Combine paths with proper separator
            iniFilePath = std::string(currentDir);
            if (iniFilePath.back() != '\\')
                iniFilePath += '\\';

            iniFilePath += pathToIniFile;
        } else // Path is absolute
            iniFilePath = pathToIniFile;
    }

    std::string readIni(const std::string& section, const std::string& key) {
        char buffer[1024] = { 0 };

        const DWORD result = GetPrivateProfileStringA(
            section.c_str(),
            key.c_str(),
            "",
            buffer,
            sizeof(buffer),
            iniFilePath.c_str()
        );

        if (result == 0) {
            throw std::runtime_error("Failed to read ini file: " + iniFilePath + ", section: " + section + ", key: " + key);
        }

        return buffer;
    }

    bool writeIni(const std::string& section, const std::string& key, const std::string& value) {
        const BOOL result = WritePrivateProfileStringA(
            section.c_str(),
            key.c_str(),
            value.c_str(),
            iniFilePath.c_str()
        );

        return result != 0;
    }
};

#endif //CONFIGHELPER_H
