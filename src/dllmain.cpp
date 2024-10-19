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
std::string sFixVer = "0.8.3";
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
bool bFixHUD;
bool bFixMovies;
bool bIntroSkip;
bool bSkipMovie;
bool bMenuFPSCap;
bool bDisableDashBlur;
float fAOResolutionScale = 1.00f;
float fGameplayFOVMulti = 1.00f;
float fLODDistance = 10.00f;
bool bFixAnalog;
float fCustomResScale = 1.00f;
bool bDisableOutlines;
int iShadowResolution = 2048;
bool bForceControllerIcons;
bool bDisableCameraShake;
bool bGameWindow;

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

    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

    inipp::get_value(ini.sections["Fix Movies"], "Enabled", bFixMovies);
    spdlog::info("Config Parse: bFixMovies: {}", bFixMovies);

    inipp::get_value(ini.sections["Intro Skip"], "Enabled", bIntroSkip);
    inipp::get_value(ini.sections["Intro Skip"], "SkipMovie", bSkipMovie);
    spdlog::info("Config Parse: bIntroSkip: {}", bIntroSkip);
    spdlog::info("Config Parse: bSkipMovie: {}", bSkipMovie);

    inipp::get_value(ini.sections["Disable Menu FPS Cap"], "Enabled", bMenuFPSCap);
    spdlog::info("Config Parse: bMenuFPSCap: {}", bMenuFPSCap);

    inipp::get_value(ini.sections["Fix Analog Movement"], "Enabled", bFixAnalog);
    spdlog::info("Config Parse: bFixAnalog: {}", bFixAnalog);

    inipp::get_value(ini.sections["Disable Dash Blur"], "Enabled", bDisableDashBlur);
    spdlog::info("Config Parse: bDisableDashBlur: {}", bDisableDashBlur);

    inipp::get_value(ini.sections["Gameplay FOV"], "Multiplier", fGameplayFOVMulti);
    if (fGameplayFOVMulti < 0.10f || fGameplayFOVMulti > 3.00f) {
        fGameplayFOVMulti = std::clamp(fGameplayFOVMulti, 0.10f, 3.00f);
        spdlog::warn("Config Parse: fGameplayFOVMulti value invalid, clamped to {}", fGameplayFOVMulti);
    }
    spdlog::info("Config Parse: fGameplayFOVMulti: {}", fGameplayFOVMulti);
    
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

    inipp::get_value(ini.sections["Shadow Quality"], "Resolution", iShadowResolution);
    iShadowResolution = ((iShadowResolution + 63) / 64) * 64;
    if (iShadowResolution < 64 || iShadowResolution > 16384) {
        iShadowResolution = std::clamp(iShadowResolution, 64, 16384);
        spdlog::warn("Config Parse: iShadowResolution value invalid, clamped to {}", iShadowResolution);
    }
    spdlog::info("Config Parse: iShadowResolution: {}", iShadowResolution);

    inipp::get_value(ini.sections["Force Controller Icons"], "Enabled", bForceControllerIcons);
    spdlog::info("Config Parse: bForceControllerIcons: {}", bForceControllerIcons);

    inipp::get_value(ini.sections["Disable Camera Shake"], "Enabled", bDisableCameraShake);
    spdlog::info("Config Parse: bDisableCameraShake: {}", bDisableCameraShake);

    inipp::get_value(ini.sections["Game Window"], "Enabled", bGameWindow);
    spdlog::info("Config Parse: bGameWindow: {}", bGameWindow);

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
    if (bGameWindow) {
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
}

