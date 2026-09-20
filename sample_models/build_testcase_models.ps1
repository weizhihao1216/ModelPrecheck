# Generate and build ModelPrecheck sample model testcase packages.
$ErrorActionPreference = "Stop"
$SrcRoot = $PSScriptRoot
$DistRoot = "D:\Projects\ModelPrecheck\dist\sample_models"
$OutRoot = Join-Path $SrcRoot "_build"

function Get-VcVars {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio with MSVC not found" }
    return Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
}

function Write-Utf8NoBom([string]$Path, [string]$Content) {
    $dir = Split-Path $Path -Parent
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function New-WeaponObjectH([string]$Guard) {
    return @"
#ifndef $Guard
#define $Guard

#include "WeaponModel.h"

// Inline helper for UserMain / Mo* harness (link this model's .lib).
class WeaponObject {
public:
    WeaponObject() : m_handle(nullptr) {}
    ~WeaponObject() { Shutdown(); }

    int Initialize(int objectId, double lat, double lon, double alt,
                   double speed, double dt) {
        Shutdown();
        m_handle = Model_Create();
        if (!m_handle) return -1;
        WeaponModelParams parameters{};
        parameters.init_lat = lat + objectId * 0.001;
        parameters.init_lon = lon + objectId * 0.001;
        parameters.init_alt = alt;
        parameters.init_speed = speed;
        parameters.init_heading = 40.0 + objectId * 6.0;
        parameters.init_pitch = 10.0;
        parameters.init_roll = 0.0;
        parameters.step_dt = dt;
        return Model_Init(m_handle, &parameters);
    }

    int Step(double& latitude, double& longitude) {
        if (!m_handle) return -1;
        WeaponModelOutput output{};
        const int result = Model_Step(m_handle, &output);
        latitude = output.lat;
        longitude = output.lon;
        return result;
    }

    void Shutdown() {
        if (m_handle) {
            Model_Destroy(m_handle);
            m_handle = nullptr;
        }
    }

private:
    void* m_handle;
};

#endif /* $Guard */
"@
}

function New-StandardHeader {
    param(
        [string]$Name,
        [string]$ExportMacro,
        [string]$Guard,
        [string]$Comment,
        [string]$ParamsBody,
        [string]$ExtraAfter
    )
    if (-not $ParamsBody) {
        $ParamsBody = @"
    double init_lat;
    double init_lon;
    double init_alt;
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
"@
    }
    return @"
#ifndef $Guard
#define $Guard

#ifdef ${ExportMacro}
#define ${Name}_API __declspec(dllexport)
#else
#define ${Name}_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)
typedef struct WeaponModelParams {
$ParamsBody
} WeaponModelParams;

typedef struct WeaponModelOutput {
    double sim_time;
    double lat;
    double lon;
    double alt;
    double vx;
    double vy;
    double vz;
    double pitch;
    double roll;
    double yaw;
    int status;
} WeaponModelOutput;
$ExtraAfter
#pragma pack(pop)

/*
 * $Name — $Comment
 */
${Name}_API void* Model_Create(void);
${Name}_API int Model_Init(void* handle, const WeaponModelParams* params);
${Name}_API int Model_Step(void* handle, WeaponModelOutput* output);
${Name}_API void Model_Destroy(void* handle);
${Name}_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* $Guard */
"@
}

$CppPass = @'
#define {EXPORTS}
#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
struct State {
    double lat, lon, alt;
    double speed, heading_deg, pitch_deg, roll_deg;
    double dt, sim_time;
    bool inited;
};

std::mutex g_mu;
std::vector<State*> g_live;

void Integrate(State* s, WeaponModelOutput* o) {
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double meters_per_deg_lat = 111320.0;
    const double meters_per_deg_lon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / meters_per_deg_lat;
    s->lon += (ve * s->dt) / (meters_per_deg_lon == 0.0 ? 1.0 : meters_per_deg_lon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    if (o) {
        std::memset(o, 0, sizeof(*o));
        o->sim_time = s->sim_time;
        o->lat = s->lat;
        o->lon = s->lon;
        o->alt = s->alt;
        o->vx = vn; o->vy = ve; o->vz = vd;
        o->pitch = s->pitch_deg; o->roll = s->roll_deg; o->yaw = s->heading_deg;
        o->status = 0;
    }
}
} // namespace

extern "C" {

void* Model_Create(void) {
    try {
        State* s = new State();
        std::memset(s, 0, sizeof(*s));
        std::lock_guard<std::mutex> lk(g_mu);
        g_live.push_back(s);
        return s;
    } catch (...) { return nullptr; }
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    State* s = static_cast<State*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->inited = true;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    State* s = static_cast<State*>(handle);
    if (!s->inited) return -2;
    Integrate(s, output);
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    State* s = static_cast<State*>(handle);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        for (size_t i = 0; i < g_live.size(); ++i) {
            if (g_live[i] == s) { g_live.erase(g_live.begin() + static_cast<std::ptrdiff_t>(i)); break; }
        }
    }
    delete s;
}

const char* Model_GetInfo(void) {
    return "{NAME} | thread-safe multi-instance | matching CRT | no shared kernel";
}

} // extern "C"
'@

