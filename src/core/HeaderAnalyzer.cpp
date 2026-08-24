#include "HeaderAnalyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

static std::string DetectFileEncoding(const std::string& buffer) {
    if (buffer.size() >= 3 &&
        static_cast<unsigned char>(buffer[0]) == 0xEF &&
        static_cast<unsigned char>(buffer[1]) == 0xBB &&
        static_cast<unsigned char>(buffer[2]) == 0xBF) {
        return "UTF-8 BOM";
    }

    bool isUtf8 = true;
    size_t i = 0;
    size_t len = buffer.size();

    while (i < len) {
        unsigned char c = buffer[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= len || (buffer[i + 1] & 0xC0) != 0x80) { isUtf8 = false; break; }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= len || (buffer[i + 1] & 0xC0) != 0x80 || (buffer[i + 2] & 0xC0) != 0x80) { isUtf8 = false; break; }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= len || (buffer[i + 1] & 0xC0) != 0x80 || (buffer[i + 2] & 0xC0) != 0x80 || (buffer[i + 3] & 0xC0) != 0x80) { isUtf8 = false; break; }
            i += 4;
        } else {
            isUtf8 = false;
            break;
        }
    }

    if (isUtf8) {
        return "UTF-8";
    }
    return "GBK / ANSI";
}

#include <set>

std::vector<std::string> HeaderAnalyzer::ExtractDeclaredFunctions(const std::string& content) {
    std::vector<std::string> funcs;
    std::set<std::string> keywords = {
        "if", "while", "for", "switch", "return", "sizeof", "declspec", "extern",
        "pragma", "typedef", "struct", "union", "enum", "void", "int", "double",
        "float", "char", "bool", "long", "short", "unsigned", "signed", "const", "APIENTRY", "WINAPI"
    };

    std::regex funcRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{}]*\)\s*;)");
    auto words_begin = std::sregex_iterator(content.begin(), content.end(), funcRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string name = match[1].str();
        if (keywords.find(name) == keywords.end()) {
            if (std::find(funcs.begin(), funcs.end(), name) == funcs.end()) {
                funcs.push_back(name);
            }
        }
    }
    return funcs;
}

HeaderAnalysisReport HeaderAnalyzer::AnalyzeHeader(const std::string& headerPath) {
    HeaderAnalysisReport report;
    report.filePath = headerPath;
    report.hasExternC = false;
    report.hasDeclspec = false;
    report.hasPackDirective = false;
    report.overallPass = true;

    std::ifstream file(headerPath, std::ios::binary);
    if (!file.is_open()) {
        report.overallPass = false;
        report.logMessages.push_back("FAIL: 无法打开头文件: " + headerPath);
        return report;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    report.encoding = DetectFileEncoding(content);
    report.logMessages.push_back("INFO: 头文件文本编码格式为 " + report.encoding);

    // 1. extern "C" Check
    if (content.find("extern \"C\"") != std::string::npos || content.find("extern 'C'") != std::string::npos) {
        report.hasExternC = true;
        report.logMessages.push_back("PASS: 头文件包含 extern \"C\" 声明，兼容 C/C++ 规范");
    } else {
        report.hasExternC = false;
        report.logMessages.push_back("WARN: 头文件未显式包含 extern \"C\" 声明");
    }

    // 2. __declspec check
    if (content.find("__declspec") != std::string::npos || content.find("dllexport") != std::string::npos || content.find("dllimport") != std::string::npos) {
        report.hasDeclspec = true;
        report.logMessages.push_back("PASS: 头文件包含 __declspec(dllexport/dllimport) 导出宏定义");
    } else {
        report.hasDeclspec = false;
        report.logMessages.push_back("INFO: 未检测到 __declspec 显式导出关键字");
    }

    // 3. #pragma pack check
    if (content.find("#pragma pack") != std::string::npos) {
        report.hasPackDirective = true;
        report.logMessages.push_back("INFO: 检测到 #pragma pack 结构体对齐指令");
    }

    // 4. Extract all declared functions
    report.declaredFunctions = ExtractDeclaredFunctions(content);
    report.logMessages.push_back("INFO: 从头文件中成功提取出 " + std::to_string(report.declaredFunctions.size()) + " 个 C 函数声明原型");

    for (const auto& funcName : report.declaredFunctions) {
        report.logMessages.push_back("  -> 接口声明: " + funcName);
    }

    report.overallPass = true;
    return report;
}

HeaderExportConsistency HeaderAnalyzer::VerifyConsistency(const std::vector<std::string>& headerDeclaredFuncs,
                                                           const std::vector<std::string>& binaryExportedSymbols) {
    HeaderExportConsistency report;
    report.declaredInHeader = headerDeclaredFuncs;
    report.exportedInBinary = binaryExportedSymbols;

    for (const auto& hFunc : headerDeclaredFuncs) {
        bool found = false;
        for (const auto& binSym : binaryExportedSymbols) {
            if (hFunc == binSym) {
                found = true;
                break;
            }
        }
        if (found) {
            report.matchedFunctions.push_back(hFunc);
        } else {
            report.declaredButNotExported.push_back(hFunc);
        }
    }

    for (const auto& binSym : binaryExportedSymbols) {
        bool found = false;
        for (const auto& hFunc : headerDeclaredFuncs) {
            if (binSym == hFunc) {
                found = true;
                break;
            }
        }
        if (!found) {
            report.exportedButNotDeclared.push_back(binSym);
        }
    }

    size_t totalUnique = report.matchedFunctions.size() + report.declaredButNotExported.size() + report.exportedButNotDeclared.size();
    if (totalUnique > 0) {
        report.consistencyRatio = (static_cast<double>(report.matchedFunctions.size()) / totalUnique) * 100.0;
    } else {
        report.consistencyRatio = 100.0;
    }

    report.isFullyConsistent = report.declaredButNotExported.empty() && report.exportedButNotDeclared.empty();

    if (report.isFullyConsistent) {
        report.logMessages.push_back("PASS: 头文件接口与二进制导出符号 100% 完全匹配！");
    } else {
        if (!report.declaredButNotExported.empty()) {
            report.logMessages.push_back("WARN: 检测到 " + std::to_string(report.declaredButNotExported.size()) + " 个函数在头文件中已声明，但 DLL/LIB 导出表中缺失（可能引发 LNK2019 链接错误）");
        }
        if (!report.exportedButNotDeclared.empty()) {
            report.logMessages.push_back("INFO: 检测到 " + std::to_string(report.exportedButNotDeclared.size()) + " 个导出符号存在于 DLL/LIB 中，但头文件中未声明");
        }
    }

    return report;
}
