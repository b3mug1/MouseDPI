#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <format>
#include "resource_ids.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

const wchar_t* APP_NAME = L"MouseControl";
const wchar_t* APP_VERSION = L"1.0.0";
const wchar_t* PUBLISHER = L"MouseControl Project";

std::wstring GetKnownFolderPath(REFKNOWNFOLDERID rfid) {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, nullptr, &path))) {
        std::wstring result(path);
        CoTaskMemFree(path);
        return result;
    }
    return L"";
}

bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& workingDir, const std::wstring& description) {
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (FAILED(hr)) return false;

    shellLink->SetPath(targetPath.c_str());
    shellLink->SetWorkingDirectory(workingDir.c_str());
    shellLink->SetDescription(description.c_str());

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    hr = shellLink.As(&persistFile);
    if (FAILED(hr)) return false;

    hr = persistFile->Save(shortcutPath.c_str(), TRUE);
    return SUCCEEDED(hr);
}

bool ExtractResourceToFile(HMODULE hMod, int resId, const std::wstring& destPath) {
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(10));
    if (!hRes) return false;

    HGLOBAL hLoaded = LoadResource(hMod, hRes);
    if (!hLoaded) return false;

    DWORD size = SizeofResource(hMod, hRes);
    void* pData = LockResource(hLoaded);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, nullptr);
    CloseHandle(hFile);

    return ok && (written == size);
}

void TerminateAppIfRunning() {
    HWND hExisting = FindWindowW(L"MouseControlMainWindowClass", nullptr);
    if (hExisting) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hExisting, &pid);
        if (pid != 0) {
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProc) {
                TerminateProcess(hProc, 0);
                CloseHandle(hProc);
                Sleep(500);
            }
        }
    }
}

int RunInstaller(HINSTANCE hInstance, bool silent) {
    std::wstring localAppData = GetKnownFolderPath(FOLDERID_LocalAppData);
    if (localAppData.empty()) {
        if (!silent) {
            MessageBoxW(nullptr, L"Failed to determine %LOCALAPPDATA% directory.", APP_NAME, MB_ICONERROR | MB_OK);
        }
        return 1;
    }

    std::wstring installDir = localAppData + L"\\" + APP_NAME;

    if (!silent) {
        std::wstring prompt = std::format(
            L"Welcome to {} Setup!\n\n"
            L"This will install {} v{} for your user profile.\n\n"
            L"Install location:\n{}\n\n"
            L"Do you want to proceed with the installation?",
            APP_NAME, APP_NAME, APP_VERSION, installDir
        );

        int answer = MessageBoxW(nullptr, prompt.c_str(), L"MouseControl Setup", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1);
        if (answer != IDYES) {
            return 0;
        }
    }

    TerminateAppIfRunning();
    CreateDirectoryW(installDir.c_str(), nullptr);

    // Extract payloads
    struct ResourcePayload {
        int resId;
        const wchar_t* filename;
    };

    ResourcePayload payloads[] = {
        { IDR_BIN_MOUSECONTROL,     L"MouseControl.exe" },
        { IDR_BIN_DEVICE_INSPECTOR, L"device_inspector.exe" },
        { IDR_BIN_FMT_DLL,          L"fmt.dll" },
        { IDR_BIN_SPDLOG_DLL,       L"spdlog.dll" },
        { IDR_BIN_SQLITE3_DLL,      L"sqlite3.dll" }
    };

    for (const auto& item : payloads) {
        std::wstring target = installDir + L"\\" + item.filename;
        if (!ExtractResourceToFile(hInstance, item.resId, target)) {
            std::wstring err = std::format(L"Failed to extract component: {}", item.filename);
            MessageBoxW(nullptr, err.c_str(), L"Installation Error", MB_ICONERROR | MB_OK);
            return 1;
        }
    }

    // Copy installer executable as uninstaller
    wchar_t currentExe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    std::wstring uninstallerPath = installDir + L"\\uninstall.exe";
    CopyFileW(currentExe, uninstallerPath.c_str(), FALSE);

    std::wstring mainExePath = installDir + L"\\MouseControl.exe";

    // Create Start Menu shortcut
    std::wstring programsDir = GetKnownFolderPath(FOLDERID_Programs);
    std::wstring startShortcut = programsDir + L"\\" + APP_NAME + L".lnk";
    CreateShortcut(mainExePath, startShortcut, installDir, L"Universal Gaming Mouse Management Software");

    // Create Desktop shortcut
    std::wstring desktopDir = GetKnownFolderPath(FOLDERID_Desktop);
    std::wstring desktopShortcut = desktopDir + L"\\" + APP_NAME + L".lnk";
    CreateShortcut(mainExePath, desktopShortcut, installDir, L"Universal Gaming Mouse Management Software");

    // Register in Windows Registry (Settings -> Installed Apps)
    HKEY hKey = nullptr;
    std::wstring regSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + std::wstring(APP_NAME);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, regSubKey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        auto set_sz = [&](const wchar_t* name, const std::wstring& val) {
            RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(val.c_str()), static_cast<DWORD>((val.length() + 1) * sizeof(wchar_t)));
        };
        auto set_dw = [&](const wchar_t* name, DWORD val) {
            RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
        };

        set_sz(L"DisplayName", APP_NAME);
        set_sz(L"DisplayVersion", APP_VERSION);
        set_sz(L"Publisher", PUBLISHER);
        set_sz(L"InstallLocation", installDir);
        set_sz(L"DisplayIcon", mainExePath + L",0");
        set_sz(L"UninstallString", L"\"" + uninstallerPath + L"\" --uninstall");
        set_dw(L"NoModify", 1);
        set_dw(L"NoRepair", 1);
        RegCloseKey(hKey);
    }

    if (!silent) {
        std::wstring successMsg = std::format(
            L"MouseControl v{} has been successfully installed!\n\n"
            L"Installed to:\n{}\n\n"
            L"Would you like to launch MouseControl now?",
            APP_VERSION, installDir
        );

        int launchAnswer = MessageBoxW(nullptr, successMsg.c_str(), L"Installation Complete", MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);
        if (launchAnswer == IDYES) {
            ShellExecuteW(nullptr, L"open", mainExePath.c_str(), nullptr, installDir.c_str(), SW_SHOWNORMAL);
        }
    }

    return 0;
}