$CppThreadUnsafe = @'
#define {EXPORTS}
#include "WeaponModel.h"
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// BUG: all instances share one unprotected global state (not thread-safe).
static struct {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    int alive;
    bool inited;
} g_world = {};

extern "C" {

void* Model_Create(void) {
    g_world.alive += 1;
    return &g_world;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    g_world.lat = params->init_lat;
    g_world.lon = params->init_lon;
    g_world.alt = params->init_alt;
    g_world.speed = params->init_speed;
    g_world.heading_deg = params->init_heading;
    g_world.pitch_deg = params->init_pitch;
    g_world.roll_deg = params->init_roll;
    g_world.dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    g_world.sim_time = 0.0;
    g_world.inited = true;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output || !g_world.inited) return -1;
    const double h = g_world.heading_deg * M_PI / 180.0;
    const double p = g_world.pitch_deg * M_PI / 180.0;
    const double vn = g_world.speed * std::cos(p) * std::cos(h);
    const double ve = g_world.speed * std::cos(p) * std::sin(h);
    const double vd = g_world.speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(g_world.lat * M_PI / 180.0);
    g_world.lat += (vn * g_world.dt) / mlat;
    g_world.lon += (ve * g_world.dt) / (mlon == 0.0 ? 1.0 : mlon);
    g_world.alt += vd * g_world.dt;
    g_world.sim_time += g_world.dt;
    std::memset(output, 0, sizeof(*output));
    output->sim_time = g_world.sim_time;
    output->lat = g_world.lat;
    output->lon = g_world.lon;
    output->alt = g_world.alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = g_world.pitch_deg; output->roll = g_world.roll_deg; output->yaw = g_world.heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    (void)handle;
    if (g_world.alive > 0) g_world.alive -= 1;
    if (g_world.alive <= 0) {
        g_world.alive = 0;
        g_world.inited = false;
    }
}

const char* Model_GetInfo(void) {
    return "{NAME} | INTENTIONAL BUG: shared global state, not thread-safe";
}

}
'@

$CppSingleInstance = @'
#define {EXPORTS}
#include "WeaponModel.h"
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct State {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    bool used;
    bool inited;
};

static State g_slots[1];
static int g_create_count = 0;

extern "C" {

void* Model_Create(void) {
    const int idx = g_create_count++;
    State* s = &g_slots[idx];
    std::memset(s, 0, sizeof(*s));
    s->used = true;
    return s;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    State* s = static_cast<State*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->inited = true;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    State* s = static_cast<State*>(handle);
    if (!s->inited) return -2;
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / mlat;
    s->lon += (ve * s->dt) / (mlon == 0.0 ? 1.0 : mlon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    std::memset(output, 0, sizeof(*output));
    output->sim_time = s->sim_time;
    output->lat = s->lat; output->lon = s->lon; output->alt = s->alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = s->pitch_deg; output->roll = s->roll_deg; output->yaw = s->heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    State* s = static_cast<State*>(handle);
    s->used = false;
    s->inited = false;
    if (g_create_count > 0) g_create_count -= 1;
}

const char* Model_GetInfo(void) {
    return "{NAME} | INTENTIONAL BUG: single-instance only / OOB on multi-create";
}

}
'@

$CppSharedKernel = @'
#define {EXPORTS}
#include "WeaponModel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma pack(push, 8)
struct SharedHub {
    double poison[32];
    struct Slot {
        int used;
        double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    } slots[2];
};
#pragma pack(pop)

static SharedHub* g_hub = nullptr;
static HANDLE g_map = nullptr;
static int g_local_seq = 0;

static SharedHub* EnsureHub() {
    if (g_hub) return g_hub;
    const wchar_t* kName = L"Local\\ModelPrecheck_SharedKernel_v1";
    g_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kName);
    if (!g_map) {
        g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   0, sizeof(SharedHub), kName);
    }
    if (!g_map) return nullptr;
    g_hub = static_cast<SharedHub*>(MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHub)));
    return g_hub;
}

