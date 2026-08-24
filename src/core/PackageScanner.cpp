#include "PackageScanner.h"
#include <QDirIterator>
#include <QFileInfo>

ModelPackageFiles PackageScanner::ScanPackageDirectory(const std::string& packageDirStr) {
    ModelPackageFiles result;
    result.packageDir = packageDirStr;
    QString rootDir = QString::fromStdString(packageDirStr);

    if (rootDir.isEmpty() || !QDir(rootDir).exists()) {
        result.scanLog.push_back("FAIL: 模型包目录不存在: " + packageDirStr);
        return result;
    }

    result.scanLog.push_back("INFO: 自动扫描模型包目录: " + packageDirStr);

    QDirIterator it(rootDir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

    std::vector<QString> allHeaders;
    std::vector<QString> allDlls;
    std::vector<QString> allLibs;

    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        QString ext = info.suffix().toLower();
        QString fullPath = info.absoluteFilePath();

        if (ext == "h" || ext == "hpp") {
            allHeaders.push_back(fullPath);
        } else if (ext == "dll") {
            allDlls.push_back(fullPath);
        } else if (ext == "lib") {
            allLibs.push_back(fullPath);
        }
    }

    // 1. Process Headers
    for (const auto& h : allHeaders) {
        result.allHeaderFiles.push_back(h.toStdString());
    }
    result.scanLog.push_back("PASS: 找到 C/C++ 头文件共 " + std::to_string(result.allHeaderFiles.size()) + " 个");

    // Helper lambda to classify Release vs Debug
    auto isDebugFile = [](const QString& filePath) {
        QString lowerPath = filePath.toLower();
        QFileInfo info(filePath);
        QString name = info.completeBaseName().toLower();

        if (lowerPath.contains("/debug/") || lowerPath.contains("\\debug\\") || lowerPath.contains("/x64/debug/") || lowerPath.contains("\\x64\\debug\\")) {
            return true;
        }
        if (name.endsWith("d") || name.endsWith("_d") || name.endsWith("_debug") || name.endsWith("-d")) {
            return true;
        }
        return false;
    };

    // 2. Classify DLLs
    for (const auto& dll : allDlls) {
        std::string sDll = dll.toStdString();
        result.allDllFiles.push_back(sDll);
        if (isDebugFile(dll)) {
            result.debugDllFiles.push_back(sDll);
        } else {
            result.releaseDllFiles.push_back(sDll);
        }
    }
    result.scanLog.push_back("PASS: 找到 DLL 动态库文件共 " + std::to_string(result.allDllFiles.size()) + " 个 (Release: " + std::to_string(result.releaseDllFiles.size()) + ", Debug: " + std::to_string(result.debugDllFiles.size()) + ")");

    // 3. Classify LIBs
    for (const auto& lib : allLibs) {
        std::string sLib = lib.toStdString();
        result.allLibFiles.push_back(sLib);
        if (isDebugFile(lib)) {
            result.debugLibFiles.push_back(sLib);
        } else {
            result.releaseLibFiles.push_back(sLib);
        }
    }
    result.scanLog.push_back("PASS: 找到 LIB 库文件共 " + std::to_string(result.allLibFiles.size()) + " 个 (Release: " + std::to_string(result.releaseLibFiles.size()) + ", Debug: " + std::to_string(result.debugLibFiles.size()) + ")");

    return result;
}
