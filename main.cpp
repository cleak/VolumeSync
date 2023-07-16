#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdio.h>

#include <windows.h>

#include "Audio.h"
#include "resource.h"

#define WM_TRAYMSG (WM_USER + 1)
#define ID_TRAY_ICON 1
#define ID_TRAY_EXIT 2

constexpr uint32_t kTrayBaseDeviceId = 1000;

NOTIFYICONDATA nid;
HWND hWnd;

std::unique_ptr<Audio> audio;

void RedirectOutputToFile(const char* filename) {
    // Open the file and create the stream buffer
    static std::ofstream file(filename);
    std::streambuf* filebuf = file.rdbuf();

    // Redirect std::cout to the file
    std::streambuf* coutbuf = std::cout.rdbuf(filebuf);

    // Redirect stdout to the file
    if (!freopen(filename, "a", stdout)) {
        // If freopen failed, restore std::cout and return nullptr
        std::cout.rdbuf(coutbuf);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TRAYMSG:
            if (lParam == WM_RBUTTONDOWN) {
                // Create the menu
                HMENU hMenu = CreatePopupMenu();

                AppendMenu(hMenu, MF_STRING | MF_GRAYED, 0, TEXT("Volume Sync"));
                AppendMenu(hMenu, MF_SEPARATOR, ID_TRAY_EXIT, nullptr);

                auto syncedDevices = audio->getSyncedDevices();
                auto devices = audio->getDevices();
                for (int i = 0; i < devices.size(); ++i) {
                    auto name = devices[i];
                    auto id = kTrayBaseDeviceId + i;

                    auto flags = MF_STRING;
                    if (syncedDevices.find(name) != syncedDevices.end()) {
                        flags |= MF_CHECKED;
                    }

                    if (name == audio->getDefaultDevice().getName()) {
                        flags |= MF_GRAYED;
                    }

                    AppendMenuW(hMenu, flags, id, name.c_str());
                }

                AppendMenu(hMenu, MF_SEPARATOR, ID_TRAY_EXIT, nullptr);
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, TEXT("Exit"));

                // Get current mouse position
                POINT pt;
                GetCursorPos(&pt);

                // Show the menu
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
                PostMessage(hWnd, WM_NULL, 0, 0);

                // Destroy the menu
                DestroyMenu(hMenu);
            } else if (lParam == WM_LBUTTONDOWN) {
                break;
            }
            break;
        case WM_COMMAND:
            if (wParam == ID_TRAY_EXIT) {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            } else if (wParam > kTrayBaseDeviceId && wParam < 2 * kTrayBaseDeviceId) {
                std::cout << "Toggle sync for device ID: " << wParam - kTrayBaseDeviceId << std::endl;
                auto devices = audio->getDevices();
                auto name = devices[wParam - kTrayBaseDeviceId];
                auto syncedDevices = audio->getSyncedDevices();
                if (syncedDevices.find(name) == syncedDevices.end()) {
                    audio->addSyncTo(name);
                } else {
                    audio->removeSyncTo(name);
                }
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// int main() {
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE mutex = CreateMutex(NULL, TRUE, "VolumeSync_Service");
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) {
            ReleaseMutex(mutex);
        }
        // Another instance is running
        return 1;
    }

#ifdef _DEBUG 
    RedirectOutputToFile("log.txt");
#endif // _DEBUG

    audio = std::make_unique<Audio>();

    // Register class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("VolumeSync");
    if (!RegisterClass(&wc)) return 1;

    // Create the hidden window
    hWnd = CreateWindowEx(0, TEXT("VolumeSync"), NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!hWnd) return 2;

    // Add icon to system tray
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYMSG;

    HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    if (icon == NULL) {
        DWORD err = GetLastError();
        std::cout << "Failed to load icon. Error: " << err << std::endl;
    } else {
        nid.hIcon = icon;
    }

    strcpy_s(nid.szTip, "Volume Sync");
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return (int)msg.wParam;
}