extern "C" {

void* Model_Create(void) {
    SharedHub* hub = EnsureHub();
    if (!hub) return nullptr;
    const int idx = g_local_seq++;
    SharedHub::Slot* slot = &hub->slots[idx];
    std::memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    return slot;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->used = 1;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / mlat;
    s->lon += (ve * s->dt) / (mlon == 0.0 ? 1.0 : mlon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    if (g_hub) {
        for (int i = 0; i < 32; ++i) g_hub->poison[i] = s->lat + i;
    }
    std::memset(output, 0, sizeof(*output));
    output->sim_time = s->sim_time;
    output->lat = s->lat; output->lon = s->lon; output->alt = s->alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = s->pitch_deg; output->roll = s->roll_deg; output->yaw = s->heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
    s->used = 0;
    if (g_local_seq > 0) g_local_seq -= 1;
}

const char* Model_GetInfo(void) {
    return "{NAME} | INTENTIONAL BUG: shared named section + OOB slot write";
}

}
'@

$CppCrtHeap = @'
#define {EXPORTS}
#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct State {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    bool inited;
    char* path_buf;
};

extern "C" {

void* Model_Create(void) {
    State* s = new (std::nothrow) State();
    if (!s) return nullptr;
    std::memset(s, 0, sizeof(*s));
    s->path_buf = static_cast<char*>(std::malloc(256));
    if (s->path_buf) {
        std::memset(s->path_buf, 0, 256);
        std::memcpy(s->path_buf, "{NAME}_heap", 12);
    }
    return s;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    State* s = static_cast<State*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->inited = true;
    if (s->path_buf) {
        std::memcpy(s->path_buf, "init_ok", 8);
    }
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    State* s = static_cast<State*>(handle);
    if (!s->inited) return -2;
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / mlat;
    s->lon += (ve * s->dt) / (mlon == 0.0 ? 1.0 : mlon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    std::memset(output, 0, sizeof(*output));
    output->sim_time = s->sim_time;
    output->lat = s->lat; output->lon = s->lon; output->alt = s->alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = s->pitch_deg; output->roll = s->roll_deg; output->yaw = s->heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    State* s = static_cast<State*>(handle);
    if (s->path_buf) {
        std::free(s->path_buf);
        s->path_buf = nullptr;
    }
    delete s;
}

const char* Model_GetInfo(void) {
    return "{NAME} | CRT demo ({CRT}) | heap alloc inside DLL";
}

}
'@

$models = @(
    @{ Name="ReleaseOnly_A"; Kind="pass"; Comment="Release-only package A (no Debug lib/dll shipped)"; ShipDebug=$false; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="ReleaseOnly_B"; Kind="pass"; Comment="Release-only package B (no Debug lib/dll shipped)"; ShipDebug=$false; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="HeaderClash_A"; Kind="pass"; Comment="Header clash pair A (standard layout)"; ShipDebug=$true; Crt="MD"; ParamsBody=$null; ExtraAfter=@"

typedef struct VendorConfig {
    double scale;
    int mode;
} VendorConfig;
"@ },
    @{ Name="HeaderClash_B"; Kind="pass"; Comment="Header clash pair B (REORDERED params + different VendorConfig)"; ShipDebug=$true; Crt="MD";
      ParamsBody=@"
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
    double init_lat;
    double init_lon;
    double init_alt;
"@
      ExtraAfter=@"

typedef struct VendorConfig {
    int mode;
    char tag[32];
    double scale;
} VendorConfig;
"@ },
    @{ Name="ThreadUnsafe_A"; Kind="thread"; Comment="Not thread-safe A"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="ThreadUnsafe_B"; Kind="thread"; Comment="Not thread-safe B"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="SingleInstance_A"; Kind="single"; Comment="No multi-instance A"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="SingleInstance_B"; Kind="single"; Comment="No multi-instance B"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="SharedKernel_A"; Kind="shared"; Comment="Shared global/singleton env A"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="SharedKernel_B"; Kind="shared"; Comment="Shared global/singleton env B"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="CrtMd_A"; Kind="crt"; Comment="Dynamic CRT /MD"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="CrtMt_B"; Kind="crt"; Comment="Static CRT /MT (mismatched with CrtMd_A)"; ShipDebug=$true; Crt="MT"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="TcPass_A"; Kind="pass"; Comment="all-pass A (clean reference)"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null },
    @{ Name="TcPass_B"; Kind="pass"; Comment="all-pass B (clean reference)"; ShipDebug=$true; Crt="MD"; ExtraAfter=""; ParamsBody=$null }
)

Write-Host "Generating sources..."
foreach ($m in $models) {
    $dir = Join-Path $SrcRoot $m.Name
    $guard = ($m.Name.ToUpper() -replace '[^A-Z0-9]', '_') + "_MODEL_H"
    $oguard = ($m.Name.ToUpper() -replace '[^A-Z0-9]', '_') + "_OBJECT_H"
    $exports = $m.Name + "_EXPORTS"

    $h = New-StandardHeader -Name $m.Name -ExportMacro $exports -Guard $guard -Comment $m.Comment -ParamsBody $m.ParamsBody -ExtraAfter $m.ExtraAfter
    Write-Utf8NoBom (Join-Path $dir "WeaponModel.h") $h
    Write-Utf8NoBom (Join-Path $dir "WeaponObject.h") (New-WeaponObjectH -Guard $oguard)

    switch ($m.Kind) {
        "pass"   { $cpp = $CppPass }
        "thread" { $cpp = $CppThreadUnsafe }
        "single" { $cpp = $CppSingleInstance }
        "shared" { $cpp = $CppSharedKernel }
        "crt"    { $cpp = $CppCrtHeap }
        default  { $cpp = $CppPass }
    }
    $cpp = $cpp.Replace("{EXPORTS}", $exports).Replace("{NAME}", $m.Name).Replace("{CRT}", $m.Crt)
    Write-Utf8NoBom (Join-Path $dir "model.cpp") $cpp

    $readme = @"
# $($m.Name)

$($m.Comment)

## API
- Model_Create / Model_Init / Model_Step / Model_Destroy
- WeaponObject helper for Mo* harness

## Package layout
$($m.Name)/
  include/WeaponModel.h
  include/WeaponObject.h
  lib/$($m.Name).lib
  models/$($m.Name).dll
"@
    Write-Utf8NoBom (Join-Path $dir "README.md") $readme
}

$vcvars = Get-VcVars
New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
New-Item -ItemType Directory -Force -Path $DistRoot | Out-Null

function Invoke-Cl([string]$CmdLine) {
    $bat = Join-Path $OutRoot "_run_cl.bat"
    @"
@echo off
call "$vcvars" >nul
$CmdLine
exit /b %ERRORLEVEL%
"@ | Set-Content -Path $bat -Encoding ASCII
    & cmd /c $bat
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $CmdLine" }
}

Write-Host "Compiling DLLs..."
foreach ($m in $models) {
    $dir = Join-Path $SrcRoot $m.Name
    $objDir = Join-Path $OutRoot $m.Name
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null

    $configs = @(@{ Name="Release"; Runtime=("/" + $m.Crt) })
    if ($m.ShipDebug) {
        $dbgRt = if ($m.Crt -eq "MT") { "/MTd" } else { "/MDd" }
        $configs += @{ Name="Debug"; Runtime=$dbgRt }
    }

    foreach ($cfg in $configs) {
        $suffix = if ($cfg.Name -eq "Debug") { "d" } else { "" }
        $dllName = "$($m.Name)$suffix.dll"
        $libName = "$($m.Name)$suffix.lib"
        $obj = Join-Path $objDir "model_$($cfg.Name).obj"
        $dll = Join-Path $objDir $dllName
        $lib = Join-Path $objDir $libName
        $exports = "$($m.Name)_EXPORTS"
        $cpp = Join-Path $dir "model.cpp"

        $cmd = "cl /nologo /EHsc /std:c++17 /O2 $($cfg.Runtime) /D$exports /DWIN32 /D_WINDOWS /I`"$dir`" /c `"$cpp`" /Fo`"$obj`" && link /nologo /DLL /OUT:`"$dll`" /IMPLIB:`"$lib`" `"$obj`" kernel32.lib user32.lib"
        Write-Host "  [$($cfg.Name)] $($m.Name)"
        Invoke-Cl $cmd
    }

    $pkg = Join-Path $DistRoot $m.Name
    foreach ($sub in @("include","lib","models")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $pkg $sub) | Out-Null
    }
    Copy-Item (Join-Path $dir "WeaponModel.h") (Join-Path $pkg "include\WeaponModel.h") -Force
    Copy-Item (Join-Path $dir "WeaponObject.h") (Join-Path $pkg "include\WeaponObject.h") -Force
    Copy-Item (Join-Path $dir "README.md") (Join-Path $pkg "README.md") -Force
    Copy-Item (Join-Path $objDir "$($m.Name).dll") (Join-Path $pkg "models\$($m.Name).dll") -Force
    Copy-Item (Join-Path $objDir "$($m.Name).lib") (Join-Path $pkg "lib\$($m.Name).lib") -Force
    if ($m.ShipDebug) {
        Copy-Item (Join-Path $objDir "$($m.Name)d.dll") (Join-Path $pkg "models\$($m.Name)d.dll") -Force
        Copy-Item (Join-Path $objDir "$($m.Name)d.lib") (Join-Path $pkg "lib\$($m.Name)d.lib") -Force
    }
}

$index = @"
# sample_models 测试用例包

生成路径：dist/sample_models/<型号>/{include,lib,models}

| 问题 | 型号对 | 如何暴露 |
|------|--------|----------|
| Debug 编译失败、Release 成功 | ReleaseOnly_A / ReleaseOnly_B | 包内仅有 Release .lib/.dll，无 Debug |
| 头文件结构体重名、无命名空间 | HeaderClash_A / HeaderClash_B | 同名 WeaponModelParams/VendorConfig，B 字段顺序不同 |
| 未做线程安全 | ThreadUnsafe_A / ThreadUnsafe_B | 全局单例状态，多线程 Step 竞态 |
| 不支持多实例 / 越界 | SingleInstance_A / SingleInstance_B | 仅 1 个槽位，第 2 次 Create 越界 |
| 共享全局/单例环境或写坏公共数据 | SharedKernel_A / SharedKernel_B | 同名 FileMapping + 越界写 |
| CRT 不匹配 / 堆隔离 | CrtMd_A (/MD) / CrtMt_B (/MT) | PE 可见不同 CRT；堆分配在 DLL 内 |
| 无上述问题（对照） | TcPass_A / TcPass_B | 多实例 + 互斥 + /MD + 完整 Debug/Release |

## 统一 API

void* Model_Create(void);
int   Model_Init(void* handle, const WeaponModelParams* params);
int   Model_Step(void* handle, WeaponModelOutput* output); // 返回 lat/lon 轨迹点
void  Model_Destroy(void* handle);

WeaponObject 供 MoCreate/MoInit/MoStep/MoDestroy 集成。
"@
Write-Utf8NoBom (Join-Path $DistRoot "README.md") $index

Write-Host "DONE. Packages installed to $DistRoot"
Get-ChildItem $DistRoot -Directory | ForEach-Object {
    $dll = @(Get-ChildItem (Join-Path $_.FullName "models") -Filter *.dll -ErrorAction SilentlyContinue).Count
    $lib = @(Get-ChildItem (Join-Path $_.FullName "lib") -Filter *.lib -ErrorAction SilentlyContinue).Count
    "{0}: {1} dll, {2} lib" -f $_.Name, $dll, $lib
}
