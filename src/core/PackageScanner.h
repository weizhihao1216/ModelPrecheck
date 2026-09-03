#ifndef PACKAGE_SCANNER_H
#define PACKAGE_SCANNER_H

#include <string>
#include <vector>

/// Fixed third-party model package layout under the selected package root:
///   include/  — headers (.h/.hpp), recursive
///   lib/      — import/static libraries (.lib), recursive
///   models/   — model DLLs (.dll), recursive
struct ModelPackageLayout {
    static const char* IncludeDirName();
    static const char* LibDirName();
    static const char* ModelsDirName();
};

struct ModelPackageFiles {
    std::string packageDir;
    std::string includeDir;
    std::string libDir;
    std::string modelsDir;

    bool includeDirExists = false;
    bool libDirExists = false;
    bool modelsDirExists = false;

    std::vector<std::string> allHeaderFiles; // under include/
    std::vector<std::string> allDllFiles;    // under models/
    std::vector<std::string> allLibFiles;    // under lib/

    std::vector<std::string> releaseDllFiles;
    std::vector<std::string> releaseLibFiles;
    std::vector<std::string> debugDllFiles;
    std::vector<std::string> debugLibFiles;

    std::vector<std::string> scanLog;

    bool layoutValid() const {
        return includeDirExists && libDirExists && modelsDirExists;
    }
};

class PackageScanner {
public:
    static std::string IncludeDirectory(const std::string& packageDir);
    static std::string LibDirectory(const std::string& packageDir);
    static std::string ModelsDirectory(const std::string& packageDir);

    /// Recursively scan include/, lib/, models/ under the package root.
    static ModelPackageFiles ScanPackageDirectory(const std::string& packageDir);
};

#endif // PACKAGE_SCANNER_H
