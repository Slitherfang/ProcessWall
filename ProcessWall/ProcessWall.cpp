#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "advapi32.lib")

using namespace std;

// 函数声明
bool IsProcessRunning(const wstring& exePath);
bool StartProcess(const wstring& exePath);
wstring GetProcessPath(DWORD processID);
bool CaseInsensitiveCompare(const wstring& str1, const wstring& str2);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 防止检测程序自身多开
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"本检测程序已经在运行中！", L"提示", MB_ICONINFORMATION);
            return 0;
    }

    // 解析命令行参数
    int argc;
    wchar_t** argv = CommandLineToArgvW(pCmdLine, &argc);

    if (argc < 1) {
        MessageBoxW(NULL, L"使用方法: ProcessGuardian.exe <程序路径>", L"参数错误", MB_ICONERROR);
        ReleaseMutex(hMutex);
        return 1;
    }

    wstring targetExe = argv[0];

    // 校验文件是否存在
    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wstring msg = L"目标文件不存在:\n" + targetExe;
        MessageBoxW(NULL, msg.c_str(), L"错误", MB_ICONERROR);
        ReleaseMutex(hMutex);
        return 2;
    }

    // 路径标准化处理
    transform(targetExe.begin(), targetExe.end(), targetExe.begin(), ::towlower);

    // 检测进程状态
    if (IsProcessRunning(targetExe)) {
        MessageBoxW(NULL, L"目标程序已经在运行中！", L"提示", MB_ICONINFORMATION);
    }
    else {
        if (!StartProcess(targetExe)) {
            DWORD err = GetLastError();
            wstring msg = L"启动程序失败！错误代码: " + to_wstring(err);
            MessageBoxW(NULL, msg.c_str(), L"错误", MB_ICONERROR);
        }
    }

    ReleaseMutex(hMutex);
    LocalFree(argv);
    return 0;
}

// 检测指定路径的程序是否正在运行
bool IsProcessRunning(const wstring& exePath) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe)) {
        CloseHandle(hSnapshot);
        return false;
    }

    bool found = false;
    do {
        wstring processPath = GetProcessPath(pe.th32ProcessID);
        if (!processPath.empty()) {
            transform(processPath.begin(), processPath.end(), processPath.begin(), ::towlower);
            if (CaseInsensitiveCompare(processPath, exePath)) {
                found = true;
                break;
            }
        }
    } while (Process32NextW(hSnapshot, &pe));

    CloseHandle(hSnapshot);
    return found;
}

// 启动指定程序
bool StartProcess(const wstring& exePath) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;

    // 构造命令行（处理空格路径）
    wstring commandLine = L"\"" + exePath + L"\"";

    return CreateProcessW(
        NULL,
        &commandLine[0],
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi
    );
}

// 获取进程完整路径
wstring GetProcessPath(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
    if (hProcess == NULL) return L"";

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return wstring(path);
    }

    CloseHandle(hProcess);
    return L"";
}

// 不区分大小写的路径比较
bool CaseInsensitiveCompare(const wstring& str1, const wstring& str2) {
    if (str1.length() != str2.length()) return false;
    return _wcsicmp(str1.c_str(), str2.c_str()) == 0;
}