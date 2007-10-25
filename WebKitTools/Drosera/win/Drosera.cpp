/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Drosera.h"

#include "DebuggerClient.h"
#include "DebuggerDocument.h"
#include "HelperFunctions.h"
#include "resource.h"
#include "ServerConnection.h"

#include <JavaScriptCore/JSStringRef.h>
#include <WebCore/IntRect.h>
#include <WebKit/IWebMutableURLRequest.h>
#include <WebKit/IWebView.h>
#include <WebKit/WebKit.h>

const unsigned MAX_LOADSTRING = 100;

TCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

static const LRESULT kNotHandledResult = -1;
static LPCTSTR kDroseraPointerProp = TEXT("DroseraPointer");
static HINSTANCE hInst;

ATOM registerDroseraClass(HINSTANCE hInstance);
LRESULT CALLBACK droseraWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK aboutWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

HINSTANCE Drosera::getInst() { return hInst; }
void Drosera::setInst(HINSTANCE in) { hInst = in; }


int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;

    Drosera drosera;

    HRESULT ret = drosera.init(hInstance, nCmdShow);
    if (FAILED(ret))
        return ret;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DROSERA));

    // Main message loop:
    while (GetMessage(&msg, 0, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}

////////////////// Setup Windows Specific Interface //////////////////

ATOM registerDroseraClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = ::droseraWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = sizeof(Drosera*);
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DROSERA));
    wcex.hCursor       = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName  = MAKEINTRESOURCE(IDC_DROSERA);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

//Processes messages for the main window.
LRESULT CALLBACK droseraWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    Drosera* drosera = reinterpret_cast<Drosera*>(longPtr);

    switch (message) {
        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            switch (wmId) {
                case ID_HELP_ABOUT:
                    DialogBox(Drosera::getInst(), MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, ::aboutWndProc);
                    break;
                case ID_FILE_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_SIZE:
            if (!drosera)
                return 0;
            return drosera->webViewLoaded() ? drosera->onSize(wParam, lParam) : 0;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK aboutWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

////////////////// End Setup Windows Specific Interface //////////////////

Drosera::Drosera()
    : m_hWnd(0)
    , m_debuggerClient(new DebuggerClient())
{
    OleInitialize(0);
}

HRESULT Drosera::init(HINSTANCE hInstance, int nCmdShow)
{
    HRESULT ret = initUI(hInstance, nCmdShow);
    if (FAILED(ret))
        return ret;

    ret = attach();
    return ret;
}


HRESULT Drosera::initUI(HINSTANCE hInstance, int nCmdShow)
{
    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));
    LoadString(hInstance, IDC_DROSERA, szWindowClass, ARRAYSIZE(szWindowClass));
    registerDroseraClass(hInstance);

    Drosera::setInst(hInstance); // Store instance handle in our local variable

    m_hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, 0);

    if (!m_hWnd)
        return HRESULT_FROM_WIN32(GetLastError());

    SetLastError(0);
    SetWindowLongPtr(m_hWnd, 0, reinterpret_cast<LONG_PTR>(this));
    HRESULT ret = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(ret))
        return ret;

    ret = CoCreateInstance(CLSID_WebView, 0, CLSCTX_ALL, IID_IWebView, (void**)&m_webView);
    if (FAILED(ret))
        return ret;

    m_webViewPrivate.query(m_webView.get());
    if (!m_webView)
        return E_FAIL;

    ret = m_webView->setHostWindow(reinterpret_cast<OLE_HANDLE>(m_hWnd));
    if (FAILED(ret))
        return ret;

    RECT rect = {0};
    ret = m_webView->initWithFrame(rect, 0, 0);
    if (FAILED(ret))
        return ret;

    HWND viewWindow;
    ret = m_webViewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow));
    if (FAILED(ret))
        return ret;

    ::SetProp(viewWindow, kDroseraPointerProp, (HANDLE)this);

    // FIXME: Implement window size/position save/restore

    RECT frame;
    frame.left = 60;
    frame.top = 200;
    frame.right = 750;
    frame.bottom = 550;
    ::SetWindowPos(m_hWnd, HWND_TOPMOST, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top, 0);
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    return ret;
}

LRESULT Drosera::onSize(WPARAM, LPARAM)
{
    if (!m_webViewPrivate)
        return 0;

    RECT clientRect = {0};
    ::GetClientRect(m_hWnd, &clientRect);

    HWND viewWindow;
    if (FAILED(m_webViewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        return 0;

    // FIXME should this be the height-command bars height?
    ::SetWindowPos(viewWindow, 0, clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, SWP_NOZORDER);
    return 0;
}

bool Drosera::webViewLoaded() const
{
    return m_debuggerClient->webViewLoaded();
}

// Server Detection Callbacks

HRESULT Drosera::attach()
{
    // Get selected server
    HRESULT ret = m_webView->setFrameLoadDelegate(m_debuggerClient.get());
    if (FAILED(ret))
        return ret;

    ret = m_webView->setUIDelegate(m_debuggerClient.get());

    COMPtr<IWebFrame> mainFrame;
    ret = m_webView->mainFrame(&mainFrame);
    if (FAILED(ret))
        return ret;

    COMPtr<IWebMutableURLRequest> request;
    ret = CoCreateInstance(CLSID_WebMutableURLRequest, 0, CLSCTX_ALL, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(ret))
        return ret;

    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("org.webkit.drosera")), CFSTR("debugger"), CFSTR("html"), CFSTR("Drosera")));
    if (!htmlURLRef)
        return E_FAIL;

    CFStringRef urlStringRef = ::CFURLGetString(htmlURLRef.get());
    BSTR tempStr = cfStringToBSTR(urlStringRef);    // Both initWithRUL and SysFreeString can handle 0.
    ret = request->initWithURL(tempStr, WebURLRequestUseProtocolCachePolicy, 60);
    SysFreeString(tempStr);
    if (FAILED(ret))
        return ret;

    ret = mainFrame->loadRequest(request.get());
    if (FAILED(ret))
        return ret;

    return ret;
}
