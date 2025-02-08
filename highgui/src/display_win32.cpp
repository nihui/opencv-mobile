//
// Copyright (C) 2024 szx0427&futz12
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// IMSHOW window for MS Windows
#include "display_win32.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <list>
#include <mutex>

static constexpr auto classNameBase = "OCVMWndClass";
static char className[100]; // real class name for current process.
                            // Use this process-specific name to ensure that picture replacement for identical-named windows
                            // only works for windows in the same process. The windows in other processes,
							// even if they have the same name, are ignored by BitmapWindow::show().
                            // That's to say, new windows with the same name will be created.

static std::map<HWND, SimpleWindow*> g_wndMap;
static std::recursive_mutex g_creationMutex, g_wndMapMutex;
static SimpleWindow* g_wndBeingCreated{};
static ATOM g_ocvmWndClassAtom{};
thread_local std::list<HWND> th_ocvmBmpWndList;

typedef std::lock_guard<std::recursive_mutex> defLockGuard;

template<class T>
static inline void safeReleaseArray(T* &array) {
	delete[] array;
	array = nullptr;
}

LRESULT CALLBACK __globalWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_wndBeingCreated) {
		g_wndBeingCreated->m_hWnd = hWnd;
		return g_wndBeingCreated->windowProc(msg, wParam, lParam);
	}

	defLockGuard mapLock(g_wndMapMutex);
	auto it = g_wndMap.find(hWnd);
	if (it != g_wndMap.end()) {
		return it->second->windowProc(msg, wParam, lParam);
	}
	else {
		fprintf(stderr, "[WARNING] display_win32: __globalWndProc: Window %p not found\n", hWnd);
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}

SimpleWindow::SimpleWindow()
	: m_hWnd(nullptr)
{
}

SimpleWindow::SimpleWindow(SimpleWindow&& right) noexcept
{
	*this = std::move(right);
}

SimpleWindow& SimpleWindow::operator=(SimpleWindow&& right) noexcept
{
	detach();
	m_hWnd = right.m_hWnd;

	if (m_hWnd) {
		defLockGuard mapLock(g_wndMapMutex);
		auto it = g_wndMap.find(m_hWnd);
		if (it != g_wndMap.end()) {
			it->second = this;
		}
		else {
			fprintf(stderr, "[WARNING] display_win32: move assignment: Window %p not found\n", m_hWnd);
		}

		right.m_hWnd = nullptr;
	}

	return *this;
}

HWND SimpleWindow::getHwnd() const
{
	return m_hWnd;
}

bool SimpleWindow::create(LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, DWORD dwExStyle, HMENU hMenu, LPVOID lpParam)
{
	assert(!m_hWnd);

	auto hInst = GetModuleHandle(nullptr);
	defLockGuard creationLock(g_creationMutex); // use mutex to ensure thread security

	if (!g_ocvmWndClassAtom) {
		sprintf(className, "%s:%d", classNameBase, static_cast<int>(GetCurrentProcessId()));

		WNDCLASSA wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = __globalWndProc;
		wc.hInstance = hInst;
		wc.lpszClassName = className;
		wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
		g_ocvmWndClassAtom = RegisterClassA(&wc);
	}

	g_wndBeingCreated = this; // storage "this" temporarily to g_wndBeingCreated, which will soon be used by __globalWndProc to process WM_CREATE.
	m_hWnd = CreateWindowExA(dwExStyle, className, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInst, lpParam);
	g_wndBeingCreated = nullptr;

	if (m_hWnd) {
		defLockGuard mapLock(g_wndMapMutex);
		g_wndMap.emplace(m_hWnd, this);
		return true;
	}
	return false;
}

void SimpleWindow::centerWindow() const
{
	assert(m_hWnd);
	RECT rc;
	GetWindowRect(m_hWnd, &rc);
	int x = (GetSystemMetrics(SM_CXFULLSCREEN) - (rc.right - rc.left)) / 2;
	int y = (GetSystemMetrics(SM_CYFULLSCREEN) - (rc.bottom - rc.top)) / 2;
	SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void SimpleWindow::show(int nCmdShow)
{
	assert(m_hWnd);

	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);
}

