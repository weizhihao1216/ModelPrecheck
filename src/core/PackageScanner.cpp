#include "PackageScanner.h"
#include "../utils/QtEncoding.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace {

QString joinSubdir(const QString& packageDir, const char* subdir) {
    return QDir::toNativeSeparators(QDir(packageDir).filePath(QString::fromUtf8(subdir)));
}

void collectFilesRecursive(const QString& rootDir,
                           const QStringList& nameFilters,
                           std::vector<QString>& out) {
    if (rootDir.isEmpty() || !QDir(rootDir).exists()) return;
    QDirIterator it(rootDir, nameFilters,
                    QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        out.push_back(QFileInfo(it.next()).absoluteFilePath());
    }
}

bool isDebugFile(const QString& filePath) {
    const QString lowerPath = filePath.toLower();
    const QFileInfo info(filePath);
    const QString name = info.completeBaseName().toLower();

    if (lowerPath.contains("/debug/") || lowerPath.contains("\\debug\\")
        || lowerPath.contains("/x64/debug/") || lowerPath.contains("\\x64\\debug\\")) {
        return true;
    }
    if (name.endsWith("d") || name.endsWith("_d")
        || name.endsWith("_debug") || name.endsWith("-d")) {
        return true;
    }
    return false;
}

} // namespace

const char* ModelPackageLayout::IncludeDirName() { return "include"; }
const char* ModelPackageLayout::LibDirName() { return "lib"; }
const char* ModelPackageLayout::ModelsDirName() { return "models"; }

std::string PackageScanner::IncludeDirectory(const std::string& packageDir) {
    return qToUtf8(joinSubdir(qUtf8(packageDir), ModelPackageLayout::IncludeDirName()));
}

std::string PackageScanner::LibDirectory(const std::string& packageDir) {
    return qToUtf8(joinSubdir(qUtf8(packageDir), ModelPackageLayout::LibDirName()));
}

std::string PackageScanner::ModelsDirectory(const std::string& packageDir) {
    return qToUtf8(joinSubdir(qUtf8(packageDir), ModelPackageLayout::ModelsDirName()));
}

ModelPackageFiles PackageScanner::ScanPackageDirectory(const std::string& packageDirStr) {
    ModelPackageFiles result;
    result.packageDir = packageDirStr;
    const QString rootDir = qUtf8(packageDirStr);

    result.includeDir = IncludeDirectory(packageDirStr);
    result.libDir = LibDirectory(packageDirStr);
    result.modelsDir = ModelsDirectory(packageDirStr);

    if (rootDir.isEmpty() || !QDir(rootDir).exists()) {
        result.scanLog.push_back("FAIL: 模型包目录不存在: " + packageDirStr);
        return result;
    }

    result.scanLog.push_back(
        "INFO: 第三方模型包必须按固定目录归放：根目录/include（头文件）、"
        "根目录/lib（.lib）、根目录/models（.dll）；以上目录均递归扫描子文件夹。");
    result.scanLog.push_back("INFO: 扫描模型包目录: " + packageDirStr);

    const QString includeDir = qUtf8(result.includeDir);
    const QString libDir = qUtf8(result.libDir);
    const QString modelsDir = qUtf8(result.modelsDir);

    result.includeDirExists = QDir(includeDir).exists();
    result.libDirExists = QDir(libDir).exists();
    result.modelsDirExists = QDir(modelsDir).exists();

    if (!result.includeDirExists) {
        result.scanLog.push_back(
            "FAIL: 缺少 include 目录（头文件必须放在 include/ 及其子目录下）: "
            + result.includeDir);
    }
    if (!result.libDirExists) {
        result.scanLog.push_back(
            "FAIL: 缺少 lib 目录（.lib 必须放在 lib/ 及其子目录下）: "
            + result.libDir);
    }
    if (!result.modelsDirExists) {
        result.scanLog.push_back(
            "FAIL: 缺少 models 目录（.dll 必须放在 models/ 及其子目录下）: "
            + result.modelsDir);
    }
    if (!result.layoutValid()) {
        result.scanLog.push_back(
            "FAIL: 模型包目录结构不符合要求，请按 include / lib / models 归放后重试");
    }

    std::vector<QString> allHeaders;
    std::vector<QString> allDlls;
    std::vector<QString> allLibs;
    collectFilesRecursive(includeDir,
                          { QStringLiteral("*.h"), QStringLiteral("*.hpp") },
                          allHeaders);
    collectFilesRecursive(modelsDir, { QStringLiteral("*.dll") }, allDlls);
    collectFilesRecursive(libDir, { QStringLiteral("*.lib") }, allLibs);

    for (const auto& h : allHeaders) {
        result.allHeaderFiles.push_back(qToUtf8(QDir::toNativeSeparators(h)));
    }
    if (result.includeDirExists) {
        result.scanLog.push_back(
            "PASS: include/ 下找到头文件共 "
            + std::to_string(result.allHeaderFiles.size()) + " 个");
        if (result.allHeaderFiles.empty()) {
            result.scanLog.push_back("WARN: include/ 目录存在，但未找到 .h/.hpp 文件");
        }
    }

    for (const auto& dll : allDlls) {
        const std::string sDll = qToUtf8(QDir::toNativeSeparators(dll));
        result.allDllFiles.push_back(sDll);
        if (isDebugFile(dll)) {
            result.debugDllFiles.push_back(sDll);
        } else {
            result.releaseDllFiles.push_back(sDll);
        }
    }
    if (result.modelsDirExists) {
        result.scanLog.push_back(
            "PASS: models/ 下找到 DLL 共 "
            + std::to_string(result.allDllFiles.size()) + " 个 (Release: "
            + std::to_string(result.releaseDllFiles.size()) + ", Debug: "
            + std::to_string(result.debugDllFiles.size()) + ")");
        if (result.allDllFiles.empty()) {
            result.scanLog.push_back("WARN: models/ 目录存在，但未找到 .dll 文件");
        }
    }

    for (const auto& lib : allLibs) {
        const std::string sLib = qToUtf8(QDir::toNativeSeparators(lib));
        result.allLibFiles.push_back(sLib);
        if (isDebugFile(lib)) {
            result.debugLibFiles.push_back(sLib);
        } else {
            result.releaseLibFiles.push_back(sLib);
        }
    }
    if (result.libDirExists) {
        result.scanLog.push_back(
            "PASS: lib/ 下找到 LIB 共 "
            + std::to_string(result.allLibFiles.size()) + " 个 (Release: "
            + std::to_string(result.releaseLibFiles.size()) + ", Debug: "
            + std::to_string(result.debugLibFiles.size()) + ")");
        if (result.allLibFiles.empty()) {
            result.scanLog.push_back("WARN: lib/ 目录存在，但未找到 .lib 文件");
        }
    }

    return result;
}
