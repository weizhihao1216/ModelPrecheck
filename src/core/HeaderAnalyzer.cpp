#include "HeaderAnalyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <set>
#include <map>

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

namespace {

struct ParsedHeaderType {
    std::string name;
    std::string qualifiedName;
    std::string kind;
    std::string normalizedBody;
    std::string file;
    size_t bodyBegin = 0;
    size_t bodyEnd = 0;
    bool globalScope = true;
};

std::string StripCommentsAndLiterals(const std::string& input) {
    std::string out(input.size(), ' ');
    enum class State { Code, LineComment, BlockComment, String, Character };
    State state = State::Code;
    bool escaped = false;
    for (size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        const char next = i + 1 < input.size() ? input[i + 1] : '\0';
        if (state == State::Code) {
            if (ch == '/' && next == '/') {
                state = State::LineComment;
                ++i;
            } else if (ch == '/' && next == '*') {
                state = State::BlockComment;
                ++i;
            } else if (ch == '"') {
                state = State::String;
                escaped = false;
            } else if (ch == '\'') {
                state = State::Character;
                escaped = false;
            } else {
                out[i] = ch;
            }
        } else if (state == State::LineComment) {
            if (ch == '\n') {
                out[i] = ch;
                state = State::Code;
            }
        } else if (state == State::BlockComment) {
            if (ch == '*' && next == '/') {
                ++i;
                state = State::Code;
            } else if (ch == '\n') {
                out[i] = ch;
            }
        } else {
            if (ch == '\n') out[i] = ch;
            if (!escaped && ((state == State::String && ch == '"')
                || (state == State::Character && ch == '\''))) {
                state = State::Code;
            }
            escaped = (!escaped && ch == '\\');
            if (ch != '\\') escaped = false;
        }
    }
    return out;
}

size_t MatchingBrace(const std::string& text, size_t opening) {
    int depth = 0;
    for (size_t i = opening; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}' && --depth == 0) return i;
    }
    return std::string::npos;
}

std::string NamespaceAt(const std::string& text, size_t position) {
    struct Scope { std::string name; };
    std::vector<Scope> scopes;
    size_t cursor = 0;
    while (cursor < position) {
        const bool boundaryBefore = cursor == 0
            || !std::isalnum(static_cast<unsigned char>(text[cursor - 1]));
        if (boundaryBefore && text.compare(cursor, 9, "namespace") == 0
            && cursor + 9 < position
            && std::isspace(static_cast<unsigned char>(text[cursor + 9]))) {
            size_t current = cursor + 9;
            while (current < position
                && std::isspace(static_cast<unsigned char>(text[current]))) ++current;
            size_t nameBegin = current;
            while (current < position
                && (std::isalnum(static_cast<unsigned char>(text[current]))
                    || text[current] == '_')) ++current;
            const std::string name = text.substr(nameBegin, current - nameBegin);
            while (current < position
                && std::isspace(static_cast<unsigned char>(text[current]))) ++current;
            if (!name.empty() && current < position && text[current] == '{') {
                scopes.push_back({ name });
                cursor = current + 1;
                continue;
            }
        }
        const char ch = text[cursor];
        if (ch == '{') scopes.push_back({ std::string() });
        else if (ch == '}' && !scopes.empty()) scopes.pop_back();
        ++cursor;
    }
    std::string result;
    for (const auto& scope : scopes) {
        if (!scope.name.empty()) {
            if (!result.empty()) result += "::";
            result += scope.name;
        }
    }
    return result;
}

std::string NormalizeTokens(const std::string& text) {
    std::string result;
    for (char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) result.push_back(ch);
    }
    return result;
}

} // namespace

