#ifndef PE_ANALYZER_H
#define PE_ANALYZER_H

#include <string>
#include <vector>
#include <map>

struct ImportedDllInfo {
    std::string name;
    bool found;
    std::string resolvedPath;
};

struct ExportedSymbolInfo {
    std::string name;
    uint32_t ordinal;
    uint32_t rva;
    bool isRequiredInterface;
};

struct PeAnalysisReport {
    std::string filePath;
    std::string architecture; // "x64" or "x86" or "Unknown"
    bool is64Bit;
    std::string crtLinkage;    // "Dynamic (MD/MDd)", "Static (MT/MTd)", or "Unknown"
    std::vector<ImportedDllInfo> importedDlls;
    std::vector<ExportedSymbolInfo> exportedSymbols;
    
    // Check results
    bool isArchMatch;          // True if matches current process (x64)
    int missingDependencyCount;
    int missingExportCount;
    bool overallPass;
    std::vector<std::string> logMessages;
};

class PeAnalyzer {
public:
    static PeAnalysisReport AnalyzeDll(const std::string& dllPath,
                                       const std::vector<std::string>& extraSearchPaths = {},
                                       const std::vector<std::string>& requiredExports = { "Model_Init", "Model_Step", "Model_Destroy" });

private:
    static bool ResolveDllLocation(const std::string& dllName,
                                   const std::string& targetDllDir,
                                   const std::vector<std::string>& extraSearchPaths,
                                   std::string& outPath);
};

#endif // PE_ANALYZER_H
