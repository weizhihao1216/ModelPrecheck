#include "SehHelper.h"
#include <atomic>
#include <sstream>

std::string SehCodeToString(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION (0xC0000005): Memory read/write access violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (0xC000008C): Out of bounds array index";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT (0xC000008E): Data alignment fault";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND (0xC000008D): Floating point denormal operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO (0xC000008E): Floating point divide by zero";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "EXCEPTION_FLT_INEXACT_RESULT (0xC000008F): Floating point inexact result";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION (0xC0000090): Floating point invalid operation";
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW (0xC0000091): Floating point overflow";
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK (0xC0000092): Floating point stack overflow/underflow";
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW (0xC0000093): Floating point underflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094): Integer divide by zero";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW (0xC0000095): Integer overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D): Illegal CPU instruction";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR (0xC0000006): Page fault / I/O error";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW (0xC00000FD): Stack overflow exception";
    default: {
        std::stringstream ss;
        ss << "UNKNOWN_HARDWARE_EXCEPTION (0x" << std::hex << code << ")";
        return ss.str();
    }
    }
}

static DWORD FilterSehException(DWORD code) {
    return EXCEPTION_EXECUTE_HANDLER;
}

bool SafeCallVoidNoArg(FnVoidNoArg fn, DWORD* outExceptionCode) {
    if (!fn) return false;
    __try {
        fn();
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallIntNoArg(FnIntNoArg fn, int* outResult, DWORD* outExceptionCode) {
    if (!fn) return false;
    __try {
        int res = fn();
        if (outResult) *outResult = res;
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallInit(FnModelInit fn, const WeaponModelParams* params, int* outResult, DWORD* outExceptionCode) {
    if (!fn) return false;
    __try {
        int res = fn(params);
        if (outResult) *outResult = res;
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallStep(FnModelStep fn, WeaponModelOutput* output, int* outResult, DWORD* outExceptionCode) {
    if (!fn) return false;
    __try {
        int res = fn(output);
        if (outResult) *outResult = res;
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallDestroy(FnModelDestroy fn, DWORD* outExceptionCode) {
    if (!fn) return false;
    __try {
        fn();
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallGetInfo(FnModelGetInfo fn, std::string& outInfo, DWORD* outExceptionCode) {
    if (!fn) return false;
    const char* str = nullptr;
    __try {
        str = fn();
        if (outExceptionCode) *outExceptionCode = 0;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }

    if (str) {
        outInfo = str;
    } else {
        outInfo = "";
    }
    return true;
}

bool SafeCallCreate(FnModelCreate fn, ModelHandle* outHandle, DWORD* outExceptionCode) {
    if (!fn || !outHandle) return false;
    __try {
        *outHandle = fn();
        if (outExceptionCode) *outExceptionCode = 0;
        return (*outHandle != nullptr);
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        *outHandle = nullptr;
        return false;
    }
}

bool SafeCallInitEx(FnModelInitEx fn, ModelHandle handle, const WeaponModelParams* params, int* outResult, DWORD* outExceptionCode) {
    if (!fn || !handle) return false;
    __try {
        int res = fn(handle, params);
        if (outResult) *outResult = res;
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallStepEx(FnModelStepEx fn, ModelHandle handle, WeaponModelOutput* output, int* outResult, DWORD* outExceptionCode) {
    if (!fn || !handle) return false;
    __try {
        int res = fn(handle, output);
        if (outResult) *outResult = res;
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

bool SafeCallDestroyEx(FnModelDestroyEx fn, ModelHandle handle, DWORD* outExceptionCode) {
    if (!fn || !handle) return false;
    __try {
        fn(handle);
        if (outExceptionCode) *outExceptionCode = 0;
        return true;
    }
    __except (FilterSehException(GetExceptionCode())) {
        if (outExceptionCode) *outExceptionCode = GetExceptionCode();
        return false;
    }
}

namespace {

std::atomic<bool> g_loadedPrivateModule(false);
std::atomic<unsigned> g_privateCopySeq(0);

std::wstring Utf8ToWidePath(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 1) return std::wstring();
    std::wstring wide(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], n);
    return wide;
}

std::string DirectoryOfPath(const std::string& path) {
    const size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string FileStemOfPath(const std::string& path) {
    const size_t slash = path.find_last_of("\\/");
    const std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

/** 独立函数里用 SEH 兜住加载异常（带 C++ 对象的函数不能直接写 __try）。 */
HMODULE LoadLibraryGuarded(const wchar_t* path) {
    HMODULE module = nullptr;
    __try {
        module = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) module = LoadLibraryW(path);
    } __except (FilterSehException(GetExceptionCode())) {
        module = nullptr;
    }
    return module;
}

} // namespace

HMODULE LoadPrivateModuleCopy(const std::string& sourcePath, const std::string& copyDir,
                              std::string* outCopyPath, std::string* error) {
    if (sourcePath.empty()) {
        if (error) *error = "源 DLL 路径为空";
        return nullptr;
    }
    const std::string dir = copyDir.empty() ? DirectoryOfPath(sourcePath) : copyDir;
    const std::string stem = FileStemOfPath(sourcePath);
    if (dir.empty() || stem.empty()) {
        if (error) *error = "无法解析 DLL 路径：" + sourcePath;
        return nullptr;
    }

    const std::string copyPath = dir + "\\" + stem + "__run"
        + std::to_string(GetCurrentProcessId()) + "_"
        + std::to_string(++g_privateCopySeq) + ".dll";
    const std::wstring wSource = Utf8ToWidePath(sourcePath);
    const std::wstring wCopy = Utf8ToWidePath(copyPath);
    if (wSource.empty() || wCopy.empty() || !CopyFileW(wSource.c_str(), wCopy.c_str(), FALSE)) {
        if (error) *error = "复制被测模块失败, GetLastError=" + std::to_string(GetLastError());
        return nullptr;
    }

    HMODULE module = LoadLibraryGuarded(wCopy.c_str());
    if (!module) {
        // 没有加载成功，副本文件可以直接删掉，不必留到下次启动。
        DeleteFileW(wCopy.c_str());
        if (error) *error = "加载被测模块副本失败, GetLastError=" + std::to_string(GetLastError());
        return nullptr;
    }

    g_loadedPrivateModule.store(true);
    if (outCopyPath) *outCopyPath = copyPath;
    return module;
}

bool HasLoadedPrivateModule() {
    return g_loadedPrivateModule.load();
}

void CleanupPrivateModuleCopies(const std::string& modelsRootDir) {
    if (modelsRootDir.empty()) return;
    const std::string dirPattern = modelsRootDir + "\\*";
    WIN32_FIND_DATAA dirData{};
    HANDLE dirFind = FindFirstFileA(dirPattern.c_str(), &dirData);
    if (dirFind == INVALID_HANDLE_VALUE) return;
    do {
        if ((dirData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
        const std::string name = dirData.cFileName;
        if (name == "." || name == "..") continue;
        const std::string dir = modelsRootDir + "\\" + name;
        WIN32_FIND_DATAA fileData{};
        HANDLE fileFind = FindFirstFileA((dir + "\\*__run*.dll").c_str(), &fileData);
        if (fileFind == INVALID_HANDLE_VALUE) continue;
        do {
            DeleteFileA((dir + "\\" + std::string(fileData.cFileName)).c_str());
        } while (FindNextFileA(fileFind, &fileData));
        FindClose(fileFind);
    } while (FindNextFileA(dirFind, &dirData));
    FindClose(dirFind);
}
