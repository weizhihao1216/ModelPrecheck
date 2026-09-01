#include "HeaderAnalyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <set>

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

static bool ContainsIgnoreCase(const std::string& hay, const std::string& needle) {
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    return it != hay.end();
}

static std::string FindPreferredSymbol(const std::vector<std::string>& funcs, const std::vector<std::string>& candidates) {
    for (const auto& c : candidates) {
        for (const auto& f : funcs) {
            if (f == c) return f;
        }
    }
    for (const auto& c : candidates) {
        for (const auto& f : funcs) {
            if (ContainsIgnoreCase(f, c)) return f;
        }
    }
    return candidates.empty() ? std::string() : candidates.front();
}

ModelApiStyle HeaderAnalyzer::DetectApiStyle(const std::string& headerContent,
                                             InterfaceMapping* outMapping,
                                             std::string* outDescription) {
    std::vector<std::string> funcs = ExtractDeclaredFunctions(headerContent);

    std::string createName = FindPreferredSymbol(funcs, { "Model_Create", "CreateModel", "CreateInstance", "Weapon_Create" });
    std::string initName = FindPreferredSymbol(funcs, { "Model_Init", "InitModel", "Weapon_Init", "Initialize" });
    std::string stepName = FindPreferredSymbol(funcs, { "Model_Step", "StepModel", "Weapon_Step", "Update" });
    std::string destroyName = FindPreferredSymbol(funcs, { "Model_Destroy", "DestroyModel", "Weapon_Destroy", "Release" });
    std::string infoName = FindPreferredSymbol(funcs, { "Model_GetInfo", "GetInfo", "Weapon_GetInfo" });

    bool hasCreateDecl = false;
    for (const auto& f : funcs) {
        if (ContainsIgnoreCase(f, "Create") || ContainsIgnoreCase(f, "CreateInstance")) {
            hasCreateDecl = true;
            break;
        }
    }

    std::regex handleInitRegex(
        R"((Model_Init|InitModel|Weapon_Init|Initialize)\s*\(\s*(void\s*\*|HANDLE|ModelHandle|[\w:]*Handle)\s*[,)])",
        std::regex::icase);
    std::regex handleStepRegex(
        R"((Model_Step|StepModel|Weapon_Step|Update)\s*\(\s*(void\s*\*|HANDLE|ModelHandle|[\w:]*Handle)\s*[,)])",
        std::regex::icase);
    std::regex singletonInitRegex(
        R"((Model_Init|InitModel|Weapon_Init)\s*\(\s*(const\s+)?WeaponModelParams)",
        std::regex::icase);

    bool handleInit = std::regex_search(headerContent, handleInitRegex);
    bool handleStep = std::regex_search(headerContent, handleStepRegex);
    bool singletonInit = std::regex_search(headerContent, singletonInitRegex);

    ModelApiStyle style = ModelApiStyle::Unknown;
    std::string desc;
    InterfaceMapping mapping;

    if (hasCreateDecl || (handleInit && handleStep)) {
        style = ModelApiStyle::HandleBased;
        desc = "句柄式多实例接口 (Create + Init(handle)/Step(handle)) — 支持多对象并发";
        mapping = InterfaceMapping::DefaultHandleBased();
        if (!createName.empty()) mapping.entries[0].symbolName = createName;
        if (!initName.empty()) mapping.entries[1].symbolName = initName;
        if (!stepName.empty()) mapping.entries[2].symbolName = stepName;
        if (!destroyName.empty()) mapping.entries[3].symbolName = destroyName;
    } else if (singletonInit || !funcs.empty()) {
        style = ModelApiStyle::Singleton;
        desc = "单例/全局状态接口 (Init/Step 无实例句柄) — 通常不支持多对象与多线程并发";
        mapping = InterfaceMapping::DefaultSingleton();
        if (!initName.empty()) mapping.entries[0].symbolName = initName;
        if (!stepName.empty()) mapping.entries[1].symbolName = stepName;
        if (!destroyName.empty()) mapping.entries[2].symbolName = destroyName;
        // Init may be absent in some models — leave enabled; user can disable in UI
        bool hasInitSymbol = false;
        for (const auto& f : funcs) {
            if (ContainsIgnoreCase(f, "Init") || ContainsIgnoreCase(f, "Initialize")) {
                hasInitSymbol = true;
                break;
            }
        }
        if (!hasInitSymbol && !singletonInit) {
            mapping.entries[0].enabled = false;
            desc += "；头文件未见 Init，已建议禁用 Setup 初始化项";
        }
    } else {
        desc = "未能从头文件判定接口风格，请在界面中手动配置调用序列";
        mapping = InterfaceMapping::DefaultSingleton();
    }

    if (!infoName.empty()) {
        CallMappingEntry info;
        info.symbolName = infoName;
        info.phase = CallPhase::Setup;
        info.signature = CallSignature::GetInfoStr;
        info.enabled = false; // optional
        mapping.entries.insert(mapping.entries.begin(), info);
    }

    if (outMapping) *outMapping = mapping;
    if (outDescription) *outDescription = desc;
    return style;
}

