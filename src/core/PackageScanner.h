#ifndef PACKAGE_SCANNER_H
#define PACKAGE_SCANNER_H

#include <string>
#include <vector>

struct ModelPackageFiles {
    std::string packageDir;
    std::vector<std::string> allHeaderFiles; // All discovered .h / .hpp files
    std::vector<std::string> allDllFiles;    // All discovered .dll files
    std::vector<std::string> allLibFiles;    // All discovered .lib files

    std::vector<std::string> releaseDllFiles;
    std::vector<std::string> releaseLibFiles;
    std::vector<std::string> debugDllFiles;
    std::vector<std::string> debugLibFiles;

    std::vector<std::string> scanLog;
};

class PackageScanner {
public:
    static ModelPackageFiles ScanPackageDirectory(const std::string& packageDir);
};

#endif // PACKAGE_SCANNER_H
