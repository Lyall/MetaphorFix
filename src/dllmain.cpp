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
std::string sFixVer = "0.7.1";
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
bool bFixHUD;
bool bFixMovies;
bool bIntroSkip;
bool bSkipMovie;
bool bPauseOnFocusLoss;
bool bCatchAltF4;

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
int iCurrentResX;
int iCurrentResY;
LPCWSTR sWindowClassName = L"METAPHOR_WINDOW";

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

    inipp::get_value(ini.sections["Fix HUD"], "Enabled", bFixHUD);
    spdlog::info("Config Parse: bFixHUD: {}", bFixHUD);

    inipp::get_value(ini.sections["Fix Movies"], "Enabled", bFixMovies);
    spdlog::info("Config Parse: bFixMovies: {}", bFixMovies);

    inipp::get_value(ini.sections["Game Window"], "PauseOnFocusLoss", bPauseOnFocusLoss);
    spdlog::info("Config Parse: bPauseOnFocusLoss: {}", bPauseOnFocusLoss);

    inipp::get_value(ini.sections["Game Window"], "CatchAltF4", bCatchAltF4);
    spdlog::info("Config Parse: bCatchAltF4: {}", bCatchAltF4);

    inipp::get_value(ini.sections["Intro Skip"], "Enabled", bIntroSkip);
    inipp::get_value(ini.sections["Intro Skip"], "SkipMovie", bSkipMovie);
    spdlog::info("Config Parse: bIntroSkip: {}", bIntroSkip);
    spdlog::info("Config Parse: bSkipMovie: {}", bSkipMovie);

    spdlog::info("----------");

    // Grab desktop resolution/aspect
    DesktopDimensions = Util::GetPhysicalDesktopDimensions();
    iCurrentResX = DesktopDimensions.first;
    iCurrentResY = DesktopDimensions.second;
    CalculateAspectRatio(true);
}

void IntroSkip()
{
    if (bIntroSkip)
    {
        // Intro Skip
        uint8_t* IntroSkipScanResult = Memory::PatternScan(baseModule, "83 ?? 49 0F 87 ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 48 ?? ??");
        if (IntroSkipScanResult)
        {
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
        else if (!IntroSkipScanResult)
        {
            spdlog::error("Intro Skip: Pattern scan failed.");
        }
    }
}

void Resolution()
{
    // Current Resolution
    uint8_t* CurrentResolutionScanResult = Memory::PatternScan(baseModule, "4C ?? ?? ?? ?? ?? ?? ?? 8B ?? 48 ?? ?? ?? ?? ?? ?? ?? C5 ?? ?? ?? C5 ?? ?? ?? 8D ?? ?? C1 ?? 04");
    if (CurrentResolutionScanResult) {
        spdlog::info("Current Resolution: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)CurrentResolutionScanResult - (uintptr_t)baseModule);

        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                int iResX = (int)ctx.rax;
                int iResY = (int)ctx.rdx;

                if (iResX != iCurrentResX || iResY != iCurrentResY) {
                    iCurrentResX = iResX;
                    iCurrentResY = iResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else if (!CurrentResolutionScanResult) {
        spdlog::error("Current Resolution: Pattern scan failed.");
    }

    if (bFixResolution) {
        // Fix resolution
        uint8_t* ResolutionFixScanResult = Memory::PatternScan(baseModule, "C5 ?? ?? ?? 89 ?? ?? ?? ?? ?? C5 ?? ?? ?? 89 ?? ?? ?? ?? ?? 85 ?? 7E ??");
        if (ResolutionFixScanResult) {
            spdlog::info("Resolution Fix: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)ResolutionFixScanResult - (uintptr_t)baseModule);

            static SafetyHookMid ResolutionFixMidHook{};
            ResolutionFixMidHook = safetyhook::create_mid(ResolutionFixScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm7.f32[0] = (float)iCurrentResX;
                    ctx.xmm6.f32[0] = (float)iCurrentResY;
                });
        }
        else if (!ResolutionFixScanResult) {
            spdlog::error("Resolution Fix: Pattern scan failed.");
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
        // app::UI::FadeLayout
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
     
        /*
        // HUD Offset 
        uint8_t* HUDOffsetScanResult = Memory::PatternScan(baseModule, "F2 41 ?? ?? ?? ?? ?? ?? ?? 4C ?? ?? ?? ?? ?? ?? 0F 28 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ??");
        if (HUDOffsetScanResult) {
            spdlog::info("HUD: Offset: Address is {:s}+{:x}", sExeName.c_str(), (uintptr_t)HUDOffsetScanResult - (uintptr_t)baseModule);

            static char* sElementName = 0x0;
            static char* sOldElementName = (char*)"old";

            static SafetyHookMid HUDOffsetMidHook{};
            HUDOffsetMidHook = safetyhook::create_mid(HUDOffsetScanResult + 0x9,
                [](SafetyHookContext& ctx) {
                    sElementName = (char*)ctx.r14 + 0x10;
                    if (strcmp(sOldElementName, sElementName) != 0) {
                        sOldElementName = sElementName;
                        spdlog::info("sElementName = {:s}", sElementName);
                    }

                    if (ctx.xmm2.f32[0] == 0.00f && strcmp(sElementName, "camp_system") != 0 && strcmp(sElementName, "title_menu") != 0) {
                        if (fAspectRatio > fNativeAspect) {
                            *reinterpret_cast<float*>(ctx.r14 + 0xB74) = ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                            ctx.xmm2.f32[0] = ((2160.00f * fAspectRatio) - 3840.00f) / 2.00f;
                        }
                        else if (fAspectRatio < fNativeAspect) {
                            ctx.xmm2.f32[1] = ((3840.00f / fAspectRatio) - 2160.00f) / 2.00f;
                        }
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
            spdlog::error("HUD: Mouse: Pattern scan failed.");
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

HWND hWnd;
WNDPROC OldWndProc;
LRESULT __stdcall NewWndProc(HWND window, UINT message_type, WPARAM w_param, LPARAM l_param) 
{
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
            // Kill ALT+F4 handler.
            return DefWindowProcW(window, message_type, w_param, l_param);
        }
        break;

    default:
        break;
    }

    return CallWindowProc(OldWndProc, window, message_type, w_param, l_param);
};

void WindowManagement()
{
    if (!bPauseOnFocusLoss || !bCatchAltF4) {
        int i = 0;
        while (i < 30 && !IsWindow(hWnd)) {
            // Wait 1 sec then try again
            Sleep(1000);
            i++;
            hWnd = FindWindowW(sWindowClassName, nullptr);
        }

        // If 30 seconds have passed and we still don't have the handle, give up
        if (i == 30) {
            spdlog::error("Window Focus: Failed to find window handle.");
            return;
        }
        else {
            OldWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);

            // Check if SetWindowLongPtr failed
            if (OldWndProc == nullptr) {
                spdlog::error("Window Focus: Failed to set new WndProc.");
                return;
            }
            else {
                spdlog::info("Window Focus: Set new WndProc.");
            }
        }
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    Resolution();
    IntroSkip();
    HUD();
    WindowManagement();
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