static std::string TrimCopy(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static bool LooksLikeHandleType(const std::string& typeText) {
    return ContainsIgnoreCase(typeText, "void*")
        || ContainsIgnoreCase(typeText, "HANDLE")
        || ContainsIgnoreCase(typeText, "Handle")
        || ContainsIgnoreCase(typeText, "HINSTANCE");
}

static bool LooksLikeParamsType(const std::string& typeText) {
    return ContainsIgnoreCase(typeText, "WeaponModelParams")
        || ContainsIgnoreCase(typeText, "ModelParams")
        || ContainsIgnoreCase(typeText, "InitParam");
}

static bool LooksLikeOutputType(const std::string& typeText) {
    return ContainsIgnoreCase(typeText, "WeaponModelOutput")
        || ContainsIgnoreCase(typeText, "ModelOutput")
        || ContainsIgnoreCase(typeText, "StepOutput");
}

static std::vector<std::string> SplitParams(const std::string& paramList) {
    std::vector<std::string> parts;
    std::string cur;
    int depth = 0;
    for (char ch : paramList) {
        if (ch == '<' || ch == '(') depth++;
        else if (ch == '>' || ch == ')') depth--;
        if (ch == ',' && depth == 0) {
            std::string t = TrimCopy(cur);
            if (!t.empty() && t != "void") parts.push_back(t);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    std::string t = TrimCopy(cur);
    if (!t.empty() && t != "void") parts.push_back(t);
    return parts;
}

CallSignature HeaderAnalyzer::ClassifyCallSignature(const std::string& returnType,
                                                    const std::string& paramList,
                                                    const std::string& funcName) {
    std::string ret = TrimCopy(returnType);
    auto params = SplitParams(paramList);

    bool retVoid = ContainsIgnoreCase(ret, "void") && ret.find('*') == std::string::npos;
    bool retVoidPtr = LooksLikeHandleType(ret) || (ContainsIgnoreCase(ret, "void") && ret.find('*') != std::string::npos);
    bool retCStr = ContainsIgnoreCase(ret, "char") && ret.find('*') != std::string::npos;

    if (retCStr && params.empty()) return CallSignature::GetInfoStr;
    if (retVoidPtr && params.empty()) return CallSignature::CreateHandle;
    if (ContainsIgnoreCase(funcName, "Create") && params.empty()) return CallSignature::CreateHandle;

    if (params.empty()) {
        if (retVoid) return CallSignature::VoidNoArg;
        return CallSignature::IntNoArg;
    }

    bool firstHandle = LooksLikeHandleType(params[0]);
    if (params.size() == 1) {
        if (firstHandle && retVoid) return CallSignature::VoidHandle;
        if (firstHandle) return CallSignature::IntHandleParams; // handle-only rare; treat as handle+...
        if (LooksLikeParamsType(params[0])) return CallSignature::IntParams;
        if (LooksLikeOutputType(params[0])) return CallSignature::IntOutput;
        if (retVoid) return CallSignature::VoidNoArg;
        return CallSignature::IntNoArg;
    }

    if (firstHandle && params.size() >= 2) {
        if (LooksLikeOutputType(params[1])) return CallSignature::IntHandleOutput;
        if (LooksLikeParamsType(params[1])) return CallSignature::IntHandleParams;
        if (retVoid) return CallSignature::VoidHandle;
        return CallSignature::IntHandleParams;
    }

    if (LooksLikeParamsType(params[0])) return CallSignature::IntParams;
    if (LooksLikeOutputType(params[0])) return CallSignature::IntOutput;
    return CallSignature::IntNoArg;
}

CallPhase HeaderAnalyzer::SuggestCallPhase(const std::string& funcName, CallSignature sig) {
    if (sig == CallSignature::CreateHandle) return CallPhase::Setup;
    if (sig == CallSignature::VoidHandle || ContainsIgnoreCase(funcName, "Destroy")
        || ContainsIgnoreCase(funcName, "Release") || ContainsIgnoreCase(funcName, "Free")) {
        return CallPhase::Teardown;
    }
    if (sig == CallSignature::IntOutput || sig == CallSignature::IntHandleOutput
        || ContainsIgnoreCase(funcName, "Step") || ContainsIgnoreCase(funcName, "Update")
        || ContainsIgnoreCase(funcName, "Tick")) {
        return CallPhase::Step;
    }
    return CallPhase::Setup;
}

std::vector<HeaderFunctionDecl> HeaderAnalyzer::ExtractFunctionDeclarations(const std::string& content) {
    std::vector<HeaderFunctionDecl> decls;
    std::set<std::string> keywords = {
        "if", "while", "for", "switch", "return", "sizeof", "declspec", "extern",
        "pragma", "typedef", "struct", "union", "enum", "void", "int", "double",
        "float", "char", "bool", "long", "short", "unsigned", "signed", "const",
        "APIENTRY", "WINAPI", "inline", "static", "virtual", "explicit", "class",
        "namespace", "template", "typename", "using", "public", "private", "protected"
    };

    // Avoid std::regex here: MSVC regex can stack-overflow on large headers.
    std::string text = content;
    // Strip // and /* */ comments (rough) to reduce false positives
    {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size(); ) {
            if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
                while (i < text.size() && text[i] != '\n') ++i;
            } else if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
                i += 2;
                while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) ++i;
                if (i + 1 < text.size()) i += 2;
            } else if (text[i] == '"' || text[i] == '\'') {
                char q = text[i++];
                out.push_back(' ');
                while (i < text.size() && text[i] != q) {
                    if (text[i] == '\\' && i + 1 < text.size()) i += 2;
                    else ++i;
                }
                if (i < text.size()) ++i;
            } else {
                out.push_back(text[i++]);
            }
        }
        text.swap(out);
    }

    std::set<std::string> seenNames;
    const size_t n = text.size();
    for (size_t i = 0; i < n; ) {
        // Find next '(' that looks like a function declarator ending with ");"
        if (text[i] != '(') { ++i; continue; }

        // Walk back to get identifier name
        size_t nameEnd = i;
        while (nameEnd > 0 && std::isspace(static_cast<unsigned char>(text[nameEnd - 1]))) --nameEnd;
        if (nameEnd == 0) { ++i; continue; }
        size_t nameBegin = nameEnd;
        while (nameBegin > 0) {
            unsigned char c = static_cast<unsigned char>(text[nameBegin - 1]);
            if (std::isalnum(c) || c == '_') --nameBegin;
            else break;
        }
        if (nameBegin >= nameEnd) { ++i; continue; }
        std::string name = text.substr(nameBegin, nameEnd - nameBegin);
        if (keywords.count(name)) { ++i; continue; }
        if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') { ++i; continue; }

        // Return type: tokens immediately before name (bounded)
        size_t retEnd = nameBegin;
        while (retEnd > 0 && std::isspace(static_cast<unsigned char>(text[retEnd - 1]))) --retEnd;
        size_t retBegin = retEnd;
        int steps = 0;
        while (retBegin > 0 && steps < 80) {
            unsigned char c = static_cast<unsigned char>(text[retBegin - 1]);
            if (std::isalnum(c) || c == '_' || c == ':' || c == '*' || c == '&' || c == '<' || c == '>') {
                --retBegin; ++steps;
            } else if (std::isspace(c)) {
                // allow spaces inside return type but stop at ; { } newline after collecting something
                if (retBegin < retEnd) {
                    --retBegin; ++steps;
                    // stop if previous non-space is terminator
                    size_t k = retBegin;
                    while (k > 0 && std::isspace(static_cast<unsigned char>(text[k - 1]))) --k;
                    if (k > 0) {
                        char p = text[k - 1];
                        if (p == ';' || p == '{' || p == '}' || p == ')' || p == '#') break;
                    }
                } else break;
            } else break;
        }
        std::string retRaw = TrimCopy(text.substr(retBegin, retEnd - retBegin));

        // Match balanced parentheses for params
        size_t j = i + 1;
        int depth = 1;
        while (j < n && depth > 0) {
            if (text[j] == '(') ++depth;
            else if (text[j] == ')') --depth;
            ++j;
            if (j - i > 4000) break; // runaway guard
        }
        if (depth != 0) { ++i; continue; }
        std::string params = TrimCopy(text.substr(i + 1, j - i - 2));

        // Must be a declaration: after ) optional whitespace then ;
        size_t k = j;
        while (k < n && std::isspace(static_cast<unsigned char>(text[k]))) ++k;
        if (k >= n || text[k] != ';') { ++i; continue; }

        // Skip if looks like a macro call at line start without type (still allow Create-style)
        HeaderFunctionDecl d;
        d.name = name;
        d.returnType = retRaw.empty() ? std::string("int") : retRaw;
        {
            std::string cleaned = d.returnType;
            const char* macros[] = {
                "WEAPON_API", "WEAPON_EXPORT", "__declspec(dllexport)",
                "__declspec(dllimport)", "DllExport", "DllImport", "extern", "inline"
            };
            for (const char* mac : macros) {
                size_t pos;
                std::string ms(mac);
                while ((pos = cleaned.find(ms)) != std::string::npos) {
                    cleaned.erase(pos, ms.size());
                }
            }
            d.returnType = TrimCopy(cleaned);
            if (d.returnType.empty()) d.returnType = "int";
        }
        d.paramList = params;
        d.suggestedSignature = ClassifyCallSignature(d.returnType, d.paramList, d.name);
        d.suggestedPhase = SuggestCallPhase(d.name, d.suggestedSignature);
        d.fullDeclaration = d.returnType + " " + d.name + "(" + (params.empty() ? "void" : params) + ")";

        if (!seenNames.count(d.name)) {
            seenNames.insert(d.name);
            decls.push_back(d);
        }
        i = k + 1;
    }

    return decls;
}

