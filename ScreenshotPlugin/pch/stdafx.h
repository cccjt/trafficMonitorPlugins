// stdafx.h: 标准系统包含文件的包含文件
#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// MFC
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <afxwin.h>         // MFC 核心和标准组件
#include <afxext.h>         // MFC 扩展
#include <afxdtctl.h>       // MFC IE 公共控件支持
#include <afxcmn.h>         // MFC 公共控件支持
#include <afxdlgs.h>        // MFC 对话框

#include <Windows.h>
#include <objbase.h>
#include <windowsx.h>   // GET_X_LPARAM, GET_Y_LPARAM

// STL
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <random>
#include <cmath>
#include <sstream>

// GDI+
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// 链接器
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
// windowsapp.lib 在阶段7实现 OCR 时再启用
// #pragma comment(lib, "windowsapp.lib")
