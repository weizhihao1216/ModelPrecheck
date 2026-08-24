#include "PeAnalyzer.h"
#include <windows.h>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>

static DWORD RvaToFileOffset(DWORD rva, PIMAGE_SECTION_HEADER pSectionHeader, WORD numberOfSections) {
    for (WORD i = 0; i < numberOfSections; ++i) {
        DWORD vAddr = pSectionHeader[i].VirtualAddress;
        DWORD vSize = pSectionHeader[i].Misc.VirtualSize;
        if (vSize == 0) vSize = pSectionHeader[i].SizeOfRawData;

        if (rva >= vAddr && rva < vAddr + vSize) {
            return rva - vAddr + pSectionHeader[i].PointerToRawData;
        }
    }
    return 0;
}

PeAnalysisReport PeAnalyzer::AnalyzeDll(const std::string& dllPath,
                                       const std::vector<std::string>& extraSearchPaths,
                                       const std::vector<std::string>& requiredExports) {
    PeAnalysisReport report;
    report.filePath = dllPath;
    report.is64Bit = false;
    report.isArchMatch = false;
    report.missingDependencyCount = 0;
    report.missingExportCount = 0;
    report.overallPass = false;
    report.architecture = "Unknown";
    report.crtLinkage = "Unknown";

    // Extract target directory from DLL path
    std::string targetDir = "";
    size_t lastSlash = dllPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        targetDir = dllPath.substr(0, lastSlash);
    }

    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        report.logMessages.push_back("ERROR: Failed to open DLL file for PE static analysis: " + dllPath);
        return report;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize)) {
        report.logMessages.push_back("ERROR: Failed to read binary content from DLL.");
        return report;
    }

    BYTE* pBase = reinterpret_cast<BYTE*>(buffer.data());
    PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(pBase);

    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        report.logMessages.push_back("ERROR: Invalid PE header (DOS signature 'MZ' missing).");
        return report;
    }

    if (pDosHeader->e_lfanew < 0 || pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS32) > static_cast<size_t>(fileSize)) {
        report.logMessages.push_back("ERROR: Invalid e_lfanew header offset.");
        return report;
    }

    DWORD dwNtHeaderOffset = pDosHeader->e_lfanew;
    PIMAGE_NT_HEADERS32 pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pBase + dwNtHeaderOffset);

    if (pNtHeaders32->Signature != IMAGE_NT_SIGNATURE) {
        report.logMessages.push_back("ERROR: Invalid PE signature 'PE\\0\\0'.");
        return report;
    }

    WORD machine = pNtHeaders32->FileHeader.Machine;
    PIMAGE_SECTION_HEADER pSectionHeader = nullptr;
    WORD numberOfSections = pNtHeaders32->FileHeader.NumberOfSections;
    IMAGE_DATA_DIRECTORY importDataDir = { 0, 0 };
    IMAGE_DATA_DIRECTORY exportDataDir = { 0, 0 };

    if (machine == IMAGE_FILE_MACHINE_AMD64) {
        report.architecture = "x64 (AMD64)";
        report.is64Bit = true;
        PIMAGE_NT_HEADERS64 pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pBase + dwNtHeaderOffset);
        pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders64);
        importDataDir = pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        exportDataDir = pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else if (machine == IMAGE_FILE_MACHINE_I386) {
        report.architecture = "x86 (i386)";
        report.is64Bit = false;
        pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders32);
        importDataDir = pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        exportDataDir = pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        report.architecture = "Unsupported Architecture";
        report.logMessages.push_back("ERROR: Unsupported CPU Machine type: " + std::to_string(machine));
        return report;
    }

    // Check host process architecture (this application is 64-bit)
#if defined(_WIN64)
    report.isArchMatch = report.is64Bit;
#else
    report.isArchMatch = !report.is64Bit;