int RunUninstaller(bool silent) {
    if (!silent) {
        std::wstring prompt = std::format(L"Are you sure you want to completely remove {} and all its components?", APP_NAME);
        int answer = MessageBoxW(nullptr, prompt.c_str(), L"MouseControl Uninstall", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
        if (answer != IDYES) {
            return 0;
        }
    }

    TerminateAppIfRunning();

    // 1. Remove Registry entries
    std::wstring regSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + std::wstring(APP_NAME);
    RegDeleteKeyW(HKEY_CURRENT_USER, regSubKey.c_str());

    HKEY hRunKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hRunKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hRunKey, APP_NAME);
        RegCloseKey(hRunKey);
    }

    // 2. Remove Shortcuts
    std::wstring programsDir = GetKnownFolderPath(FOLDERID_Programs);
    std::wstring startShortcut = programsDir + L"\\" + APP_NAME + L".lnk";
    DeleteFileW(startShortcut.c_str());

    std::wstring desktopDir = GetKnownFolderPath(FOLDERID_Desktop);
    std::wstring desktopShortcut = desktopDir + L"\\" + APP_NAME + L".lnk";
    DeleteFileW(desktopShortcut.c_str());

    // 3. Remove Installed Files
    std::wstring localAppData = GetKnownFolderPath(FOLDERID_LocalAppData);
    std::wstring installDir = localAppData + L"\\" + APP_NAME;

    const wchar_t* filesToDelete[] = {
        L"MouseControl.exe",
        L"device_inspector.exe",
        L"fmt.dll",
        L"spdlog.dll",
        L"sqlite3.dll",
        L"mousecontrol.db",
        L"mousecontrol.db-shm",
        L"mousecontrol.db-wal"
    };

    for (const auto* f : filesToDelete) {
        std::wstring p = installDir + L"\\" + f;
        DeleteFileW(p.c_str());
    }

    // Self-delete uninstaller via cmd script
    std::wstring cmd = std::format(
        L"/c timeout /t 1 /nobreak >nul & del /f /q \"{}\" & rmdir \"{}\"",
        installDir + L"\\uninstall.exe",
        installDir
    );
    ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);

    if (!silent) {
        MessageBoxW(nullptr, L"MouseControl has been successfully uninstalled.", L"Uninstall Complete", MB_ICONINFORMATION | MB_OK);
    }
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    bool isUninstall = false;
    bool isSilent = false;
    if (pCmdLine) {
        std::wstring cmd(pCmdLine);
        if (cmd.find(L"--uninstall") != std::wstring::npos || cmd.find(L"/uninstall") != std::wstring::npos) {
            isUninstall = true;
        }
        if (cmd.find(L"--silent") != std::wstring::npos || cmd.find(L"/silent") != std::wstring::npos ||
            cmd.find(L"/S") != std::wstring::npos || cmd.find(L"-s") != std::wstring::npos) {
            isSilent = true;
        }
    }

    wchar_t currentExe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    std::wstring exeName(currentExe);
    if (exeName.find(L"uninstall") != std::wstring::npos) {
        isUninstall = true;
    }

    int result = isUninstall ? RunUninstaller(isSilent) : RunInstaller(hInstance, isSilent);

    CoUninitialize();
    return result;
}