std::vector<std::string> HeaderAnalyzer::ExtractDeclaredFunctions(const std::string& content) {
    std::vector<std::string> funcs;
    for (const auto& d : ExtractFunctionDeclarations(content)) {
        funcs.push_back(d.name);
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

    if (content.find("extern \"C\"") != std::string::npos || content.find("extern 'C'") != std::string::npos) {
        report.hasExternC = true;
        report.logMessages.push_back("PASS: 头文件包含 extern \"C\" 声明，兼容 C/C++ 规范");
    } else {
        report.hasExternC = false;
        report.logMessages.push_back("WARN: 头文件未显式包含 extern \"C\" 声明");
    }

    if (content.find("__declspec") != std::string::npos || content.find("dllexport") != std::string::npos || content.find("dllimport") != std::string::npos) {
        report.hasDeclspec = true;
        report.logMessages.push_back("PASS: 头文件包含 __declspec(dllexport/dllimport) 导出宏定义");
    } else {
        report.hasDeclspec = false;
        report.logMessages.push_back("INFO: 未检测到 __declspec 显式导出关键字");
    }

    if (content.find("#pragma pack") != std::string::npos) {
        report.hasPackDirective = true;
        report.logMessages.push_back("INFO: 检测到 #pragma pack 结构体对齐指令");
    }

    report.functionDecls = ExtractFunctionDeclarations(content);
    report.declaredFunctions.clear();
    for (const auto& d : report.functionDecls) {
        report.declaredFunctions.push_back(d.name);
    }
    report.logMessages.push_back("INFO: 从头文件中成功提取出 " + std::to_string(report.functionDecls.size()) + " 个 C 函数声明原型");

    for (const auto& d : report.functionDecls) {
        report.logMessages.push_back("  -> " + d.fullDeclaration
            + "  [约定:" + InterfaceMapping::SignatureName(d.suggestedSignature)
            + ", 阶段:" + InterfaceMapping::PhaseName(d.suggestedPhase) + "]");
    }

    report.detectedApiStyle = DetectApiStyle(content, &report.suggestedMapping, &report.apiStyleDescription);
    report.logMessages.push_back("INFO: 接口风格判定: " + report.apiStyleDescription);
    report.logMessages.push_back("INFO: 建议调用序列共 " + std::to_string(report.suggestedMapping.entries.size()) + " 步:");
    for (size_t i = 0; i < report.suggestedMapping.entries.size(); ++i) {
        const auto& e = report.suggestedMapping.entries[i];
        report.logMessages.push_back("  [" + std::to_string(i + 1) + "] "
            + (e.enabled ? "ON " : "OFF ")
            + InterfaceMapping::PhaseName(e.phase) + " | "
            + e.symbolName + " | "
            + InterfaceMapping::SignatureName(e.signature)
            + " | slot=" + std::to_string(e.handleSlot));
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
