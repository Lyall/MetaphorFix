#include "stdafx.h"
#include "helper.hpp"

#include <inipp/inipp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <safetyhook.hpp>

HMODULE baseModule = GetModuleHandle(NULL);
HMODULE thisModule; // Fix DLL

// Version
std::string sFixName = "MetaphorFix";
std::string sFixVer = "0.7.7";
std::string sLogFile = sFixName + ".log";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::filesystem::path sExePath;
std::string sExeName;
std::filesystem::path sThisModulePath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::pair DesktopDimensions = { 0,0 };

// Ini variables
bool bFixResolution;
bool bFixAspect;
bool bFixFOV;
bool bFixMovies;
bool bIntroSkip;
bool bSkipMovie;
bool bFixFPSCap;
bool bDisableDashBlur;
float fAOResolutionScale = 1.00f;
float fGameplayFOVMulti;
float fLODDistance = 10.00f;
bool bFixAnalog;
float fCustomResScale = 1.00f;
bool bDisableOutlines;

// Aspect ratio + HUD stuff
float fPi = (float)3.141592653;
float fAspectRatio;
float fNativeAspect = (float)16 / 9;
float fAspectMultiplier;
float fHUDWidth;
float fHUDHeight;
float fHUDWidthOffset;
float fHUDHeightOffset;

// Variables
int iPreResScaleX;
int iPreResScaleY;
int iCurrentResX;
int iCurrentResY;
int iResScaleOption = 4;
uintptr_t LODDistanceAddr;

