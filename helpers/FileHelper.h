#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <shlwapi.h>
#include <shlobj.h>
#include <string>

class FileHelper {
public:
    static void createAllSubdirectories(const std::string& path) {
        std::string fullPath;
        if (PathIsRelativeA(path.c_str())) {
            // Get current directory and append
            char currentDir[MAX_PATH] = { 0 };
            GetCurrentDirectoryA(MAX_PATH, currentDir);

            // Combine paths with proper separator
            fullPath = std::string(currentDir);
            if (fullPath.back() != '\\')
                fullPath += '\\';

            fullPath += path;
        } else // Path is absolute
            fullPath = path;

        // Create intermediate directories if they do not exist
        if (!PathFileExistsA(fullPath.c_str())) {
            SHCreateDirectoryExA(nullptr, fullPath.c_str(), nullptr);
        }
    }
};

#endif //FILEHELPER_H
