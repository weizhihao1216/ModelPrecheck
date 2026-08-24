#ifndef HEADER_ANALYZER_H
#define HEADER_ANALYZER_H

#include <string>
#include <vector>

struct HeaderExportConsistency {
    std::vector<std::string> declaredInHeader;
    std::vector<std::string> exportedInBinary;

    std::vector<std::string> matchedFunctions;       // In both header and binary export table
    std::vector<std::string> declaredButNotExported; // In header, missing in DLL/LIB
    std::vector<std::string> exportedButNotDeclared; // In DLL/LIB, missing in header

    double consistencyRatio = 100.0;
    bool isFullyConsistent = true;
    std::vector<std::string> logMessages;
};

struct HeaderAnalysisReport {
    std::string filePath;
    std::string encoding;       // "UTF-8", "UTF-8 BOM", "GBK", "ASCII"
    bool hasExternC;            // True if extern "C" guard exists
    bool hasDeclspec;           // True if __declspec(dllexport/dllimport) macro exists
    bool hasPackDirective;      // True if #pragma pack is used
    std::vector<std::string> declaredFunctions; // All extracted function names
    bool overallPass = true;
    std::vector<std::string> logMessages;
};

class HeaderAnalyzer {
public:
    static HeaderAnalysisReport AnalyzeHeader(const std::string& headerPath);
    static std::vector<std::string> ExtractDeclaredFunctions(const std::string& headerContent);
    static HeaderExportConsistency VerifyConsistency(const std::vector<std::string>& headerDeclaredFuncs,
                                                     const std::vector<std::string>& binaryExportedSymbols);
};

#endif // HEADER_ANALYZER_H
