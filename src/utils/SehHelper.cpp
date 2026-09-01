#include "SehHelper.h"
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
