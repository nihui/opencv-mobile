// Created by szx0427
// on 2024/03/08

#ifdef _WIN32
#include "bmpWnd.h"
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <mutex>

constexpr auto *className = "SZXwndclass";

static std::map<HWND, SimpleWindow *> g_wndMap;
static std::mutex g_creationMutex;
static SimpleWindow *g_wndBeingCreated{};

LRESULT CALLBACK __globalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_wndBeingCreated) {
		g_wndBeingCreated->m_hwnd = hwnd;
		return g_wndBeingCreated->windowProc(msg, wParam, lParam);
	}
	return g_wndMap.at(hwnd)->windowProc(msg, wParam, lParam);
}

SimpleWindow::SimpleWindow()
	: m_hwnd(nullptr)
{
}

HWND SimpleWindow::getHwnd() const
{
	return m_hwnd;
}

bool SimpleWindow::create(LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, DWORD dwExStyle, HMENU hMenu, LPVOID lpParam)
{
	assert(!m_hwnd);
	
	auto hInst = GetModuleHandle(nullptr);

	WNDCLASSA wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = __globalWndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = className;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassA(&wc);

	g_creationMutex.lock(); // use mutex to ensure thread security
	g_wndBeingCreated = this; // storage "this" temporarily to g_wndBeingCreated, which will soon be used by __globalWndProc to process WM_CREATE.
	m_hwnd = CreateWindowExA(dwExStyle, className, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInst, lpParam);
	g_wndBeingCreated = nullptr;
	g_creationMutex.unlock();

	if (m_hwnd) {
		g_wndMap.emplace(m_hwnd, this);
		return true;
	}
	return false;
}

int SimpleWindow::doModal()
{
	assert(m_hwnd);
	ShowWindow(m_hwnd, SW_SHOWNORMAL);
	MSG msg;
	while (GetMessageA(&msg, m_hwnd, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	g_wndMap.erase(m_hwnd);
	m_hwnd = nullptr;
	return (int)msg.wParam;
}

void SimpleWindow::centerWindow() const
{
	assert(m_hwnd);
	RECT rc;
	GetWindowRect(m_hwnd, &rc);
	int x = (GetSystemMetrics(SM_CXFULLSCREEN) - (rc.right - rc.left)) / 2;
	int y = (GetSystemMetrics(SM_CYFULLSCREEN) - (rc.bottom - rc.top)) / 2;
	SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT SimpleWindow::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcA(m_hwnd, msg, wParam, lParam);
}

SimpleWindow::~SimpleWindow()
{
	if (m_hwnd) {
		// NOTE: Directly remove mapping when one SimpleWindow object is being destructed.
		//       Errors will occur if multiple SimpleWindow objects are attached to one HWND.
		g_wndMap.erase(m_hwnd);
	}
}

LRESULT BitmapWindow::windowProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hwnd, &ps);
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, m_xSrc, m_ySrc, rc.right, rc.bottom, m_bits, &m_bi, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(m_hwnd, &ps);
		return 0;
	}
	case WM_HSCROLL: {
		int pos = HIWORD(wParam);
		switch (LOWORD(wParam)) {
		case SB_THUMBTRACK:
			drawBitmap(pos);
			break;
		case SB_THUMBPOSITION:
			SetScrollPos(m_hwnd, SB_HORZ, pos, TRUE);
			break;
		case SB_LINELEFT:
			SetScrollPos(m_hwnd, SB_HORZ, GetScrollPos(m_hwnd, SB_HORZ) - 1, TRUE);
			drawBitmap();
			break;
		case SB_LINERIGHT:
			SetScrollPos(m_hwnd, SB_HORZ, GetScrollPos(m_hwnd, SB_HORZ) + 1, TRUE);
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
			SetScrollPos(m_hwnd, SB_VERT, pos, TRUE);
			break;
		case SB_LINEDOWN:
			SetScrollPos(m_hwnd, SB_VERT, GetScrollPos(m_hwnd, SB_VERT) + 1, TRUE);
			drawBitmap();
			break;
		case SB_LINEUP:
			SetScrollPos(m_hwnd, SB_VERT, GetScrollPos(m_hwnd, SB_VERT) - 1, TRUE);
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
		rc.right = m_bi.bmiHeader.biWidth;
		rc.bottom = m_bi.bmiHeader.biHeight;
		AdjustWindowRect(&rc, GetWindowLongPtr(m_hwnd, GWL_STYLE), FALSE);
		int cxVertScroll = GetSystemMetrics(SM_CXVSCROLL);
		int cyHorzScroll = GetSystemMetrics(SM_CYHSCROLL);
		auto mmi = (LPMINMAXINFO)lParam;
		mmi->ptMaxTrackSize = { rc.right - rc.left + cxVertScroll,rc.bottom - rc.top + cyHorzScroll };
		return 0;
	}
	case WM_SIZE: {
		if (wParam == SIZE_RESTORED) {
			int cx = LOWORD(lParam);
			int cy = HIWORD(lParam);
			if (m_bi.bmiHeader.biWidth > cx) {
				EnableScrollBar(m_hwnd, SB_HORZ, ESB_ENABLE_BOTH);
				SetScrollRange(m_hwnd, SB_HORZ, 0, m_bi.bmiHeader.biWidth - cx, TRUE);
			}
			else {
				SetScrollPos(m_hwnd, SB_HORZ, 0, FALSE);
				EnableScrollBar(m_hwnd, SB_HORZ, ESB_DISABLE_BOTH);
			}
			if (abs(m_bi.bmiHeader.biHeight) > cy) {
				EnableScrollBar(m_hwnd, SB_VERT, ESB_ENABLE_BOTH);
				SetScrollRange(m_hwnd, SB_VERT, 0, abs(m_bi.bmiHeader.biHeight) - cy, TRUE);
			}
			else {
				SetScrollPos(m_hwnd, SB_VERT, 0, FALSE);
				EnableScrollBar(m_hwnd, SB_VERT, ESB_DISABLE_BOTH);
			}
			drawBitmap();
			return 0;
		}
		break;
	}
	default:
		break;
	}

	return SimpleWindow::windowProc(msg, wParam, lParam);
}