void CalculateAspectRatio(bool bLog)
{
    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD variables
    fHUDWidth = iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2;
    fHUDHeightOffset = 0;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2;
    }

    if (bLog) {
        // Log details about current resolution
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {}x{}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }   
}

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size) {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open()) {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (std::filesystem::exists(_filename) && std::filesystem::file_size(_filename) >= _max_size) {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file() {
        if (std::filesystem::exists(_filename)) {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

void Logging()
{
    // Get this module path
    WCHAR thisModulePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(thisModule, thisModulePath, MAX_PATH);
    sThisModulePath = thisModulePath;
    sThisModulePath = sThisModulePath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(baseModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // spdlog initialisation
    {
        try {
            // Create 10MB truncated logger
            logger = logger = std::make_shared<spdlog::logger>(sLogFile, std::make_shared<size_limited_sink<std::mutex>>(sThisModulePath.string() + sLogFile, 10 * 1024 * 1024));
            spdlog::set_default_logger(logger);

            spdlog::flush_on(spdlog::level::debug);
            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", sFixName.c_str(), sFixVer.c_str());
            spdlog::info("----------");
            spdlog::info("Log file: {}", sThisModulePath.string() + sLogFile);
            spdlog::info("----------");

            // Log module details
            spdlog::info("Module Name: {0:s}", sExeName.c_str());
            spdlog::info("Module Path: {0:s}", sExePath.string());
            spdlog::info("Module Address: 0x{0:x}", (uintptr_t)baseModule);
            spdlog::info("Module Timestamp: {0:d}", Memory::ModuleTimestamp(baseModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex) {
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialisation failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(baseModule, 1);
        }
    }
}

void Configuration()
{
    // Initialise config
    std::ifstream iniFile(sThisModulePath.string() + sConfigFile);
    if (!iniFile) {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVer.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sThisModulePath.string().c_str() << std::endl;
        FreeLibraryAndExitThread(baseModule, 1);
    }
    else {
        spdlog::info("Config file: {}", sThisModulePath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    inipp::get_value(ini.sections["Fix Resolution"], "Enabled", bFixResolution);
    spdlog::info("Config Parse: bFixResolution: {}", bFixResolution);

    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    spdlog::info("Config Parse: bFixAspect: {}", bFixAspect);

    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    spdlog::info("Config Parse: bFixFOV: {}", bFixFOV);

    inipp::get_value(ini.sections["Fix Movies"], "Enabled", bFixMovies);
    spdlog::info("Config Parse: bFixMovies: {}", bFixMovies);

    inipp::get_value(ini.sections["Intro Skip"], "Enabled", bIntroSkip);
    inipp::get_value(ini.sections["Intro Skip"], "SkipMovie", bSkipMovie);
    spdlog::info("Config Parse: bIntroSkip: {}", bIntroSkip);
    spdlog::info("Config Parse: bSkipMovie: {}", bSkipMovie);

    inipp::get_value(ini.sections["Fix Framerate Cap"], "Enabled", bFixFPSCap);
    spdlog::info("Config Parse: bFixFPSCap: {}", bFixFPSCap);

    inipp::get_value(ini.sections["Fix Analog Movement"], "Enabled", bFixAnalog);
    spdlog::info("Config Parse: bFixAnalog: {}", bFixAnalog);

    inipp::get_value(ini.sections["Disable Dash Blur"], "Enabled", bDisableDashBlur);
    spdlog::info("Config Parse: bDisableDashBlur: {}", bDisableDashBlur);

    inipp::get_value(ini.sections["Ambient Occlusion"], "Resolution", fAOResolutionScale);
    if (fAOResolutionScale < 0.10f || fAOResolutionScale > 1.00f) {
        fAOResolutionScale = std::clamp(fAOResolutionScale, 0.10f, 1.00f);
        spdlog::warn("Config Parse: fAOResolutionScale value invalid, clamped to {}", fAOResolutionScale);
    }
    spdlog::info("Config Parse: fAOResolutionScale: {}", fAOResolutionScale);

    inipp::get_value(ini.sections["LOD"], "Distance", fLODDistance);
    if (fLODDistance < 1.00f || fLODDistance > 100.00f) {
        fLODDistance = std::clamp(fLODDistance, 1.00f, 100.00f);
        spdlog::warn("Config Parse: fLODDistance value invalid, clamped to {}", fLODDistance);
    }
    spdlog::info("Config Parse: fLODDistance: {}", fLODDistance);

    inipp::get_value(ini.sections["Custom Resolution Scale"], "Resolution", fCustomResScale);
    if (fCustomResScale < 0.10f || fCustomResScale > 4.00f) {
        fCustomResScale = std::clamp(fCustomResScale, 0.10f, 4.00f);
        spdlog::warn("Config Parse: fCustomResScale value invalid, clamped to {}", fCustomResScale);
    }
    spdlog::info("Config Parse: fCustomResScale: {}", fCustomResScale);

    inipp::get_value(ini.sections["Disable Outlines"], "Enabled", bDisableOutlines);
    spdlog::info("Config Parse: bDisableOutlines: {}", bDisableOutlines);

    spdlog::info("----------");

    // Grab desktop resolution/aspect
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    iCurrentResX = DesktopDimensions.first;
    iCurrentResY = DesktopDimensions.second;
    CalculateAspectRatio(true);
}

WNDPROC OldWndProc;
LRESULT __stdcall NewWndProc(HWND window, UINT message_type, WPARAM w_param, LPARAM l_param) {
    switch (message_type) {
    case WM_SYSCOMMAND:
        switch (w_param) {
            case SC_SCREENSAVE: // Disable screensaver/monitor sleep
            case SC_MONITORPOWER: {
                if (l_param != -1)
                    return TRUE;
            }
        }
        break;

    case WM_CLOSE:
        return DefWindowProc(window, message_type, w_param, l_param); // Return default WndProc
    }

    // Return old WndProc
    return CallWindowProc(OldWndProc, window, message_type, w_param, l_param);
};

SafetyHookInline SetWindowLongPtrW_sh{};
LONG_PTR WINAPI SetWindowLongPtrW_hk(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    WCHAR className[256];
    const LPCWSTR targetClass = L"METAPHOR_WINDOW";

    // Only match game class name
    if (GetClassNameW(hWnd, className, sizeof(className) / sizeof(WCHAR))) {
        if (wcscmp(className, targetClass) == 0) {
            // Set new wnd proc
            if (OldWndProc == nullptr) {
                OldWndProc = (WNDPROC)SetWindowLongPtrW_sh.stdcall<LONG_PTR>(hWnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
                spdlog::info("Game Window: Set new WndProc successfully.");
            }
        }
    }

    // Call the original function
    return SetWindowLongPtrW_sh.stdcall<LONG_PTR>(hWnd, nIndex, dwNewLong);
}

void WindowManagement()
{
    // Hook SetWindowLongPtrW
    HMODULE user32Module = GetModuleHandleW(L"user32.dll");
    if (user32Module) {
        FARPROC SetWindowLongPtrW_fn = GetProcAddress(user32Module, "SetWindowLongPtrW");
        if (SetWindowLongPtrW_fn) {
            SetWindowLongPtrW_sh = safetyhook::create_inline(SetWindowLongPtrW_fn, reinterpret_cast<void*>(SetWindowLongPtrW_hk));
            spdlog::info("Game Window: Hooked SetWindowLongPtrW.");
        }
        else {
            spdlog::error("Game Window: Failed to get function address for SetWindowLongPtrW.");
        }
    }
    else {
        spdlog::error("Game Window: Failed to get module handle for user32.dll.");
    }
}

void IntroSkip()
{
    if (bIntroSkip) {
        // Intro Skip
        uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "83 ?? 49 0F 87 ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 48 ?? ??");
        if (IntroSkipScanResult) {
            spdlog::info("Intro Skip: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult - (uintptr_t)baseModule);
            static SafetyHookMid IntroSkipMidHook{};
            IntroSkipMidHook = safetyhook::create_mid(IntroSkipScanResult,
                [](SafetyHookContext& ctx) {
                    int iTitleState = (int)ctx.rax;

                    // Title States
                    // 0x11 - 0x1E = OOBE
                    // 0x1F = Autosave dialog
                    // 0x21 = Unauthorized reproduction warning
                    // 0x2A = Atlus logo
                    // 0x2B = Studio Zero logo
                    // 0x31 = Middleware logo
                    // 0x36 = Opening movie
                    // 0x3A = Demo message
                    // 0x3F = Main menu

                    // Check if at autosave dialog
                    if (iTitleState == 0x1F) {
                        // Skip to pre-Atlus logo
                        ctx.rax = 0x27;
                    }

                    // Check if at Atlus logo
                    if (iTitleState == 0x2A) {            
                        // Skip to main menu
                        ctx.rax = 0x3F;

                        if (!bSkipMovie) {
                            // Skip to opening movie instead
                            ctx.rax = 0x36;
                        }
                    }    
                });
        }
        else if (!IntroSkipScanResult) {
            spdlog::error("Intro Skip: Pattern scan failed.");
        }
    }
}

void Resolution()
{
    // Get current resolution and fix scaling to 16:9
    uint8_t* CurrentResolutionScanResult = nullptr;
    for (int attempts = 0; attempts < 1000; ++attempts) {
        if (CurrentResolutionScanResult = Memory::PatternScan(baseModule, "4C ?? ?? ?? ?? ?? ?? ?? 8B ?? 48 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 8D ?? ?? C1 ?? 04"))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    uint8_t* ResolutionFixScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? 89 ?? ?? ?? ?? ?? C5 ?? ?? ?? 89 ?? ?? ?? ?? ?? 85 ?? 7E ??");
    if (CurrentResolutionScanResult && ResolutionFixScanResult) {
        spdlog::info("Resolution: Current: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentResolutionScanResult - (uintptr_t)baseModule);
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                // Store resolution, before scaling to 16:9 happens.
                iPreResScaleX = (int)ctx.rax;
                iPreResScaleY = (int)ctx.rdx;
            });

        spdlog::info("Resolution: Fix: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionFixScanResult - (uintptr_t)baseModule);
        static SafetyHookMid ResolutionFixMidHook{};
        ResolutionFixMidHook = safetyhook::create_mid(ResolutionFixScanResult,
            [](SafetyHookContext& ctx) {
                // Undo scaling to 16:9
                if (bFixResolution) {
                    ctx.xmm7.f32[0] = (float)iPreResScaleX;
                    ctx.xmm6.f32[0] = (float)iPreResScaleY;
                }
          
                // Log current resolution
                int iResX = (int)ctx.xmm7.f32[0];
                int iResY = (int)ctx.xmm6.f32[0];

                if (iResX != iCurrentResX || iResY != iCurrentResY) {
                    iCurrentResX = iResX;
                    iCurrentResY = iResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else if (!CurrentResolutionScanResult || !ResolutionFixScanResult) {
        spdlog::error("Resolution: Pattern scan(s) failed.");
    }   
}

void AspectRatioFOV()
{
    if (bFixAspect) {
        // Aspect ratio
        uint8_t* AspectRatioScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? C5 ?? ?? ?? ?? C4 ?? ?? ?? ?? C5 ?? ?? ?? 33 ?? C7 ?? ?? ?? ?? ?? 00 00 80 BF");
        if (AspectRatioScanResult) {
            spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)AspectRatioScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(AspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rbx + 0x190) {
                        *reinterpret_cast<float*>(ctx.rbx + 0x190) = fAspectRatio;
                        ctx.xmm2.f32[0] = fAspectRatio;
                    }
                });
        }
        else if (!AspectRatioScanResult) {
            spdlog::error("Aspect Ratio: Pattern scan failed.");
        }
    }

    if (bFixFOV) {
        // Global FOV
        uint8_t* GlobalFOVScanResult = Memory::PatternScan(baseModule, "E9 ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? E8 ?? ?? ?? ?? C5 ?? ?? ??");
        if (GlobalFOVScanResult) {
            spdlog::info("FOV: Global: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GlobalFOVScanResult - (uintptr_t)baseModule);
            static SafetyHookMid GlobalFOVMidHook{};
            GlobalFOVMidHook = safetyhook::create_mid(GlobalFOVScanResult + 0xD,
                [](SafetyHookContext& ctx) {
                    // Fix cropped field of view
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = atan(tan(ctx.xmm0.f32[0] * fPi / 360.0f) / fAspectRatio * fNativeAspect) * 360.0f / fPi;
                });
        }
        else if (!GlobalFOVScanResult) {
            spdlog::error("FOV: Global: Pattern scan failed.");
        }
    }
}

void HUD() 
{
    if (bFixMovies) {
        // Movies
        // TPL::movie::MovieSofdecWIN64
        uint8_t* MoviesScanResult = Memory::PatternScan(baseModule, "8B ?? ?? 48 ?? ?? ?? 48 ?? ?? ?? ?? 4C ?? ?? ?? ?? 4C ?? ?? ?? ?? F3 0F ?? ?? ?? ?? E8 ?? ?? ?? ??");
        if (MoviesScanResult) {
            spdlog::info("HUD: Movies: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MoviesScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MoviesMidHook{};
            MoviesMidHook = safetyhook::create_mid(MoviesScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x30) {
                        if (fAspectRatio > fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x30) = fHUDWidthOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x38) = fHUDWidth;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x34) = fHUDHeightOffset;
                            *reinterpret_cast<float*>(ctx.rsp + 0x3C) = fHUDHeight;
                        }
                    }
                });
        }
        else if (!MoviesScanResult) {
            spdlog::error("HUD: Movies: Pattern scan failed.");
        }
    }
}

void Misc() 
{
    if (bFixFPSCap) {
        // Fix framerate cap. Stops menus being locked to 60fps with vsync off and other odd behaviour.
        uint8_t* FramerateCapScanResult = Memory::PatternScan(baseModule, "89 ?? ?? ?? ?? ?? 8B ?? C7 ?? ?? ?? ?? ?? ?? ?? 85 ?? 75 ?? 48 ?? ?? ?? ?? ?? ?? 00");
        if (FramerateCapScanResult) {
            spdlog::info("Framerate Cap: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FramerateCapScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FramerateCapMidHook{};
            FramerateCapMidHook = safetyhook::create_mid(FramerateCapScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.rcx = 0;
                });
        }
        else if (!FramerateCapScanResult) {
            spdlog::error("Framerate Cap: Pattern scan failed.");
        }
    }

    if (bFixAnalog) {
        uint8_t* XInputGetStateScanResult = Memory::PatternScan(baseModule, "3D ?? ?? ?? ?? 8D ?? ?? ?? ?? ?? C5 ?? ?? ?? 41 ?? ?? ?? 3D ?? ?? ?? ?? C5 ?? ?? ?? 0F ?? ?? ?? ??");
        if (XInputGetStateScanResult) {
            spdlog::info("Analog Movement Fix: XInputGetState: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)XInputGetStateScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)XInputGetStateScanResult + 0x55, 0);
            Memory::Write((uintptr_t)XInputGetStateScanResult + 0x68, 0);
            spdlog::info("Analog Movement Fix: XInputGetState: Patched instructions.");
        }
        else if (!XInputGetStateScanResult) {
            spdlog::error("Analog Movement Fix: XInputGetState: Pattern scan failed.");
        }
    }
}

void Graphics()
{
    if (bDisableDashBlur) {
        // Disable dash blur + speed lines
        uint8_t* DashBlurScanResult = Memory::PatternScan(baseModule, "74 ?? 84 ?? 75 ?? 45 ?? ?? 48 ?? ?? 41 ?? ?? ?? E8 ?? ?? ?? ??");
        if (DashBlurScanResult) {
            spdlog::info("Dash Blur: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)DashBlurScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)DashBlurScanResult, "\xEB", 1);
            spdlog::info("Dash Blur: Patched instruction.");
        }
        else if (!DashBlurScanResult) {
            spdlog::error("Dash Blur: Pattern scan failed.");
        }
    }

    // Resolution Scale
    uint8_t* ResolutionScaleScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 44 ?? ?? ??");
    if (ResolutionScaleScanResult) {
        spdlog::info("Resolution Scale: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionScaleScanResult - (uintptr_t)baseModule);
        static SafetyHookMid ResolutionScaleMidHook{};
        ResolutionScaleMidHook = safetyhook::create_mid(ResolutionScaleScanResult + 0xE,
            [](SafetyHookContext& ctx) {
                // Set custom resolution scale
                if (fCustomResScale != 1.00f && ctx.rcx + 0x888) {
                    // Set res scale option to 4 (100%)
                    *reinterpret_cast<int*>(ctx.rcx + 0x888) = 4;
                    ctx.rax = 4;
                    // Write new resolution scale
                    Memory::Write(ctx.rdx + 0x10, 1.00f / fCustomResScale);

                    spdlog::info("Resolution Scale: Custom: Base Resolution: {}x{}.", iCurrentResX, iCurrentResY);
                    spdlog::info("Resolution Scale: Custom: Scaled Resolution: {}x{}.", static_cast<int>(iCurrentResX * fCustomResScale), static_cast<int>(iCurrentResY * fCustomResScale));
                }

                // Log res scale option for AO
                iResScaleOption = (int)ctx.rax;
            });
    }
    else if (!ResolutionScaleScanResult) { 
        spdlog::error("Resolution Scale: Pattern scan failed.");
    }

    if (fAOResolutionScale != 1.00f) {
        // Ambient Occlusion Resolution
        uint8_t* AOResolutionScanResult = Memory::PatternScan(baseModule, "8B ?? 48 ?? ?? ?? ?? ?? ?? 48 ?? ?? 74 ?? E8 ?? ?? ?? ?? 41 ?? 00 40 00 00 41 ?? 16 00 00 00");
        if (AOResolutionScanResult) {
            spdlog::info("Ambient Occlusion Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)AOResolutionScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AOResolutionMidHook{};
            AOResolutionMidHook = safetyhook::create_mid(AOResolutionScanResult,
                [](SafetyHookContext& ctx) {
                    float fResScale = 1.00f;
                    switch (iResScaleOption) {
                    case 0:
                        fResScale = 2.00f;  // 200%
                        break;
                    case 1:
                        fResScale = 1.75f;  // 175%
                        break;
                    case 2:
                        fResScale = 1.50f;  // 150%
                        break;
                    case 3:
                        fResScale = 1.25f;  // 125%
                        break;
                    case 4:
                        fResScale = 1.00f;  // 100%
                        break;
                    case 5:
                        fResScale = 0.75f;  // 75%
                        break;
                    case 6:
                        fResScale = 0.50f;  // 50%
                        break;
                    default:
                        fResScale = 1.00f;
                        break;
                    }

                    if (fCustomResScale != 1.00f)
                        fResScale = fCustomResScale;

                    // Calculate resolution with in-game resolution scale
                    int iScaledResX = static_cast<int>(iCurrentResX * fResScale);
                    int iScaledResY = static_cast<int>(iCurrentResY * fResScale);

                    // Calculate new ambient occlusion resolution
                    int iAmbientOcclusionResX = static_cast<int>(iScaledResX * fAOResolutionScale);
                    int iAmbientOcclusionResY = static_cast<int>(iScaledResY * fAOResolutionScale);

                    // Log old and new resolution
                    spdlog::info("Ambient Occlusion: Previous Resolution: {}x{}.", iScaledResX, iScaledResY);
                    spdlog::info("Ambient Occlusion: New Resolution: {}x{}.", iAmbientOcclusionResX, iAmbientOcclusionResY);

                    // Apply new resolution
                    ctx.rbx = iAmbientOcclusionResX;
                    ctx.rax = iAmbientOcclusionResY;
                });
        }
        else if (!AOResolutionScanResult) {
            spdlog::error("Ambient Occlusion Resolution: Pattern scan failed.");
        }
    }
  
    if (fLODDistance != 10.00f) {
        // LOD Distance
        uint8_t* LODDistanceScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? ?? ?? ? ?? C5 ?? ?? ?? ?? ?? 73 ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 72 ?? C5 ?? ?? ?? ?? 73 ??");
        if (LODDistanceScanResult) {
            spdlog::info("LOD Distance: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)LODDistanceScanResult - (uintptr_t)baseModule);
            LODDistanceAddr = Memory::GetAbsolute((uintptr_t)LODDistanceScanResult + 0x4);
            spdlog::info("LOD Distance: Value address is {:s}+{:x}", sExeName.c_str(), LODDistanceAddr - (uintptr_t)baseModule);

            // Big number scary
            float fRealLODDistance = fLODDistance * 1000.00f;
            // This value can be modified directly since it's only accessed by one function. 
            Memory::Write(LODDistanceAddr, fRealLODDistance);
        }
        else if (!LODDistanceScanResult) {
            spdlog::error("LOD Distance: Pattern scan failed.");
        }
    }

    if (bDisableOutlines) {
        // Outline Shader
        uint8_t* OutlineShaderScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? ?? ?? ?? 0F 00 00 00 C6 ?? ?? ?? ?? ?? 01 C6 ?? ?? ?? ?? ?? 01");
        if (OutlineShaderScanResult) {
            spdlog::info("Outline Shader: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)OutlineShaderScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)OutlineShaderScanResult + 0x10, "\x00", 1);
            spdlog::info("Outline Shader: Patched instruction.");
        }
        else if (!OutlineShaderScanResult) {
            spdlog::error("Outline Shader: Pattern scan failed.");
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    WindowManagement();
    Resolution();
    IntroSkip();
    AspectRatioFOV();
    HUD();
    Misc();
    Graphics();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        // Detach from "crash_handler.exe"
        char exeName[MAX_PATH];
        GetModuleFileNameA(NULL, exeName, MAX_PATH);
        std::string exeStr(exeName);
        if (exeStr.find("crash_handler.exe") != std::string::npos)
            return FALSE;

        thisModule = hModule;
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, CREATE_SUSPENDED, 0);
        if (mainHandle) {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_TIME_CRITICAL);
            ResumeThread(mainHandle);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}