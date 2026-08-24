#ifndef LIB_ANALYZER_H
#define LIB_ANALYZER_H

#include <string>
#include <vector>

struct LibAnalysisReport {
    std::string filePath;
    std::string architecture;   // "x64", "x86", "Unknown"
    bool is64Bit;
    std::string libType;        // "Import Library (.lib 导入库)", "Static Library (.lib 静态库)", "Invalid"
    std::vector<std::string> foundSymbols;
    std::vector<std::string> missingSymbols;
    bool isArchMatch;
    bool overallPass;
    std::vector<std::string> logMessages;
};

class LibAnalyzer {
public:
    static LibAnalysisReport AnalyzeLib(const std::string& libPath,
                                         const std::vector<std::string>& requiredExports = { "Model_Init", "Model_Step", "Model_Destroy" });
};

#endif // LIB_ANALYZER_H