#endif

    if (!report.isArchMatch) {
        report.logMessages.push_back("WARNING: Architecture mismatch! Validator is 64-bit, but DLL is " + report.architecture);
    } else {
        report.logMessages.push_back("INFO: Binary architecture matches host process (" + report.architecture + ").");
    }

    // --- Parse Imports & CRT Linkage ---
    bool hasCrtDll = false;
    std::string detectedCrt = "";

    if (importDataDir.VirtualAddress != 0 && importDataDir.Size != 0) {
        DWORD importOffset = RvaToFileOffset(importDataDir.VirtualAddress, pSectionHeader, numberOfSections);
        if (importOffset != 0 && importOffset + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= static_cast<size_t>(fileSize)) {
            PIMAGE_IMPORT_DESCRIPTOR pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(pBase + importOffset);
            while (pImportDesc->Name != 0) {
                DWORD nameOffset = RvaToFileOffset(pImportDesc->Name, pSectionHeader, numberOfSections);
                if (nameOffset != 0 && nameOffset < static_cast<size_t>(fileSize)) {
                    const char* dllName = reinterpret_cast<const char*>(pBase + nameOffset);
                    ImportedDllInfo info;
                    info.name = dllName;
                    info.found = ResolveDllLocation(dllName, targetDir, extraSearchPaths, info.resolvedPath);

                    if (!info.found) {
                        report.missingDependencyCount++;
                    }

                    // Check CRT DLL indicators
                    std::string upperDllName = dllName;
                    std::transform(upperDllName.begin(), upperDllName.end(), upperDllName.begin(), ::toupper);
                    if (upperDllName.find("VCRUNTIME") != std::string::npos ||
                        upperDllName.find("MSVCP") != std::string::npos ||
                        upperDllName.find("MSVCR") != std::string::npos ||
                        upperDllName.find("UCRTBASE") != std::string::npos) {
                        hasCrtDll = true;
                        if (detectedCrt.empty()) detectedCrt = dllName;
                        else detectedCrt += ", " + std::string(dllName);
                    }

                    report.importedDlls.push_back(info);
                }
                pImportDesc++;
            }
        }
    }

    if (hasCrtDll) {
        report.crtLinkage = "Dynamic CRT (/MD or /MDd) [" + detectedCrt + "]";
    } else {
        report.crtLinkage = "Static CRT (/MT or /MTd) or No CRT dependency";
    }
    report.logMessages.push_back("INFO: CRT Linkage identified as: " + report.crtLinkage);

    // Parse Export Table
    std::vector<std::string> exportedNames;
    if (exportDataDir.VirtualAddress != 0 && exportDataDir.Size != 0) {
        DWORD exportOffset = RvaToFileOffset(exportDataDir.VirtualAddress, pSectionHeader, numberOfSections);
        if (exportOffset != 0 && exportOffset + sizeof(IMAGE_EXPORT_DIRECTORY) <= static_cast<size_t>(fileSize)) {
            PIMAGE_EXPORT_DIRECTORY pExportDir = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(pBase + exportOffset);
            DWORD namesOffset = RvaToFileOffset(pExportDir->AddressOfNames, pSectionHeader, numberOfSections);
            DWORD ordinalsOffset = RvaToFileOffset(pExportDir->AddressOfNameOrdinals, pSectionHeader, numberOfSections);

            if (namesOffset != 0 && ordinalsOffset != 0) {
                DWORD* pNames = reinterpret_cast<DWORD*>(pBase + namesOffset);
                WORD* pOrdinals = reinterpret_cast<WORD*>(pBase + ordinalsOffset);

                for (DWORD i = 0; i < pExportDir->NumberOfNames; ++i) {
                    DWORD nameStrOffset = RvaToFileOffset(pNames[i], pSectionHeader, numberOfSections);
                    if (nameStrOffset != 0 && nameStrOffset < static_cast<size_t>(fileSize)) {
                        std::string expName = reinterpret_cast<const char*>(pBase + nameStrOffset);
                        ExportedSymbolInfo symInfo;
                        symInfo.name = expName;
                        symInfo.ordinal = pExportDir->Base + pOrdinals[i];
                        symInfo.rva = 0;
                        symInfo.isRequiredInterface = false;

                        for (const auto& req : requiredExports) {
                            if (expName == req) {
                                symInfo.isRequiredInterface = true;
                                break;
                            }
                        }

                        exportedNames.push_back(expName);
                        report.exportedSymbols.push_back(symInfo);
                    }
                }
            }
        }
    }

    report.logMessages.push_back("INFO: 导出函数符号解析完成，共包含 " + std::to_string(report.exportedSymbols.size()) + " 个导出符号。");

    // PE Pass condition depends on architecture match and zero missing required DLL dependencies
    report.overallPass = (report.isArchMatch && report.missingDependencyCount == 0);
    return report;
}

bool PeAnalyzer::ResolveDllLocation(const std::string& dllName,
                                   const std::string& targetDllDir,
                                   const std::vector<std::string>& extraSearchPaths,
                                   std::string& outPath) {
    // 1. Check target DLL directory
    std::string path1 = targetDllDir + "\\" + dllName;
    if (GetFileAttributesA(path1.c_str()) != INVALID_FILE_ATTRIBUTES) {
        outPath = path1;
        return true;
    }

    // 2. Check Extra search paths
    for (const auto& dir : extraSearchPaths) {
        std::string pathExtra = dir + "\\" + dllName;
        if (GetFileAttributesA(pathExtra.c_str()) != INVALID_FILE_ATTRIBUTES) {
            outPath = pathExtra;
            return true;
        }
    }

    // 3. Check System32 / SysWOW64
    char sysDir[MAX_PATH];
    if (GetSystemDirectoryA(sysDir, MAX_PATH) > 0) {
        std::string pathSys = std::string(sysDir) + "\\" + dllName;
        if (GetFileAttributesA(pathSys.c_str()) != INVALID_FILE_ATTRIBUTES) {
            outPath = pathSys;
            return true;
        }
    }

    // 4. Use SearchPathA API
    char searchBuf[MAX_PATH];
    LPSTR pFilePart;
    if (SearchPathA(NULL, dllName.c_str(), ".dll", MAX_PATH, searchBuf, &pFilePart) > 0) {
        outPath = searchBuf;
        return true;
    }

    outPath = "MISSING";
    return false;
}
