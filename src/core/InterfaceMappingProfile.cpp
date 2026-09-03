#include "InterfaceMappingProfile.h"
#include "../utils/QtEncoding.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <cctype>
#include <set>

namespace {

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size()
           && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin
           && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string baseType(std::string type) {
    const std::vector<std::string> remove = {
        "const", "volatile", "struct", "class", "__restrict", "__restrict__"
    };
    for (const auto& token : remove) {
        size_t position = std::string::npos;
        while ((position = type.find(token)) != std::string::npos)
            type.erase(position, token.size());
    }
    type.erase(std::remove(type.begin(), type.end(), '*'), type.end());
    type.erase(std::remove(type.begin(), type.end(), '&'), type.end());
    return trim(type);
}

QString stripComments(QString text) {
    text.replace(QRegularExpression(QStringLiteral(R"(/\*[\s\S]*?\*/)")),
                 QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral(R"(//[^\r\n]*)")),
                 QStringLiteral(" "));
    return text;
}

QStringList splitParameters(const QString& text) {
    QStringList result;
    QString current;
    int nesting = 0;
    for (const QChar character : text) {
        if (character == '(' || character == '[' || character == '<') ++nesting;
        if (character == ')' || character == ']' || character == '>') --nesting;
        if (character == ',' && nesting == 0) {
            result.push_back(current.trimmed());
            current.clear();
        } else {
            current += character;
        }
    }
    if (!current.trimmed().isEmpty()) result.push_back(current.trimmed());
    return result;
}

QJsonObject parameterToJson(const ParameterBinding& binding) {
    QJsonObject object;
    object[QStringLiteral("index")] = binding.parameterIndex;
    object[QStringLiteral("source")] =
        QString::fromLatin1(InterfaceSchemaAnalyzer::SourceName(binding.source));
    object[QStringLiteral("type")] = qUtf8(binding.typeName);
    object[QStringLiteral("value")] = qUtf8(binding.value);
    return object;
}

ParameterBinding parameterFromJson(const QJsonObject& object) {
    ParameterBinding binding;
    binding.parameterIndex = object[QStringLiteral("index")].toInt(-1);
    binding.source = InterfaceSchemaAnalyzer::SourceFromName(
        qToUtf8(object[QStringLiteral("source")].toString()));
    binding.typeName = qToUtf8(object[QStringLiteral("type")].toString());
    binding.value = qToUtf8(object[QStringLiteral("value")].toString());
    return binding;
}

QJsonObject functionToJson(const FunctionBinding& binding) {
    QJsonObject object;
    object[QStringLiteral("role")] = qUtf8(binding.role);
    object[QStringLiteral("name")] = qUtf8(binding.functionName);
    QJsonArray parameters;
    for (const auto& parameter : binding.parameters)
        parameters.push_back(parameterToJson(parameter));
    object[QStringLiteral("parameters")] = parameters;
    return object;
}

FunctionBinding functionFromJson(const QJsonObject& object) {
    FunctionBinding binding;
    binding.role = qToUtf8(object[QStringLiteral("role")].toString());
    binding.functionName = qToUtf8(object[QStringLiteral("name")].toString());
    for (const auto& value : object[QStringLiteral("parameters")].toArray())
        binding.parameters.push_back(parameterFromJson(value.toObject()));
    return binding;
}

} // namespace

const char* InterfaceSchemaAnalyzer::SourceName(MappingValueSource source) {
    switch (source) {
    case MappingValueSource::Handle: return "Handle";
    case MappingValueSource::InputStructPointer: return "InputStructPointer";
    case MappingValueSource::InputStructValue: return "InputStructValue";
    case MappingValueSource::OutputStructPointer: return "OutputStructPointer";
    case MappingValueSource::OutputStructValue: return "OutputStructValue";
    case MappingValueSource::FixedValue: return "FixedValue";
    case MappingValueSource::RandomVariable: return "RandomVariable";
    case MappingValueSource::DeltaTime: return "DeltaTime";
    case MappingValueSource::StepIndex: return "StepIndex";
    case MappingValueSource::ObjectId: return "ObjectId";
    case MappingValueSource::PreviousOutput: return "PreviousOutput";
    case MappingValueSource::NullPointer: return "NullPointer";
    }
    return "NullPointer";
}