HeaderConflictReport HeaderAnalyzer::AnalyzeHeaderSet(
    const std::vector<std::string>& headerPaths) {
    HeaderConflictReport report;
    std::vector<ParsedHeaderType> allTypes;

    for (const std::string& path : headerPaths) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            report.logMessages.push_back("WARN: 冲突分析无法打开头文件: " + path);
            continue;
        }
        std::stringstream stream;
        stream << file.rdbuf();
        const std::string code = StripCommentsAndLiterals(stream.str());

        std::regex typeRegex(
            R"(\b(struct|class|union|enum(?:\s+class)?)\s+([A-Za-z_]\w*)\s*(?:\:[^{;]*)?\{)");
        for (std::sregex_iterator it(code.begin(), code.end(), typeRegex), end; it != end; ++it) {
            const size_t declaration = static_cast<size_t>(it->position());
            const size_t opening = declaration + static_cast<size_t>(it->length()) - 1;
            const size_t closing = MatchingBrace(code, opening);
            if (closing == std::string::npos) continue;
            const std::string nameSpace = NamespaceAt(code, declaration);
            ParsedHeaderType type;
            type.kind = (*it)[1].str();
            type.name = (*it)[2].str();
            type.qualifiedName = nameSpace.empty() ? ("::" + type.name)
                                                   : (nameSpace + "::" + type.name);
            type.normalizedBody = NormalizeTokens(code.substr(opening, closing - opening + 1));
            type.file = path;
            type.bodyBegin = opening;
            type.bodyEnd = closing;
            type.globalScope = nameSpace.empty();
            allTypes.push_back(type);

            if (type.globalScope) {
                HeaderConflictIssue issue;
                issue.category = "NAMESPACE_POLLUTION";
                issue.severity = "WARNING";
                issue.symbol = type.name;
                issue.files.push_back(path);
                issue.detail = type.kind + " " + type.name
                    + " 定义在全局命名空间，集成多个模型时存在名称污染风险";
                report.issues.push_back(issue);
                ++report.namespacePollutionCount;
            }
        }

        std::regex usingNamespaceRegex(R"(\busing\s+namespace\s+([A-Za-z_][\w:]*)\s*;)");
        for (std::sregex_iterator it(code.begin(), code.end(), usingNamespaceRegex), end; it != end; ++it) {
            HeaderConflictIssue issue;
            issue.category = "NAMESPACE_POLLUTION";
            issue.severity = "WARNING";
            issue.symbol = (*it)[1].str();
            issue.files.push_back(path);
            issue.detail = "头文件使用 using namespace，会把命名空间成员引入所有包含该头文件的编译单元";
            report.issues.push_back(issue);
            ++report.namespacePollutionCount;
        }

        std::regex useNamespaceMacro(R"(\b([A-Z][A-Z0-9_]*_USE_NAMESPACE)\b)");
        for (std::sregex_iterator it(code.begin(), code.end(), useNamespaceMacro), end; it != end; ++it) {
            HeaderConflictIssue issue;
            issue.category = "NAMESPACE_POLLUTION";
            issue.severity = "WARNING";
            issue.symbol = (*it)[1].str();
            issue.files.push_back(path);
            issue.detail = "头文件中的命名空间展开宏可能污染包含者的全局作用域";
            report.issues.push_back(issue);
            ++report.namespacePollutionCount;
        }

        std::regex functionBodyRegex(
            R"((^|[\r\n])\s*(?!inline\b|static\b|constexpr\b|template\b)([A-Za-z_][\w:<>,\s*&~]*?)\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{)");
        for (std::sregex_iterator it(code.begin(), code.end(), functionBodyRegex), end; it != end; ++it) {
            const size_t position = static_cast<size_t>(it->position());
            bool insideType = false;
            for (const auto& type : allTypes) {
                if (type.file == path && position > type.bodyBegin && position < type.bodyEnd) {
                    insideType = true;
                    break;
                }
            }
            if (insideType) continue;
            HeaderConflictIssue issue;
            issue.category = "ODR_CONFLICT";
            issue.severity = "FAIL";
            issue.symbol = (*it)[3].str();
            issue.files.push_back(path);
            issue.detail = "头文件中定义了非 inline/static 的函数体，多翻译单元包含时可能违反 ODR";
            report.issues.push_back(issue);
            ++report.odrConflictCount;
        }
    }

    std::map<std::string, std::vector<const ParsedHeaderType*>> byQualifiedName;
    for (const auto& type : allTypes) byQualifiedName[type.qualifiedName].push_back(&type);
    for (const auto& entry : byQualifiedName) {
        std::set<std::string> files;
        std::set<std::string> bodies;
        for (const auto* type : entry.second) {
            files.insert(type->file);
            bodies.insert(type->normalizedBody);
        }
        if (entry.second.size() < 2) continue;
        HeaderConflictIssue issue;
        issue.symbol = entry.first;
        issue.files.assign(files.begin(), files.end());
        if (bodies.size() > 1) {
            issue.category = "ODR_CONFLICT";
            issue.severity = "FAIL";
            issue.detail = "同名类型在多个头文件中具有不同定义，属于疑似 ODR 冲突";
            ++report.odrConflictCount;
        } else {
            issue.category = "DUPLICATE_TYPE";
            issue.severity = "FAIL";
            issue.detail = "同名类型在多个头文件中重复定义，合并包含时可能发生重定义";
            ++report.duplicateTypeCount;
        }
        report.issues.push_back(issue);
    }

    report.overallPass = report.duplicateTypeCount == 0 && report.odrConflictCount == 0;
    report.logMessages.push_back(
        "INFO: 头文件集合冲突检查完成，重复类型=" + std::to_string(report.duplicateTypeCount)
        + "，ODR 冲突=" + std::to_string(report.odrConflictCount)
        + "，命名空间污染风险=" + std::to_string(report.namespacePollutionCount));
    for (const auto& issue : report.issues) {
        report.logMessages.push_back(issue.severity + ": [" + issue.category + "] "
            + issue.symbol + " - " + issue.detail);
    }
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
