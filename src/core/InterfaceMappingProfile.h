#ifndef INTERFACE_MAPPING_PROFILE_H
#define INTERFACE_MAPPING_PROFILE_H

#include <string>
#include <vector>

enum class MappingValueSource {
    Handle = 0,
    InputStructPointer,
    InputStructValue,
    OutputStructPointer,
    OutputStructValue,
    FixedValue,
    RandomVariable,
    DeltaTime,
    StepIndex,
    ObjectId,
    PreviousOutput,
    NullPointer
};

struct InterfaceParameterSchema {
    std::string type;
    std::string name;
    int pointerDepth = 0;
    bool isConst = false;
};

struct InterfaceFunctionSchema {
    std::string name;
    std::string returnType;
    std::string declaration;
    std::vector<InterfaceParameterSchema> parameters;
    bool externC = false;
    bool exportedByDll = false;
};

struct InterfaceFieldSchema {
    std::string type;
    std::string name;
    int arrayLength = 0;
    int pointerDepth = 0;
};

struct InterfaceStructSchema {
    std::string name;
    int pack = 0;
    std::vector<InterfaceFieldSchema> fields;
};

struct InterfaceHeaderSchema {
    std::string headerPath;
    std::vector<InterfaceFunctionSchema> functions;
    std::vector<InterfaceStructSchema> structs;
    std::vector<std::string> warnings;
};

struct ParameterBinding {
    int parameterIndex = -1;
    MappingValueSource source = MappingValueSource::NullPointer;
    std::string typeName;
    std::string value;
};

struct StructFieldBinding {
    std::string structType;
    std::string fieldPath;
    MappingValueSource source = MappingValueSource::FixedValue;
    std::string value;
    double objectOffset = 0.0;
};

struct FunctionBinding {
    std::string role;
    std::string functionName;
    std::vector<ParameterBinding> parameters;
};

struct InterfaceMappingProfile {
    std::string modelName;
    std::string dllPath;
    std::string headerPath;
    std::string libPath;
    std::string handleType = "void*";
    std::string inputStructType;
    std::string outputStructType;
    FunctionBinding createFunction;
    FunctionBinding initFunction;
    FunctionBinding stepFunction;
    FunctionBinding destroyFunction;
    std::vector<StructFieldBinding> fieldBindings;
    std::string latitudeField;
    std::string longitudeField;
    std::string statusField;
    bool mappingValidated = false;
    bool abiValidated = false;
    std::string validationMessage;

    bool SaveJson(const std::string& path, std::string& error) const;
    static bool LoadJson(const std::string& path, InterfaceMappingProfile& profile,
                         std::string& error);
};

class InterfaceSchemaAnalyzer {
public:
    static InterfaceHeaderSchema Analyze(
        const std::string& headerPath,
        const std::vector<std::string>& exportedSymbols);
    static InterfaceMappingProfile Suggest(
        const InterfaceHeaderSchema& schema,
        const std::string& modelName,
        const std::string& dllPath,
        const std::string& libPath);
    static bool Validate(const InterfaceHeaderSchema& schema,
                         InterfaceMappingProfile& profile,
                         std::vector<std::string>& errors);
    static const InterfaceFunctionSchema* FindFunction(
        const InterfaceHeaderSchema& schema, const std::string& name);
    static const InterfaceStructSchema* FindStruct(
        const InterfaceHeaderSchema& schema, const std::string& name);
    static const char* SourceName(MappingValueSource source);
    static MappingValueSource SourceFromName(const std::string& name);
};

#endif // INTERFACE_MAPPING_PROFILE_H