#if 0
int SimpleWindow::messageLoop()
{
	MSG msg;
	while (GetMessageA(&msg, getHwnd(), 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return (int)msg.wParam;
}
#endif

LRESULT SimpleWindow::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto ret = DefWindowProcA(m_hWnd, msg, wParam, lParam);
	if (msg == WM_NCDESTROY) {
		detach();
	}
	return ret;
}

double SimpleWindow::getDesktopDpiFactor()
{
	HDC hDC = GetDC(nullptr);
	double ret = GetDeviceCaps(hDC, LOGPIXELSX) / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
	ReleaseDC(nullptr, hDC);
	return ret;
}

void SimpleWindow::detach()
{
	if (m_hWnd) {
		defLockGuard mapLock(g_wndMapMutex);
		g_wndMap.erase(m_hWnd);
		m_hWnd = nullptr;
	}
}

SimpleWindow::~SimpleWindow()
{
	// NOTE: Directly remove mapping when one SimpleWindow object is being destructed.
	//       Errors will occur if multiple SimpleWindow objects are attached to one HWND.
	detach();
}

BitmapWindow* BitmapWindow::findOCVMWindowByName(LPCSTR windowName)
{
	HWND hWnd = FindWindowA((LPCSTR)(intptr_t)g_ocvmWndClassAtom, windowName);
	if (hWnd) {
		defLockGuard mapLock(g_wndMapMutex);
		auto it = g_wndMap.find(hWnd);
		if (it != g_wndMap.end()) {
			return reinterpret_cast<BitmapWindow*>(it->second);
		}
	}
	return nullptr;
}

void BitmapWindow::drawBitmap(int horz, int vert)
{
	if (horz < 0) {
		horz = GetScrollPos(m_hWnd, SB_HORZ);
	}
	if (vert < 0) {
		vert = GetScrollPos(m_hWnd, SB_VERT);
	}

	RECT rc;
	GetClientRect(m_hWnd, &rc);
	if (m_bmpInfo.bmiHeader.biHeight < 0) {
		m_xSrc = horz;
		m_ySrc = vert;
	}
	else {
		m_xSrc = horz;
		m_ySrc = m_bmpInfo.bmiHeader.biHeight - vert - rc.bottom;
	}

	InvalidateRect(m_hWnd, nullptr, FALSE);
}

int BitmapWindow::waitKey(UINT delay)
{
	if (th_ocvmBmpWndList.size() == 0) {
		return -1;
	}

	if (delay > 0) {
		for (auto wnd : th_ocvmBmpWndList) {
			SetTimer(wnd, waitKeyTimerId, delay, nullptr);
		}
	}

	MSG msg;
	int asciiCode = -1;
	while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
		if (msg.hwnd) {
			if (msg.message == WM_WAITKEYTIMEOUT) {
				for (auto wnd : th_ocvmBmpWndList) {
					KillTimer(wnd, waitKeyTimerId);
				}
				// As this thread may have created more than one OCVMWindow,
				// we need to clean up remaining messages in the queue
				// to avoid receiving many WM_WAITKEYTIMEOUT as soon as
				// waitKey() is called next time.
				while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE));
				return -1;
			}
			if (msg.message == WM_CHAR) {
				asciiCode = static_cast<int>(msg.wParam);
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	return asciiCode;
}

BitmapWindow::BitmapWindow()
	: m_xSrc(0)
	, m_ySrc(0)
	, m_bits(nullptr)
{
	ZeroMemory(&m_bmpInfo, sizeof(m_bmpInfo));

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	HDC hDesktopDC = GetDC(nullptr);
	m_hMemDC = CreateCompatibleDC(hDesktopDC);
	m_hMemBitmap = CreateCompatibleBitmap(hDesktopDC, screenWidth, screenHeight);
	m_hOldBitmap = SelectObject(m_hMemDC, m_hMemBitmap);
	ReleaseDC(nullptr, hDesktopDC);
}

BitmapWindow::~BitmapWindow()
{
	if (m_bits) {
		safeReleaseArray(m_bits);
	}

	SelectObject(m_hMemDC, m_hOldBitmap);
	DeleteObject(m_hMemBitmap);
	DeleteDC(m_hMemDC);
}

void BitmapWindow::show(LPCSTR windowName, const BYTE* bmpFileData)
{
	auto formerWindow = findOCVMWindowByName(windowName);
	if (formerWindow) { // if a window with this name already exists, just update its image
		formerWindow->updateBitmapData(bmpFileData);
		RECT rc;
		GetWindowRect(formerWindow->getHwnd(), &rc);
		SetWindowPos(formerWindow->getHwnd(), nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOMOVE);
		formerWindow->updateScrollBarStatus();
		formerWindow->drawBitmap();
		return;
	}

	BitmapWindow* newWindow = new BitmapWindow;
	newWindow->updateBitmapData(bmpFileData);

	auto dpiFactor = getDesktopDpiFactor();
	newWindow->create(windowName, (WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL) & ~WS_MAXIMIZEBOX, 0, 0, 800 * dpiFactor, 600 * dpiFactor);
	newWindow->centerWindow();
	newWindow->drawBitmap();
	newWindow->updateScrollBarStatus();
	newWindow->SimpleWindow::show(SW_SHOWNORMAL);

	th_ocvmBmpWndList.push_back(newWindow->m_hWnd);
}

void BitmapWindow::updateBitmapData(const BYTE* bmpFileData)
{
	if (m_bits) {
		safeReleaseArray(m_bits);
	}
	BITMAPFILEHEADER* fileHeader = (BITMAPFILEHEADER*)bmpFileData;
	auto bitsSize = fileHeader->bfSize - fileHeader->bfOffBits;
	m_bits = new BYTE[bitsSize];
	memcpy(m_bits, bmpFileData + fileHeader->bfOffBits, bitsSize);
	ZeroMemory(&m_bmpInfo, sizeof(m_bmpInfo));
	memcpy(&m_bmpInfo.bmiHeader, bmpFileData + sizeof(BITMAPFILEHEADER), sizeof(BITMAPINFOHEADER));
}

void BitmapWindow::updateScrollBarStatus()
{
	RECT rc;
	GetClientRect(m_hWnd, &rc);
	int cx = rc.right;
	int cy = rc.bottom;
	if (m_bmpInfo.bmiHeader.biWidth > cx) {
		EnableScrollBar(m_hWnd, SB_HORZ, ESB_ENABLE_BOTH);
		SetScrollRange(m_hWnd, SB_HORZ, 0, m_bmpInfo.bmiHeader.biWidth - cx, TRUE);
	}
	else {
		SetScrollPos(m_hWnd, SB_HORZ, 0, FALSE);
		EnableScrollBar(m_hWnd, SB_HORZ, ESB_DISABLE_BOTH);
	}
	if (abs(m_bmpInfo.bmiHeader.biHeight) > cy) {
		EnableScrollBar(m_hWnd, SB_VERT, ESB_ENABLE_BOTH);
		SetScrollRange(m_hWnd, SB_VERT, 0, abs(m_bmpInfo.bmiHeader.biHeight) - cy, TRUE);
	}
	else {
		SetScrollPos(m_hWnd, SB_VERT, 0, FALSE);
		EnableScrollBar(m_hWnd, SB_VERT, ESB_DISABLE_BOTH);
	}
}

LRESULT BitmapWindow::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_TIMER:
		if (wParam == waitKeyTimerId) {
			PostMessageA(m_hWnd, WM_WAITKEYTIMEOUT, 0, 0);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hWnd, &ps);

		int cx = (std::min)(ps.rcPaint.right, m_bmpInfo.bmiHeader.biWidth);
		int cy = (std::min)(ps.rcPaint.bottom, m_bmpInfo.bmiHeader.biHeight);

		if (ps.rcPaint.right > m_bmpInfo.bmiHeader.biWidth || ps.rcPaint.bottom > m_bmpInfo.bmiHeader.biHeight) {
			HBRUSH hbrBkgnd = CreateSolidBrush(RGB(240, 240, 240));
			FillRect(m_hMemDC, &ps.rcPaint, hbrBkgnd);
			DeleteObject(hbrBkgnd);
			StretchDIBits(m_hMemDC, 0, 0, cx, cy, m_xSrc, m_ySrc, cx, cy, m_bits, &m_bmpInfo, DIB_RGB_COLORS, SRCCOPY);
			BitBlt(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, m_hMemDC, 0, 0, SRCCOPY);
		}
		else {
			StretchDIBits(hdc, 0, 0, cx, cy, m_xSrc, m_ySrc, cx, cy, m_bits, &m_bmpInfo, DIB_RGB_COLORS, SRCCOPY);
		}

		EndPaint(m_hWnd, &ps);
		return 0;
	}
	case WM_HSCROLL: {
		int pos = HIWORD(wParam);
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
			drawBitmap(pos);
			break;
		case SB_THUMBPOSITION:
			SetScrollPos(m_hWnd, SB_HORZ, pos, TRUE);
			break;
		case SB_LINELEFT:
			SetScrollPos(m_hWnd, SB_HORZ, GetScrollPos(m_hWnd, SB_HORZ) - 1, TRUE);
			drawBitmap();
			break;
		case SB_LINERIGHT:
			SetScrollPos(m_hWnd, SB_HORZ, GetScrollPos(m_hWnd, SB_HORZ) + 1, TRUE);
			drawBitmap();
			break;
		default:
			break;
		}
		return 0;
	}
	case WM_VSCROLL: {
		int pos = HIWORD(wParam);
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
			drawBitmap(-1, pos);
			break;
		case SB_THUMBPOSITION:
			SetScrollPos(m_hWnd, SB_VERT, pos, TRUE);
			break;
		case SB_LINEDOWN:
			SetScrollPos(m_hWnd, SB_VERT, GetScrollPos(m_hWnd, SB_VERT) + 1, TRUE);
			drawBitmap();
			break;
		case SB_LINEUP:
			SetScrollPos(m_hWnd, SB_VERT, GetScrollPos(m_hWnd, SB_VERT) - 1, TRUE);
			drawBitmap();
			break;
		default:
			break;
		}
		return 0;
	}
	case WM_GETMINMAXINFO: {
		RECT rc;
		rc.left = rc.top = 0;
		rc.right = m_bmpInfo.bmiHeader.biWidth;
		rc.bottom = m_bmpInfo.bmiHeader.biHeight;
		AdjustWindowRect(&rc, GetWindowLongPtr(m_hWnd, GWL_STYLE), FALSE);
		int cxVertScroll = GetSystemMetrics(SM_CXVSCROLL);
		int cyHorzScroll = GetSystemMetrics(SM_CYHSCROLL);
		auto mmi = (LPMINMAXINFO)lParam;
		mmi->ptMaxTrackSize = { rc.right - rc.left + cxVertScroll,rc.bottom - rc.top + cyHorzScroll };
		return 0;
	}
	case WM_SIZE: {
		if (wParam == SIZE_RESTORED) {
			updateScrollBarStatus();
			drawBitmap();
			return 0;
		}
		break;
	}
	case WM_NCDESTROY:
		th_ocvmBmpWndList.remove(m_hWnd);
		if (th_ocvmBmpWndList.size() == 0) { // request to stop the message loop in waitKey()
			                    // when all windows owned by current thread have been closed
			PostQuitMessage(0);
		}
		SimpleWindow::windowProc(msg, wParam, lParam);
		delete this;
		return 0;
	default:
		break;
	}

	return SimpleWindow::windowProc(msg, wParam, lParam);
}