MappingValueSource InterfaceSchemaAnalyzer::SourceFromName(const std::string& name) {
    for (int i = static_cast<int>(MappingValueSource::Handle);
         i <= static_cast<int>(MappingValueSource::NullPointer); ++i) {
        const auto source = static_cast<MappingValueSource>(i);
        if (name == SourceName(source)) return source;
    }
    return MappingValueSource::NullPointer;
}

bool InterfaceMappingProfile::SaveJson(const std::string& path,
                                       std::string& error) const {
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("modelName")] = qUtf8(modelName);
    root[QStringLiteral("dllPath")] = qUtf8(dllPath);
    root[QStringLiteral("headerPath")] = qUtf8(headerPath);
    root[QStringLiteral("libPath")] = qUtf8(libPath);
    root[QStringLiteral("handleType")] = qUtf8(handleType);
    root[QStringLiteral("inputStructType")] = qUtf8(inputStructType);
    root[QStringLiteral("outputStructType")] = qUtf8(outputStructType);
    root[QStringLiteral("latitudeField")] = qUtf8(latitudeField);
    root[QStringLiteral("longitudeField")] = qUtf8(longitudeField);
    root[QStringLiteral("statusField")] = qUtf8(statusField);
    root[QStringLiteral("mappingValidated")] = mappingValidated;
    root[QStringLiteral("abiValidated")] = abiValidated;
    root[QStringLiteral("validationMessage")] = qUtf8(validationMessage);
    root[QStringLiteral("create")] = functionToJson(createFunction);
    root[QStringLiteral("init")] = functionToJson(initFunction);
    root[QStringLiteral("step")] = functionToJson(stepFunction);
    root[QStringLiteral("destroy")] = functionToJson(destroyFunction);
    QJsonArray fields;
    for (const auto& field : fieldBindings) {
        QJsonObject object;
        object[QStringLiteral("structType")] = qUtf8(field.structType);
        object[QStringLiteral("fieldPath")] = qUtf8(field.fieldPath);
        object[QStringLiteral("source")] =
            QString::fromLatin1(InterfaceSchemaAnalyzer::SourceName(field.source));
        object[QStringLiteral("value")] = qUtf8(field.value);
        object[QStringLiteral("objectOffset")] = field.objectOffset;
        fields.push_back(object);
    }
    root[QStringLiteral("fields")] = fields;

    QFile file(qUtf8(path));
    if (!QFileInfo(file).dir().mkpath(QStringLiteral("."))) {
        error = "无法创建映射配置目录";
        return false;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = "无法写入映射配置: " + path;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool InterfaceMappingProfile::LoadJson(
    const std::string& path, InterfaceMappingProfile& profile,
    std::string& error) {
    QFile file(qUtf8(path));
    if (!file.open(QIODevice::ReadOnly)) {
        error = "无法读取映射配置: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        error = "映射配置 JSON 无效";
        return false;
    }
    const QJsonObject root = document.object();
    profile = InterfaceMappingProfile();
    profile.modelName = qToUtf8(root[QStringLiteral("modelName")].toString());
    profile.dllPath = qToUtf8(root[QStringLiteral("dllPath")].toString());
    profile.headerPath = qToUtf8(root[QStringLiteral("headerPath")].toString());
    profile.libPath = qToUtf8(root[QStringLiteral("libPath")].toString());
    profile.handleType = qToUtf8(root[QStringLiteral("handleType")].toString());
    profile.inputStructType = qToUtf8(root[QStringLiteral("inputStructType")].toString());
    profile.outputStructType = qToUtf8(root[QStringLiteral("outputStructType")].toString());
    profile.latitudeField = qToUtf8(root[QStringLiteral("latitudeField")].toString());
    profile.longitudeField = qToUtf8(root[QStringLiteral("longitudeField")].toString());
    profile.statusField = qToUtf8(root[QStringLiteral("statusField")].toString());
    profile.mappingValidated = root[QStringLiteral("mappingValidated")].toBool();
    profile.abiValidated = root[QStringLiteral("abiValidated")].toBool();
    profile.validationMessage =
        qToUtf8(root[QStringLiteral("validationMessage")].toString());
    profile.createFunction = functionFromJson(root[QStringLiteral("create")].toObject());
    profile.initFunction = functionFromJson(root[QStringLiteral("init")].toObject());
    profile.stepFunction = functionFromJson(root[QStringLiteral("step")].toObject());
    profile.destroyFunction = functionFromJson(root[QStringLiteral("destroy")].toObject());
    for (const auto& value : root[QStringLiteral("fields")].toArray()) {
        const QJsonObject object = value.toObject();
        StructFieldBinding field;
        field.structType = qToUtf8(object[QStringLiteral("structType")].toString());
        field.fieldPath = qToUtf8(object[QStringLiteral("fieldPath")].toString());
        field.source = InterfaceSchemaAnalyzer::SourceFromName(
            qToUtf8(object[QStringLiteral("source")].toString()));
        field.value = qToUtf8(object[QStringLiteral("value")].toString());
        field.objectOffset = object[QStringLiteral("objectOffset")].toDouble();
        profile.fieldBindings.push_back(field);
    }
    return true;
}

InterfaceHeaderSchema InterfaceSchemaAnalyzer::Analyze(
    const std::string& headerPath,
    const std::vector<std::string>& exportedSymbols) {
    InterfaceHeaderSchema schema;
    schema.headerPath = headerPath;
    QFile file(qUtf8(headerPath));
    if (!file.open(QIODevice::ReadOnly)) {
        schema.warnings.push_back("无法读取头文件");
        return schema;
    }
    const QString source = stripComments(QString::fromUtf8(file.readAll()));
    const bool hasExternC = source.contains(
        QRegularExpression(QStringLiteral(R"(extern\s*"C")")));
    int currentPack = 0;
    QRegularExpression packExpression(
        QStringLiteral(R"(#\s*pragma\s+pack\s*\(\s*(?:push\s*,\s*)?(\d+))"));
    const auto packMatch = packExpression.match(source);
    if (packMatch.hasMatch()) currentPack = packMatch.captured(1).toInt();

    QRegularExpression structExpression(QStringLiteral(
        R"((?:typedef\s+)?(?:struct|class)\s*([A-Za-z_]\w*)?\s*\{([\s\S]*?)\}\s*([A-Za-z_]\w*)?\s*;)"));
    auto structIterator = structExpression.globalMatch(source);
    while (structIterator.hasNext()) {
        const auto match = structIterator.next();
        InterfaceStructSchema structure;
        structure.name = qToUtf8(
            !match.captured(3).isEmpty() ? match.captured(3) : match.captured(1));
        structure.pack = currentPack;
        const QString body = match.captured(2);
        for (const QString& rawField : body.split(';', Qt::SkipEmptyParts)) {
            QString fieldText = rawField.trimmed();
            if (fieldText.isEmpty() || fieldText.contains('(')
                || fieldText.contains('{')) continue;
            QRegularExpression fieldExpression(QStringLiteral(
                R"(^(.+?)\s+([A-Za-z_]\w*)\s*(?:\[\s*(\d+)\s*\])?$)"));
            const auto fieldMatch = fieldExpression.match(fieldText);
            if (!fieldMatch.hasMatch()) continue;
            InterfaceFieldSchema field;
            field.type = qToUtf8(fieldMatch.captured(1).trimmed());
            field.name = qToUtf8(fieldMatch.captured(2));
            field.arrayLength = fieldMatch.captured(3).toInt();
            field.pointerDepth = static_cast<int>(
                std::count(field.type.begin(), field.type.end(), '*'));
            structure.fields.push_back(field);
        }
        if (!structure.name.empty()) schema.structs.push_back(structure);
    }

    const std::set<std::string> exports(exportedSymbols.begin(), exportedSymbols.end());
    QRegularExpression functionExpression(QStringLiteral(
        R"((?:^|[\r\n])\s*([^#\r\n;{}]+?)\s+([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*;)"));
    auto functionIterator = functionExpression.globalMatch(source);
    while (functionIterator.hasNext()) {
        const auto match = functionIterator.next();
        const QString prefix = match.captured(1).trimmed();
        const QString functionName = match.captured(2);
        if (prefix.startsWith(QStringLiteral("typedef"))
            || prefix.contains(QStringLiteral("return "))) continue;
        InterfaceFunctionSchema function;
        function.name = qToUtf8(functionName);
        function.returnType = qToUtf8(prefix);
        function.declaration = qToUtf8(match.captured(0).trimmed());
        function.externC = hasExternC;
        function.exportedByDll = exports.count(function.name) > 0;
        const QString parameterText = match.captured(3).trimmed();
        if (!parameterText.isEmpty() && parameterText != QStringLiteral("void")) {
            int unnamedIndex = 0;
            for (const QString& rawParameter : splitParameters(parameterText)) {
                QString parameterTextOne = rawParameter;
                parameterTextOne.remove(
                    QRegularExpression(QStringLiteral(R"(\s*=\s*.+$)")));
                QRegularExpression nameExpression(
                    QStringLiteral(R"(([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$)"));
                const auto nameMatch = nameExpression.match(parameterTextOne);
                InterfaceParameterSchema parameter;
                if (nameMatch.hasMatch()) {
                    parameter.name = qToUtf8(nameMatch.captured(1));
                    parameter.type = qToUtf8(
                        parameterTextOne.left(nameMatch.capturedStart(1)).trimmed());
                } else {
                    parameter.name = "arg" + std::to_string(unnamedIndex);
                    parameter.type = qToUtf8(parameterTextOne.trimmed());
                }
                parameter.pointerDepth = static_cast<int>(
                    std::count(parameter.type.begin(), parameter.type.end(), '*'));
                parameter.isConst = parameter.type.find("const") != std::string::npos;
                function.parameters.push_back(parameter);
                ++unnamedIndex;
            }
        }
        schema.functions.push_back(function);
    }
    if (schema.functions.empty())
        schema.warnings.push_back("未从头文件解析到可映射函数声明");
    return schema;
}

const InterfaceFunctionSchema* InterfaceSchemaAnalyzer::FindFunction(
    const InterfaceHeaderSchema& schema, const std::string& name) {
    for (const auto& function : schema.functions)
        if (function.name == name) return &function;
    return nullptr;
}

const InterfaceStructSchema* InterfaceSchemaAnalyzer::FindStruct(
    const InterfaceHeaderSchema& schema, const std::string& name) {
    const std::string wanted = baseType(name);
    for (const auto& structure : schema.structs)
        if (structure.name == wanted) return &structure;
    return nullptr;
}

InterfaceMappingProfile InterfaceSchemaAnalyzer::Suggest(
    const InterfaceHeaderSchema& schema,
    const std::string& modelName,
    const std::string& dllPath,
    const std::string& libPath) {
    InterfaceMappingProfile profile;
    profile.modelName = modelName;
    profile.dllPath = dllPath;
    profile.headerPath = schema.headerPath;
    profile.libPath = libPath;
    profile.createFunction.role = "Create";
    profile.initFunction.role = "Init";
    profile.stepFunction.role = "Step";
    profile.destroyFunction.role = "Destroy";

    auto choose = [&](const std::vector<std::string>& words) -> const InterfaceFunctionSchema* {
        for (const auto& function : schema.functions) {
            if (!function.exportedByDll) continue;
            const std::string name = lower(function.name);
            for (const auto& word : words)
                if (name.find(word) != std::string::npos) return &function;
        }
        return nullptr;
    };
    const InterfaceFunctionSchema* create =
        choose({ "create", "new", "alloc", "open" });
    const InterfaceFunctionSchema* init =
        choose({ "init", "initialize", "setup", "boot" });
    const InterfaceFunctionSchema* step =
        choose({ "step", "update", "advance", "tick", "run" });
    const InterfaceFunctionSchema* destroy =
        choose({ "destroy", "release", "delete", "close", "free" });
    if (create) {
        profile.createFunction.functionName = create->name;
        profile.handleType = trim(create->returnType);
    }
    if (init) profile.initFunction.functionName = init->name;
    if (step) profile.stepFunction.functionName = step->name;
    if (destroy) profile.destroyFunction.functionName = destroy->name;

    auto bindFunction = [&](const InterfaceFunctionSchema* function,
                            FunctionBinding& binding, bool isStep) {
        if (!function) return;
        for (int i = 0; i < static_cast<int>(function->parameters.size()); ++i) {
            const auto& parameter = function->parameters[static_cast<size_t>(i)];
            ParameterBinding parameterBinding;
            parameterBinding.parameterIndex = i;
            parameterBinding.typeName = parameter.type;
            const std::string type = baseType(parameter.type);
            const InterfaceStructSchema* structure = FindStruct(schema, type);
            if (i == 0 && parameter.pointerDepth > 0
                && (!structure || type == baseType(profile.handleType))) {
                parameterBinding.source = MappingValueSource::Handle;
                profile.handleType = parameter.type;
            } else if (structure && parameter.pointerDepth > 0) {
                parameterBinding.source = isStep
                    ? MappingValueSource::OutputStructPointer
                    : MappingValueSource::InputStructPointer;
                if (isStep) profile.outputStructType = structure->name;
                else profile.inputStructType = structure->name;
            } else if (lower(parameter.name).find("dt") != std::string::npos) {
                parameterBinding.source = MappingValueSource::DeltaTime;
            } else if (lower(parameter.name).find("step") != std::string::npos) {
                parameterBinding.source = MappingValueSource::StepIndex;
            } else if (parameter.pointerDepth > 0) {
                parameterBinding.source = MappingValueSource::NullPointer;
            } else {
                parameterBinding.source = MappingValueSource::FixedValue;
                parameterBinding.value = "0";
            }
            binding.parameters.push_back(parameterBinding);
        }
    };
    bindFunction(create, profile.createFunction, false);
    bindFunction(init, profile.initFunction, false);
    bindFunction(step, profile.stepFunction, true);
    bindFunction(destroy, profile.destroyFunction, false);

    const InterfaceStructSchema* input = FindStruct(schema, profile.inputStructType);
    if (input) {
        for (const auto& field : input->fields) {
            StructFieldBinding binding;
            binding.structType = input->name;
            binding.fieldPath = field.name;
            const std::string name = lower(field.name);
            if (name.find("lat") != std::string::npos) {
                binding.source = MappingValueSource::RandomVariable;
                binding.value = "lat";
                binding.objectOffset = 0.001;
            } else if (name.find("lon") != std::string::npos) {
                binding.source = MappingValueSource::RandomVariable;
                binding.value = "lon";
                binding.objectOffset = 0.001;
            } else if (name.find("alt") != std::string::npos) {
                binding.source = MappingValueSource::RandomVariable;
                binding.value = "alt";
            } else if (name.find("speed") != std::string::npos
                       || name.find("velocity") != std::string::npos) {
                binding.source = MappingValueSource::RandomVariable;
                binding.value = "speed";
            } else if (name == "dt" || name.find("step_dt") != std::string::npos) {
                binding.source = MappingValueSource::DeltaTime;
            } else {
                binding.source = MappingValueSource::FixedValue;
                binding.value = "0";
            }
            profile.fieldBindings.push_back(binding);
        }
    }
    const InterfaceStructSchema* output = FindStruct(schema, profile.outputStructType);
    if (output) {
        for (const auto& field : output->fields) {
            const std::string name = lower(field.name);
            if (profile.latitudeField.empty()
                && name.find("lat") != std::string::npos)
                profile.latitudeField = field.name;
            if (profile.longitudeField.empty()
                && name.find("lon") != std::string::npos)
                profile.longitudeField = field.name;
            if (profile.statusField.empty()
                && (name == "status" || name == "state"))
                profile.statusField = field.name;
        }
    }
    return profile;
}

bool InterfaceSchemaAnalyzer::Validate(
    const InterfaceHeaderSchema& schema,
    InterfaceMappingProfile& profile,
    std::vector<std::string>& errors) {
    errors.clear();
    auto validateFunction = [&](const char* role, const FunctionBinding& binding) {
        if (binding.functionName.empty()) {
            errors.push_back(std::string(role) + " 尚未选择函数");
            return;
        }
        const auto* function = FindFunction(schema, binding.functionName);
        if (!function) {
            errors.push_back(std::string(role) + " 函数不在头文件中");
        } else if (!function->exportedByDll) {
            errors.push_back(std::string(role) + " 函数未在 DLL 导出表中找到");
        } else if (binding.parameters.size() != function->parameters.size()) {
            errors.push_back(std::string(role) + " 参数映射数量与函数原型不一致");
        }
    };
    validateFunction("Create", profile.createFunction);
    validateFunction("Init", profile.initFunction);
    validateFunction("Step", profile.stepFunction);
    validateFunction("Destroy", profile.destroyFunction);
    if (profile.outputStructType.empty())
        errors.push_back("尚未指定 Step 输出结构体");
    if (profile.latitudeField.empty() || profile.longitudeField.empty())
        errors.push_back("尚未映射输出纬度/经度字段");
    profile.mappingValidated = errors.empty();
    profile.validationMessage = errors.empty() ? "接口与字段映射完整" : errors.front();
    return errors.empty();
}
