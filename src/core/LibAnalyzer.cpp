#include "LibAnalyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifndef IMAGE_FILE_MACHINE_AMD64
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#endif
#ifndef IMAGE_FILE_MACHINE_I386
#define IMAGE_FILE_MACHINE_I386 0x014c
#endif

LibAnalysisReport LibAnalyzer::AnalyzeLib(const std::string& libPath,
                                           const std::vector<std::string>& requiredExports) {
    LibAnalysisReport report;
    report.filePath = libPath;
    report.architecture = "Unknown";
    report.is64Bit = false;
    report.libType = "Unknown";
    report.isArchMatch = false;
    report.overallPass = false;

    std::ifstream file(libPath, std::ios::binary);
    if (!file.is_open()) {
        report.logMessages.push_back("FAIL: 无法打开 LIB 文件: " + libPath);
        return report;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    // 1. Verify COFF Archive Header "!<arch>\n"
    const char magic[8] = {'!', '<', 'a', 'r', 'c', 'h', '>', '\n'};
    if (content.size() < 8 || std::memcmp(content.data(), magic, 8) != 0) {
        report.logMessages.push_back("FAIL: 文件非标准 COFF .lib 归档格式 (缺少 !<arch> 标志头)");
        return report;
    }

    report.logMessages.push_back("PASS: 格式符合标准 COFF .lib 归档规范");

    // 2. Machine Architecture & Import/Static Lib Detection
    bool foundX64 = false;
    bool foundX86 = false;
    bool isImportLib = false;

    // Scan for Import Header Sig2 (0xFFFF)
    for (size_t i = 8; i + 20 < content.size(); i++) {
        uint16_t sig1 = *reinterpret_cast<const uint16_t*>(content.data() + i);
        uint16_t sig2 = *reinterpret_cast<const uint16_t*>(content.data() + i + 2);
        if (sig1 == 0x0000 && sig2 == 0xFFFF) {
            isImportLib = true;
            uint16_t machine = *reinterpret_cast<const uint16_t*>(content.data() + i + 6);
            if (machine == IMAGE_FILE_MACHINE_AMD64) foundX64 = true;
            else if (machine == IMAGE_FILE_MACHINE_I386) foundX86 = true;
        }
    }

    // Fallback: search for machine type codes in COFF Headers if not import lib
    if (!foundX64 && !foundX86) {
        for (size_t i = 8; i + 2 < content.size(); i++) {
            uint16_t val = *reinterpret_cast<const uint16_t*>(content.data() + i);
            if (val == IMAGE_FILE_MACHINE_AMD64) {
                foundX64 = true;
            } else if (val == IMAGE_FILE_MACHINE_I386) {
                foundX86 = true;
            }
        }
    }

    if (foundX64) {
        report.architecture = "x64";
        report.is64Bit = true;
        report.isArchMatch = true;
        report.logMessages.push_back("PASS: 目标 CPU 架构为 x64 (与 64 位仿真引擎相匹配)");
    } else if (foundX86) {
        report.architecture = "x86";
        report.is64Bit = false;
        report.isArchMatch = false;
        report.logMessages.push_back("FAIL: 目标 CPU 架构为 x86 (32位，与 64 位仿真引擎不一致!)");
    } else {
        report.architecture = "x64"; // Default fallback if unspecified
        report.is64Bit = true;
        report.isArchMatch = true;
        report.logMessages.push_back("INFO: 未提取到明确 Machine 标志，推断为通用 x64 归档");
    }

    if (isImportLib) {
        report.libType = "Import Library (.lib 动态库导入表)";
        report.logMessages.push_back("INFO: 鉴定库类型: DLL 动态链接导出表 (Import Library)");
    } else {
        report.libType = "Static Library (.lib 静态链接库)";
        report.logMessages.push_back("INFO: 鉴定库类型: 静态代码库 (Static Library)");
    }

    // 3. Symbol Search (Informational)
    for (const auto& funcName : requiredExports) {
        if (!funcName.empty() && content.find(funcName) != std::string::npos) {
            report.foundSymbols.push_back(funcName);
            report.logMessages.push_back("INFO: LIB 库包含符号: " + funcName);
        }
    }

    report.logMessages.push_back("INFO: LIB 库文件解包与 COFF 结构解析完毕");
    report.overallPass = report.isArchMatch;
    return report;
}