void BitmapWindow::drawBitmap(int horz, int vert)
{
	if (horz < 0) {
		horz = GetScrollPos(m_hwnd, SB_HORZ);
	}
	if (vert < 0) {
		vert = GetScrollPos(m_hwnd, SB_VERT);
	}
	//std::cout << horz << " " << vert << "\n";
	RECT rc;
	GetClientRect(m_hwnd, &rc);
	if (m_bi.bmiHeader.biHeight < 0) {
		m_xSrc = horz;
		m_ySrc = vert;
	}
	else {
		m_xSrc = horz;
		m_ySrc = m_bi.bmiHeader.biHeight - vert - rc.bottom;
	}
	InvalidateRect(m_hwnd, nullptr, FALSE);
}

BitmapWindow::BitmapWindow(const void *bmpFileData)
	: m_bits((uint8_t *)bmpFileData + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
	, m_xSrc(0)
	, m_ySrc(0)
{
	ZeroMemory(&m_bi, sizeof(m_bi));
	memcpy(&m_bi.bmiHeader, ((uint8_t *)bmpFileData + sizeof(BITMAPFILEHEADER)), sizeof(BITMAPINFOHEADER));
}

void BitmapWindow::show(LPCSTR title)
{
	float scaleFactor = getDpiFactor();
	if (!create(title, (WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL) & ~WS_MAXIMIZEBOX, 0, 0, 800 * scaleFactor, 640 * scaleFactor)) {
		// 窗口创建失败了
		std::cerr << "ERROR: Bitmap window creation failed\n";
		return;
	}
	centerWindow();
	doModal();
}

float getDpiFactor()
{
	HDC hdc = GetDC(nullptr);
	float factor = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0;
	ReleaseDC(nullptr, hdc);
	return factor;
}

#endif // _WIN32