void IntroSkip()
{
    if (bIntroSkip) {
        // Intro Skip
        uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "83 ?? ?? 0F 87 ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 48 ?? ?? FF ?? BA 01 00 00 00 48 ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ??");
        if (IntroSkipScanResult) {
            static uint8_t* DemoIntroSkipScanResult = Memory::PatternScan(baseModule, "83 ?? 49 0F 87 ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 48 ?? ??");

            spdlog::info("Intro Skip: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)IntroSkipScanResult - (uintptr_t)baseModule);
            static bool bHasSkippedIntro = false;
            static SafetyHookMid IntroSkipMidHook{};
            IntroSkipMidHook = safetyhook::create_mid(IntroSkipScanResult,
                [](SafetyHookContext& ctx) {
                    int iTitleState = (int)ctx.rax;

                    if (DemoIntroSkipScanResult)
                    {
                        // Title States (Demo)
                        // 0x11 - 0x1E = OOBE
                        // 0x1F = Autosave dialog
                        // 0x21 = Unauthorized reproduction warning
                        // 0x2A = Atlus logo
                        // 0x2B = Studio Zero logo
                        // 0x31 = Middleware logo
                        // 0x36 = Opening movie
                        // 0x3A = Demo message
                        // 0x3F = Main menu

                        if (!bHasSkippedIntro && ctx.rcx + 0x8) {
                            // Check if at autosave dialog
                            if (iTitleState == 0x1F) {
                                // Skip to pre-Atlus logo
                                *reinterpret_cast<int*>(ctx.rcx + 0x8) = 0x27;
                            }

                            // Check if at Atlus logo
                            if (iTitleState == 0x2A) {
                                // Skip to main menu
                                *reinterpret_cast<int*>(ctx.rcx + 0x8) = 0x3F;

                                // Skip to opening movie instead
                                if (!bSkipMovie) {
                                    *reinterpret_cast<int*>(ctx.rcx + 0x8) = 0x36;
                                }

                                bHasSkippedIntro = true;
                            }
                        }
                    }
                    else
                    {
                        // Title States (Full Game)
                        // 0x0 - 0x2F = OOBE
                        // 0x30 = Atlus Logo
                        // 0x31 = Studio Zero Logo
                        // 0x37 = Middleware
                        // 0x3C = Opening Movie
                        // 0x40 = Attract Movie
                        // 0x43 = Press Any Key

                        if (!bHasSkippedIntro && ctx.rcx + 0x8) {
                            // Check if at Atlus logo
                            if (iTitleState == 0x30) {
                                // Skip to main menu
                                *reinterpret_cast<int*>(ctx.rcx + 0x8) = 0x43;

                                // Skip to intro movie
                                if (!bSkipMovie)
                                    *reinterpret_cast<int*>(ctx.rcx + 0x8) = 0x3C;

                                bHasSkippedIntro = true;
                            }
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
        // Shadow Aspect Ratio
        uint8_t* ShadowAspectRatioScanResult = Memory::PatternScan(baseModule, "48 ?? ?? ?? C5 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? 4C ?? ?? ?? ?? ??");
        if (ShadowAspectRatioScanResult) {
            spdlog::info("Aspect Ratio: Shadows: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowAspectRatioScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ShadowAspectRatioMidHook{};
            ShadowAspectRatioMidHook = safetyhook::create_mid(ShadowAspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm1.f32[0] = fAspectRatio;
                });
        }
        else if (!ShadowAspectRatioScanResult) {
            spdlog::error("Aspect Ratio: Shadows: Pattern scan failed.");
        }

        // CameraPane Aspect Ratio
        uint8_t* CameraPaneAspectRatioScanResult = Memory::PatternScan(baseModule, "48 ?? ?? E8 ?? ?? ?? ?? C5 ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? C5 ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? 4C ?? ??");
        if (CameraPaneAspectRatioScanResult) {
            spdlog::info("Aspect Ratio: CameraPane: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CameraPaneAspectRatioScanResult - (uintptr_t)baseModule);
            static SafetyHookMid CameraPaneAspectRatioMidHook{};
            CameraPaneAspectRatioMidHook = safetyhook::create_mid(CameraPaneAspectRatioScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm1.f32[0] = fNativeAspect;
                });
        }
        else if (!CameraPaneAspectRatioScanResult) {
            spdlog::error("Aspect Ratio: CameraPane: Pattern scan failed.");
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
    
    if (fGameplayFOVMulti != 1.00f) {
        // Gameplay FOV
        uint8_t* GameplayFOVScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 48 ?? ?? C4 ?? ?? ?? ?? E8 ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? C4 ?? ?? ?? ?? C5 ?? ?? ??");
        if (GameplayFOVScanResult) {
            spdlog::info("FOV: Gameplay: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);
            uintptr_t GameplayFOVFunctionAddr = Memory::GetAbsolute((uintptr_t)GameplayFOVScanResult + 0xC);
            spdlog::info("FOV: Gameplay: Function address is {:s}+{:x}", sExeName.c_str(), GameplayFOVFunctionAddr - (uintptr_t)baseModule);
            static SafetyHookMid GameplayFOVMidHook{};
            GameplayFOVMidHook = safetyhook::create_mid(GameplayFOVFunctionAddr,
                [](SafetyHookContext& ctx) {
                    if (ctx.rax != 0)
                        ctx.xmm1.f32[0] *= fGameplayFOVMulti;
                });
        }
        else if (!GameplayFOVScanResult) {
            spdlog::error("FOV: Gameplay: Pattern scan failed.");
        }
    }
}

void HUD() 
{
    if (bFixHUD) {
        // HUD Size
        uint8_t* HUDWidthScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F ?? ?? 66 0F ?? ?? 0F ?? ?? F3 0F ?? ??");
        if (HUDWidthScanResult) {
            spdlog::info("HUD: Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDWidthScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDWidthMidHook{};
            HUDWidthMidHook = safetyhook::create_mid(HUDWidthScanResult + 0xD,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm6.f32[0] = (float)iCurrentResX / (2160.00f * fAspectRatio);
                });

            static SafetyHookMid HUDHeightMidHook{};
            HUDHeightMidHook = safetyhook::create_mid(HUDWidthScanResult + 0x24,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = (float)iCurrentResY / (3840.00f / fAspectRatio);
                });
        }
        else if (!HUDWidthScanResult) {
            spdlog::error("HUD: Size: Pattern scan failed.");
        }

        // Fades
        uint8_t* FadesScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? ?? ?? ?? ?? 89 ?? ?? 0F ?? ?? ?? ?? ?? ?? C1 ?? 08");
        if (FadesScanResult) {
            spdlog::info("HUD: Fades: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FadesScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FadesMidHook{};
            FadesMidHook = safetyhook::create_mid(FadesScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rbx + 0x40) {
                        if (*reinterpret_cast<float*>(ctx.rbx + 0x64) == 2160.00f && *reinterpret_cast<float*>(ctx.rbx + 0x80) == 3840.00f) {
                            if (fAspectRatio > fNativeAspect) {
                                float fWidthOffset = ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                                *reinterpret_cast<float*>(ctx.rbx + 0x80) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0x40) = -fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0x60) = -fWidthOffset;
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                float fHeightOffset = ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                                *reinterpret_cast<float*>(ctx.rbx + 0x64) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0x44) = -fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rbx + 0x84) = -fHeightOffset;
                            }
                        }
                    }
                });
        }
        else if (!FadesScanResult) {
            spdlog::error("HUD: Fades: Pattern scan failed.");
        }

        // Pause Screen Capture
        uint8_t* PauseCaptureScanResult = Memory::PatternScan(baseModule, "48 ?? ?? ?? ?? 48 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? ?? 5F 5E 5B C3");
        if (PauseCaptureScanResult) {
            spdlog::info("HUD: Pause Capture: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)PauseCaptureScanResult - (uintptr_t)baseModule);
            static SafetyHookMid PauseCaptureMidHook{};
            PauseCaptureMidHook = safetyhook::create_mid(PauseCaptureScanResult + 0xA,
                [](SafetyHookContext& ctx) {
                    if (ctx.rsp + 0x60) {
                        if (fAspectRatio > fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x60) = -(((2160.00f * fAspectRatio) - 3840.00f) / 2.00f);
                            *reinterpret_cast<float*>(ctx.rsp + 0x70) = 2160.00f * fAspectRatio;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.rsp + 0x64) = -(((3840.00f / fAspectRatio) - 2160.00f) / 2.00f);
                            *reinterpret_cast<float*>(ctx.rsp + 0x74) = 3840.00f / fAspectRatio;
                        }
                    }
                });
        }
        else if (!PauseCaptureScanResult) {
            spdlog::error("HUD: Pause Capture: Pattern scan failed.");
        }

        // HUD Offset
        uint8_t* HUDOffsetScanResult = Memory::PatternScan(baseModule, "F2 0F ?? ?? ?? ?? 0F ?? ?? 0F ?? ?? ?? ?? 45 ?? ?? 74 ?? 48 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? 48 ?? ?? FF ?? ??");
        uint8_t* HUDOffsetClipScanResult = Memory::PatternScan(baseModule, "66 0F ?? ?? ?? ?? 0F ?? ?? 0F ?? ?? ?? ?? 45 ?? ?? 74 ?? 48 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? 48 ?? ?? FF ?? ??");
        if (HUDOffsetScanResult && HUDOffsetClipScanResult) {
            spdlog::info("HUD: Offset: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDOffsetMidHook{};
            HUDOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult + 0x9,
                [](SafetyHookContext& ctx) {
                    if (ctx.r12 == 1) {
                        if (fAspectRatio > fNativeAspect)
                            ctx.xmm0.f32[0] += ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                        if (fAspectRatio < fNativeAspect)
                            ctx.xmm0.f32[1] += ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                    }
                });

            spdlog::info("HUD: Offset: Clipping: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetClipScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDOffsetClipMidHook{};
            HUDOffsetClipMidHook = safetyhook::create_mid(HUDOffsetClipScanResult + 0x9,
                [](SafetyHookContext& ctx) {
                    if (ctx.r12 == 1) {
                        if (fAspectRatio > fNativeAspect)
                            ctx.xmm0.f32[0] += ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                        if (fAspectRatio < fNativeAspect)
                            ctx.xmm0.f32[1] += ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                    }
                });
        }
        else if (!HUDOffsetScanResult || !HUDOffsetClipScanResult) {
            spdlog::error("HUD: Offset: Pattern scan(s) failed.");
        }

        // Screen Position
        uint8_t* ScreenPosHorScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 48 8B ?? ?? ?? C5 ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? ?? ??");
        uint8_t* ScreenPosVertScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? C5 ?? ?? ?? 48 ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ??");
        if (ScreenPosHorScanResult && ScreenPosVertScanResult) {
            spdlog::info("HUD: ScreenPos: Horizontal: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ScreenPosHorScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ScreenPosHorMidHook{};
            ScreenPosHorMidHook = safetyhook::create_mid(ScreenPosHorScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = 2160.00f * fAspectRatio;
                });

            static SafetyHookMid ScreenPosHorOffsetMidHook{};
            ScreenPosHorOffsetMidHook = safetyhook::create_mid(ScreenPosHorScanResult + 0x21,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] -= ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                });

            spdlog::info("HUD: ScreenPos: Vertical: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ScreenPosVertScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ScreenPosVertMidHook{};
            ScreenPosVertMidHook = safetyhook::create_mid(ScreenPosVertScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = 3840.00f / fAspectRatio;
                });

            static SafetyHookMid ScreenPosVertOffsetMidHook{};
            ScreenPosVertOffsetMidHook = safetyhook::create_mid(ScreenPosHorScanResult + 0x11,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm8.f32[0] -= ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                });
        }
        else if (!ScreenPosHorScanResult || !ScreenPosVertScanResult) {
            spdlog::error("HUD: ScreenPos: Pattern scan(s) failed.");
        }

        // Adjust individual HUD elements
        uint8_t* ElementSizeScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 8B ?? ?? 0F ?? ?? ?? ?? 89 ?? ?? 8B ?? ?? ?? 89 ?? ??");
        if (ElementSizeScanResult) {
            spdlog::info("HUD: Element Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ElementSizeScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ElementSizeMidHook{};
            ElementSizeMidHook = safetyhook::create_mid(ElementSizeScanResult + 0x3,
                [](SafetyHookContext& ctx) {
                    if (ctx.r8 + 0x18 && ctx.rdi + 0xC0) {
                        // Get name of SpriteStudio 6 APK
                        char* sAPKName = reinterpret_cast<char*>(ctx.rdi + 0xC0);

                        // Cinematic letterboxing
                        if (sAPKName != nullptr && std::string(sAPKName) == "event_face") {
                            if (ctx.xmm14.f32[0] == 1920.00f && (ctx.xmm3.f32[0] == 1898.00f || ctx.xmm3.f32[0] == 262.00f))
                            {
                                if (fAspectRatio > fNativeAspect) {
                                    ctx.xmm6.f32[0] *= fAspectMultiplier;
                                }
                                else if (fAspectRatio < fNativeAspect) {
                                    ctx.xmm5.f32[0] /= fAspectMultiplier;
                                }
                            }
                        }

                        // "common_wipe", "mask"
                    }
                });
        }
        else if (!ElementSizeScanResult) {
            spdlog::error("HUD: Element Size: Pattern scan failed.");
        }

        // Fade Wipe
        uint8_t* FadeWipeScanResult = Memory::PatternScan(baseModule, "48 ?? ?? B2 01 48 ?? ?? FF ?? ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? 48 ?? ?? 0F 84 ?? ?? ?? ??");
        if (FadeWipeScanResult) {
            spdlog::info("HUD: Fade Wipe: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FadeWipeScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FadeWipeMidHook{};
            FadeWipeMidHook = safetyhook::create_mid(FadeWipeScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rdi) {
                        if (*reinterpret_cast<float*>(ctx.rdi + 0xD0) == 3840.00f && *reinterpret_cast<float*>(ctx.rdi + 0xB4) == 2160.00f) {
                            if (fAspectRatio > fNativeAspect) {
                                float fWidthOffset = ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                                // 0 - This needs to remain at 16:9.
                                //*reinterpret_cast<float*>(ctx.rdi + 0xD0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0xF0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0x90) = -fWidthOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0xB0) = -fWidthOffset;
                                // 1
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xD0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xF0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0x90) = -fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xB0) = -fWidthOffset;
                                // 2
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xD0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xF0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0x90) = -fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xB0) = -fWidthOffset;
                                // 3
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xD0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xF0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0x90) = -fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xB0) = -fWidthOffset;
                                // 4
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xD0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xF0) = (2160.00f * fAspectRatio) - fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0x90) = -fWidthOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xB0) = -fWidthOffset;
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                float fHeightOffset = ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                                // 0 - This needs to remain at 16:9.
                                //*reinterpret_cast<float*>(ctx.rdi + 0xB4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0xF4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0x94) = -fHeightOffset;
                                //*reinterpret_cast<float*>(ctx.rdi + 0xD4) = -fHeightOffset;
                                // 1
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xB4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xF4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0x94) = -fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0xE0 + 0xD4) = -fHeightOffset;
                                // 2
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xB4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xF4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0x94) = -fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x1C0 + 0xD4) = -fHeightOffset;
                                // 3
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xB4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xF4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0x94) = -fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x2A0 + 0xD4) = -fHeightOffset;
                                // 4
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xB4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xF4) = (3840.00f / fAspectRatio) - fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0x94) = -fHeightOffset;
                                *reinterpret_cast<float*>(ctx.rdi + 0x380 + 0xD4) = -fHeightOffset;
                            }
                        }
                    }
                });
        }
        else if (!FadeWipeScanResult) {
            spdlog::error("HUD: Fade Wipe: Pattern scan failed.");
        }

        // CameraPane Size
        uint8_t* CameraPaneScanResult = Memory::PatternScan(baseModule, "41 ?? ?? ?? 0F ?? ?? ?? 0F ?? ?? ?? 41 0F ?? ?? ?? 0F ?? ?? ?? 0F ?? ?? ?? 0F ?? ?? ?? 0F ?? ?? ?? ?? ?? ??");
        if (CameraPaneScanResult) {
            spdlog::info("HUD: CameraPane Size: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CameraPaneScanResult - (uintptr_t)baseModule);
            static SafetyHookMid CameraPaneWidthMidHook{};
            CameraPaneWidthMidHook = safetyhook::create_mid(CameraPaneScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm10.f32[0] = fHUDWidth / 2.00f;
                });

            static SafetyHookMid CameraPaneHeightMidHook{};
            CameraPaneHeightMidHook = safetyhook::create_mid(CameraPaneScanResult - 0x13,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm4.f32[0] = fHUDHeight / 2.00f;
                });
        }
        else if (!CameraPaneScanResult) {
            spdlog::error("HUD: CameraPane Size: Pattern scan failed.");
        }
    }   

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
    if (bMenuFPSCap) {
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
        // Fix 8-way analog gating
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
    
    if (bForceControllerIcons) {
        // Force Controller Icons
        uint8_t* KeyboardIconsScanResult = Memory::PatternScan(baseModule, "84 ?? 74 ?? C7 ?? ?? ?? ?? ?? 02 00 00 00 48 ?? ?? ?? 5B C3");
        uint8_t* MouseIcons1ScanResult = Memory::PatternScan(baseModule, "E8 ?? ?? ?? ?? 48 ?? ?? ?? 5B E9 ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? 01 00 00 00 48 ?? ?? ?? 5B C3");
        uint8_t* MouseIcons2ScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? ?? ?? ?? 01 00 00 00 E8 ?? ?? ?? ?? 83 ?? 01 75 ?? 0F ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 ?? 0F 85 ?? ?? ?? ?? 4C ?? ?? ?? ??");
        if (KeyboardIconsScanResult && MouseIcons1ScanResult && MouseIcons2ScanResult) {
            spdlog::info("Force Controller Icons: Keyboard: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)KeyboardIconsScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)KeyboardIconsScanResult + 0xA, "\x00", 1);

            spdlog::info("Force Controller Icons: Mouse: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MouseIcons1ScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)MouseIcons1ScanResult + 0x15, "\x00", 1);

            spdlog::info("Force Controller Icons: Mouse: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MouseIcons2ScanResult - (uintptr_t)baseModule);
            Memory::PatchBytes((uintptr_t)MouseIcons2ScanResult + 0x6, "\x00", 1);
        }
        else if (!KeyboardIconsScanResult || !MouseIcons1ScanResult || !MouseIcons2ScanResult) {
            spdlog::error("Force Controller Icons: Pattern scan(s) failed.");
        }
    }

    if (bDisableCameraShake) {
        // Camera Shake
        uint8_t* CameraShakeScanResult = Memory::PatternScan(baseModule, "41 ?? ?? 05 44 ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? C4 ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ??");
        if (CameraShakeScanResult) {
            spdlog::info("Camera Shake: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CameraShakeScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)CameraShakeScanResult + 0x3, (BYTE)4);
            spdlog::info("Camera Shake: Patched instruction.");
        }
        else if (!CameraShakeScanResult) {
            spdlog::error("Camera Shake: Pattern scan failed.");
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
        uint8_t* FoliageDistanceScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? 73 ?? C5 ?? ?? ?? EB ?? C5 ?? ?? ?? ?? ?? ?? ?? EB ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? 77 ??");
        if (LODDistanceScanResult && FoliageDistanceScanResult) {
            spdlog::info("LOD: Distance: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)LODDistanceScanResult - (uintptr_t)baseModule);
            LODDistanceAddr = Memory::GetAbsolute((uintptr_t)LODDistanceScanResult + 0x4);
            spdlog::info("LOD: Distance: Value address is {:s}+{:x}", sExeName.c_str(), LODDistanceAddr - (uintptr_t)baseModule);

            // Big number scary
            static float fRealLODDistance = fLODDistance * 1000.00f;
            // This value can be modified directly since it's only accessed by one function. 
            Memory::Write(LODDistanceAddr, fRealLODDistance);

            spdlog::info("LOD: Foliage: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)FoliageDistanceScanResult - (uintptr_t)baseModule);
            static SafetyHookMid FoliageDistanceMidHook{};
            FoliageDistanceMidHook = safetyhook::create_mid(FoliageDistanceScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm0.f32[0] = fRealLODDistance; // Default is 5000
                });            
        }
        else if (!LODDistanceScanResult || !FoliageDistanceScanResult) {
            spdlog::error("LOD: Pattern scan(s) failed.");
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

    if (iShadowResolution != 2048) {
        uint8_t* ShadowResolutionScanResult = Memory::PatternScan(baseModule, "C7 ?? ?? 00 08 00 00 C7 ?? ?? 00 08 00 00 C7 ?? ?? ?? ?? ?? ?? C7 ?? ?? 01 00 00 00");
        uint8_t* ShadowTexShiftScanResult = Memory::PatternScan(baseModule, "41 ?? ?? 48 ?? ?? ?? 48 ?? ?? FF ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? 4C ?? ?? ?? ??");
        uint8_t* CSMSplitsScanResult = Memory::PatternScan(baseModule, "8B ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? ?? C4 ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? 48 ?? ?? ??");
        if (ShadowResolutionScanResult && ShadowTexShiftScanResult && CSMSplitsScanResult) {
            // Set shadowmap resolution
            spdlog::info("Shadow Quality: Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowResolutionScanResult - (uintptr_t)baseModule);
            Memory::Write((uintptr_t)ShadowResolutionScanResult + 0x3, iShadowResolution);
            Memory::Write((uintptr_t)ShadowResolutionScanResult + 0xA, iShadowResolution);
            spdlog::info("Shadow Quality: Resolution: Patched instruction.");

            // Set shadowTexShift property to account for increased/decreased shadowmap resolution
            spdlog::info("Shadow Quality: ShadowTexShift: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ShadowTexShiftScanResult - (uintptr_t)baseModule);
            static SafetyHookMid ShadowTexShiftMidHook{};
            ShadowTexShiftMidHook = safetyhook::create_mid(ShadowTexShiftScanResult,
                [](SafetyHookContext& ctx) {
                    // Default = 1.00f / 2048 (0.00048828125f)
                    // If this isn't adjusted then shadows can look offset and artifacty
                    ctx.xmm3.f32[0] = (float)1.00f / iShadowResolution;
                });

            if (iShadowResolution > 2048) {
                // Adjust CSM split distances
                // TODO: Is this the right way of scaling CSM split distances? Should they even be adjusted?
                spdlog::info("Shadow Quality: CSM Splits: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CSMSplitsScanResult - (uintptr_t)baseModule);
                static SafetyHookMid CSMSplitsMidHook{};
                CSMSplitsMidHook = safetyhook::create_mid(CSMSplitsScanResult,
                    [](SafetyHookContext& ctx) {
                        ctx.xmm12.f32[0] = ctx.xmm12.f32[0] * (1 + std::log((float)iShadowResolution / 2048.00f));
                    });
            }
        }
        else if (!ShadowResolutionScanResult || !ShadowTexShiftScanResult || !CSMSplitsScanResult) {
            spdlog::error("Shadow Quality: Pattern scan(s) failed.");
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