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
std::string sFixVer = "0.7.4";
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
bool bPauseOnFocusLoss;
bool bCatchAltF4;
bool bFixFPSCap;
bool bDisableDashBlur;
bool bAORes;
float fGameplayFOVMulti;

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

    inipp::get_value(ini.sections["Gameplay FOV"], "Multiplier", fGameplayFOVMulti);
    if (fGameplayFOVMulti < 0.10f || fGameplayFOVMulti > 3.00f) {
        fGameplayFOVMulti = std::clamp(fGameplayFOVMulti, 0.10f, 3.00f);
        spdlog::warn("Config Parse: fGameplayFOVMulti value invalid, clamped to {}", fGameplayFOVMulti);
    }
    spdlog::info("Config Parse: fGameplayFOVMulti: {}", fGameplayFOVMulti);

    inipp::get_value(ini.sections["Game Window"], "PauseOnFocusLoss", bPauseOnFocusLoss);
    inipp::get_value(ini.sections["Game Window"], "CatchAltF4", bCatchAltF4);
    spdlog::info("Config Parse: bPauseOnFocusLoss: {}", bPauseOnFocusLoss);
    spdlog::info("Config Parse: bCatchAltF4: {}", bCatchAltF4);

    inipp::get_value(ini.sections["Fix Framerate Cap"], "Enabled", bFixFPSCap);
    spdlog::info("Config Parse: bFixFPSCap: {}", bFixFPSCap);

    inipp::get_value(ini.sections["Disable Dash Blur"], "Enabled", bDisableDashBlur);
    spdlog::info("Config Parse: bDisableDashBlur: {}", bDisableDashBlur);

    inipp::get_value(ini.sections["Ambient Occlusion Resolution"], "Enabled", bAORes);
    spdlog::info("Config Parse: bAORes: {}", bAORes);

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
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        if (w_param == WA_INACTIVE && !bPauseOnFocusLoss) {
            // Disable pause on focus loss.
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!bCatchAltF4) {
            // Return default WndProc
            return DefWindowProc(window, message_type, w_param, l_param);
        }
        break;
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

    if (fGameplayFOVMulti != 1.00f) {
        // Gameplay FOV
        uint8_t* GameplayFOVScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? C7 ?? ?? 09 00 00 00 E9 ?? ?? ?? ??");
        if (GameplayFOVScanResult) {
            spdlog::info("FOV: Gameplay: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)GameplayFOVScanResult - (uintptr_t)baseModule);
            uintptr_t GameplayFOVFunctionAddr = Memory::GetAbsolute((uintptr_t)GameplayFOVScanResult + 0xC);
            spdlog::info("FOV: Gameplay: Function address is {:s}+{:x}", sExeName.c_str(), GameplayFOVFunctionAddr - (uintptr_t)baseModule);

            static SafetyHookMid GameplayFOVMidHook{};
            GameplayFOVMidHook = safetyhook::create_mid(GameplayFOVFunctionAddr,
                [](SafetyHookContext& ctx) {
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
                    if (ctx.rbx + 0x64) {
                        if (*reinterpret_cast<float*>(ctx.rbx + 0x64) == 2160.00f && *reinterpret_cast<float*>(ctx.rbx + 0x80) == 3840.00f) {
                            if (fAspectRatio > fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x80) = 2160.00f * fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA0) = 2160.00f * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x64) = 3840.00f / fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA4) = 3840.00f / fAspectRatio;
                            }
                        }
                    }
                });
        }
        else if (!FadesScanResult) {
            spdlog::error("HUD: Fades: Pattern scan failed.");
        }

        // Backgrounds 1
        uint8_t* Backgrounds1ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 66 0F ?? ?? ?? F3 0F ?? ?? ?? ?? 66 0F ?? ?? ??");
        if (Backgrounds1ScanResult) {
            spdlog::info("HUD: Backgrounds: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)Backgrounds1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid Backgrounds1MidHook{};
            Backgrounds1MidHook = safetyhook::create_mid(Backgrounds1ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rbx + 0x64) {
                        if (*reinterpret_cast<float*>(ctx.rbx + 0x64) == 2160.00f && *reinterpret_cast<float*>(ctx.rbx + 0x80) == 3840.00f) {
                            if (fAspectRatio > fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x80) = 2160.00f * fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA0) = 2160.00f * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x64) = 3840.00f / fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA4) = 3840.00f / fAspectRatio;
                            }
                        }
                    }
                });
        }
        else if (!Backgrounds1ScanResult) {
            spdlog::error("HUD: Backgrounds: 1: Pattern scan failed.");
        }

        // Backgrounds 2
        uint8_t* Backgrounds2ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 66 0F ?? ?? ?? F3 0F ?? ?? ?? ?? 66 0F ?? ?? ??");
        if (Backgrounds2ScanResult) {
            spdlog::info("HUD: Backgrounds: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)Backgrounds2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid Backgrounds2MidHook{};
            Backgrounds2MidHook = safetyhook::create_mid(Backgrounds2ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rbx + 0x64) {
                        if (*reinterpret_cast<float*>(ctx.rbx + 0x64) == 2160.00f && *reinterpret_cast<float*>(ctx.rbx + 0x80) == 3840.00f) {
                            if (fAspectRatio > fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x80) = 2160.00f * fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA0) = 2160.00f * fAspectRatio;
                            }
                            else if (fAspectRatio < fNativeAspect) {
                                *reinterpret_cast<float*>(ctx.rbx + 0x64) = 3840.00f / fAspectRatio;
                                *reinterpret_cast<float*>(ctx.rbx + 0xA4) = 3840.00f / fAspectRatio;
                            }
                        }
                    }
                });
        }
        else if (!Backgrounds2ScanResult) {
            spdlog::error("HUD: Backgrounds: 2: Pattern scan failed.");
        }

        
        // HUD Offset 
        uint8_t* HUDOffset1ScanResult = Memory::PatternScan(baseModule, "F2 41 ?? ?? ?? ?? ?? ?? ?? 4C ?? ?? ?? ?? ?? ?? 0F 28 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ??");
        uint8_t* HUDOffset2ScanResult = Memory::PatternScan(baseModule, "F3 0F ?? ?? 74 ?? ?? ?? F3 0F ?? ?? 78 ?? ?? ?? C3");
        if (HUDOffset1ScanResult && HUDOffset2ScanResult) {
            spdlog::info("HUD: Offset: 1: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffset1ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDOffset1MidHook{};
            HUDOffset1MidHook = safetyhook::create_mid(HUDOffset1ScanResult + 0x9,
                [](SafetyHookContext& ctx) {
                    if (ctx.r14 + 0xB6C) {
                        std::string sElementName = (std::string)(char*)(ctx.r14 + 0x10);

                        // Check if marked or 0.00f
                        if ( (*reinterpret_cast<int*>(ctx.r14 + 0xB6C) == 42069 || *reinterpret_cast<float*>(ctx.r14 + 0xB74) == 0.00f) 
                            && !sElementName.contains("minimap_icon") ) {
                            if (fAspectRatio > fNativeAspect)
                                ctx.xmm2.f32[0] += ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;

                            if (fAspectRatio < fNativeAspect)
                                ctx.xmm2.f32[1] += ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                        }  
                    }  
                });

            spdlog::info("HUD: Offset: 2: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffset2ScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDOffset2MidHook{};
            HUDOffset2MidHook = safetyhook::create_mid(HUDOffset2ScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rcx + 0xB6C) {
                        // Write marker
                        *reinterpret_cast<int*>(ctx.rcx + 0xB6C) = 42069;
                        // Subtract hud offset
                        if (fAspectRatio > fNativeAspect)
                            ctx.xmm1.f32[0] -= ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;

                        if (fAspectRatio < fNativeAspect)
                            ctx.xmm2.f32[0] -= ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;     
                    }
                });
        }
        else if (!HUDOffset1ScanResult || !HUDOffset2ScanResult) {     
            spdlog::error("HUD: Offset: Pattern scan failed.");
        }

        /*
        // HUD Offset (Alt)
        uint8_t* HUDOffsetScanResult = Memory::PatternScan(baseModule, "45 ?? ?? 8B ?? ?? 0F ?? ?? ?? ?? 89 ?? ?? 8B ?? ?? ?? 89 ?? ??");
        if (HUDOffsetScanResult) {
            spdlog::info("HUD: Offset: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetScanResult - (uintptr_t)baseModule);
            static SafetyHookMid HUDOffset1MidHook{};
            HUDOffset1MidHook = safetyhook::create_mid(HUDOffsetScanResult + 0x3,
                [](SafetyHookContext& ctx) {
                        if (ctx.xmm3.f32[0] == 1080 || ctx.xmm14.f32[0] == 1920.00f) {
                            if (fAspectRatio > fNativeAspect)
                                ctx.xmm14.f32[0] += ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                            if (fAspectRatio < fNativeAspect)
                                ctx.xmm3.f32[0] += ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                        }
                });
        }
        else if (!HUDOffsetScanResult) {
            spdlog::error("HUD: Offset: Pattern scan failed.");
        }
        */

        // Mouse
        uint8_t* MouseHorScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 48 8B ?? ?? ?? C5 ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? ?? ??");
        uint8_t* MouseVertScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? 48 ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ??");
        if (MouseHorScanResult && MouseVertScanResult) {
            spdlog::info("HUD: Mouse: Horizontal: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MouseHorScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MouseHorMidHook{};
            MouseHorMidHook = safetyhook::create_mid(MouseHorScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.rax = (int)std::round(fHUDWidth);
                });

            spdlog::info("HUD: Mouse: Vertical: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)MouseHorScanResult - (uintptr_t)baseModule);
            static SafetyHookMid MouseVertMidHook{};
            MouseVertMidHook = safetyhook::create_mid(MouseVertScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.rax = (int)std::round(fHUDHeight);
                });
        }
        else if (!MouseHorScanResult || !MouseVertScanResult) {
            spdlog::error("HUD: Mouse: Pattern scan(s) failed.");
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

void Framerate() 
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

    if (bAORes) {
        // Ambient Occlusion Resolution
        uint8_t* AOResolutionScanResult = Memory::PatternScan(baseModule, "8B ?? 48 ?? ?? ?? ?? ?? ?? 48 ?? ?? 74 ?? E8 ?? ?? ?? ?? 41 ?? 00 40 00 00 41 ?? 16 00 00 00");
        if (!AOResolutionScanResult) {
            spdlog::info("Ambient Occlusion Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)AOResolutionScanResult - (uintptr_t)baseModule);
            static SafetyHookMid AOResolutionMidHook{};
            AOResolutionMidHook = safetyhook::create_mid(AOResolutionScanResult,
                [](SafetyHookContext& ctx) {
                    if (ctx.rax + 0x888) {
                        int iResScale = *reinterpret_cast<int*>(ctx.rax + 0x888);
                        // 0 = 200%, 1 = 175%, 2 = 150%, 3 = 125%, 4 = 100%, 5 = 75%, 6 = 50%

                        // Run ambient occlusion at native resolution when beyond 100% resolution scale.
                        if (iResScale < 4) {
                            ctx.rbx = iCurrentResX;
                            ctx.rax = iCurrentResY;
                        }
                    }
                });
        }
        else if (!AOResolutionScanResult) {
            spdlog::error("Ambient Occlusion Resolution: Pattern scan failed.");
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
    Framerate();
    Graphics();
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        thisModule = hModule;
        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, CREATE_SUSPENDED, 0);
        if (mainHandle)
        {
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