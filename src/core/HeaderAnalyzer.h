#ifndef HEADER_ANALYZER_H
#define HEADER_ANALYZER_H

#include <string>
#include <vector>
#include "DllLoader.h"

struct HeaderExportConsistency {
    std::vector<std::string> declaredInHeader;
    std::vector<std::string> exportedInBinary;

    std::vector<std::string> matchedFunctions;
    std::vector<std::string> declaredButNotExported;
    std::vector<std::string> exportedButNotDeclared;

    double consistencyRatio = 100.0;
    bool isFullyConsistent = true;
    std::vector<std::string> logMessages;
};

// One C function prototype extracted from a header (for UI selection)
struct HeaderFunctionDecl {
    std::string name;
    std::string returnType;
    std::string paramList;       // text inside (...)
    std::string fullDeclaration; // display: "int Model_Init(const WeaponModelParams*)"
    CallSignature suggestedSignature = CallSignature::IntNoArg;
    CallPhase suggestedPhase = CallPhase::Setup;
};

struct HeaderAnalysisReport {
    std::string filePath;
    std::string encoding;
    bool hasExternC = false;
    bool hasDeclspec = false;
    bool hasPackDirective = false;
    std::vector<std::string> declaredFunctions;
    std::vector<HeaderFunctionDecl> functionDecls;
    ModelApiStyle detectedApiStyle = ModelApiStyle::Unknown;
    std::string apiStyleDescription;
    InterfaceMapping suggestedMapping;
    bool overallPass = true;
    std::vector<std::string> logMessages;
};

struct HeaderConflictIssue {
    std::string category;   // DUPLICATE_TYPE / ODR_CONFLICT / NAMESPACE_POLLUTION
    std::string severity;   // FAIL / WARNING
    std::string symbol;
    std::vector<std::string> files;
    std::string detail;
};

struct HeaderConflictReport {
    int duplicateTypeCount = 0;
    int odrConflictCount = 0;
    int namespacePollutionCount = 0;
    bool overallPass = true;
    std::vector<HeaderConflictIssue> issues;
    std::vector<std::string> logMessages;
};

class HeaderAnalyzer {
public:
    static HeaderAnalysisReport AnalyzeHeader(const std::string& headerPath);
    static HeaderConflictReport AnalyzeHeaderSet(const std::vector<std::string>& headerPaths);
    static std::vector<std::string> ExtractDeclaredFunctions(const std::string& headerContent);
    static std::vector<HeaderFunctionDecl> ExtractFunctionDeclarations(const std::string& headerContent);
    static CallSignature ClassifyCallSignature(const std::string& returnType,
                                               const std::string& paramList,
                                               const std::string& funcName);
    static CallPhase SuggestCallPhase(const std::string& funcName, CallSignature sig);
    static HeaderExportConsistency VerifyConsistency(const std::vector<std::string>& headerDeclaredFuncs,
                                                     const std::vector<std::string>& binaryExportedSymbols);

    static ModelApiStyle DetectApiStyle(const std::string& headerContent,
                                        InterfaceMapping* outMapping = nullptr,
                                        std::string* outDescription = nullptr);
};

#endif // HEADER_ANALYZER_H
