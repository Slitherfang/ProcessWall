#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <shellapi.h>
#include <random>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

using namespace std;

// 自定义对话框控件ID
#define IDC_MESSAGE_TEXT 1001
#define IDC_OK_BUTTON    1002

// 函数声明
bool IsProcessRunning(const wstring& exePath);
bool StartProcess(const wstring& exePath);
wstring GetProcessPath(DWORD processID);
void ShowRandomMessage(HWND hParent, const wstring& text, const wstring& caption);
LRESULT CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
bool CaseInsensitiveCompare(const wstring& str1, const wstring& str2);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 初始化随机种子
    srand(time(0));

    // 防止自身多开
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ProcessGuardianMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ShowRandomMessage(nullptr, L"本实例已在运行！", L"Oops!");
        return 0;
    }

    // 解析命令行参数
    int argc;
    LPWSTR* argv = CommandLineToArgvW(pCmdLine, &argc);

    if (argc < 1) {
        ShowRandomMessage(nullptr, L"Usage: ProcessGuardian.exe <Program Path>", L"ArgErr");
        ReleaseMutex(hMutex);
        LocalFree(argv);
        return 1;
    }

    wstring targetExe = argv[0];
    transform(targetExe.begin(), targetExe.end(), targetExe.begin(), ::towlower);

    // 验证文件存在
    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ShowRandomMessage(nullptr, L"Target Program does not Exist!\n" + targetExe, L"OhNo!");
        ReleaseMutex(hMutex);
        LocalFree(argv);
        return 2;
    }

    // 检测进程状态
    if (IsProcessRunning(targetExe)) {
        ShowRandomMessage(nullptr, L"实例已在运行", L"Oops!");
    }
    else {
        if (!StartProcess(targetExe)) {
            DWORD err = GetLastError();
            ShowRandomMessage(nullptr, L"Create Process Failed! Error Code: " + to_wstring(err), L"OhNo!");
        }
    }

    // 清理资源
    ReleaseMutex(hMutex);
    LocalFree(argv);
    return 0;
}

// 自定义消息窗口
void ShowRandomMessage(HWND hParent, const wstring& text, const wstring& caption) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"RandomMsgClass";
    RegisterClassW(&wc);
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenWidth = workArea.right - workArea.left;
    int screenHeight = workArea.bottom - workArea.top;

    const int width = 300, height = 150;

    // 确保窗口不超出工作区
    int maxX = screenWidth - width;
    int maxY = screenHeight - height;

    // 生成真随机位置（使用更均匀的分布）
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distX(0, maxX);
    uniform_int_distribution<> distY(0, maxY);

    int x = workArea.left + distX(gen);
    int y = workArea.top + distY(gen);

    // 创建窗口
    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RandomMsgClass",
        caption.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y,
        width, height,
        hParent,
        nullptr,
        wc.hInstance,
        nullptr
    );

    // 添加控件
    CreateWindowW(
        L"STATIC", text.c_str(),
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, 20, 260, 50,
        hDlg, (HMENU)IDC_MESSAGE_TEXT, nullptr, nullptr
    );

    CreateWindowW(
        L"BUTTON", L"Fuck!",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        110, 80, 80, 30,
        hDlg, (HMENU)IDC_OK_BUTTON, nullptr, nullptr
    );

    // 显示窗口
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(hDlg)) break;
    }
}

// 对话框消息处理
LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_OK_BUTTON) {
            DestroyWindow(hWnd);
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 检测进程是否运行
bool IsProcessRunning(const wstring& exePath) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    bool found = false;

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            wstring path = GetProcessPath(pe.th32ProcessID);
            transform(path.begin(), path.end(), path.begin(), ::towlower);
            if (CaseInsensitiveCompare(path, exePath)) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

// 启动进程
bool StartProcess(const wstring& exePath) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;
    wstring cmdLine = L"\"" + exePath + L"\"";

    return CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &si,
        &pi
    );
}

// 获取进程路径
wstring GetProcessPath(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
    if (!hProcess) return L"";

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    wstring result;

    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        result = path;
    }

    CloseHandle(hProcess);
    return result;
}

// 不区分大小写比较
bool CaseInsensitiveCompare(const wstring& str1, const wstring& str2) {
    return _wcsicmp(str1.c_str(), str2.c_str()) == 0;
}