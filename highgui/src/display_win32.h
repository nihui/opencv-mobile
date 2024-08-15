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

#pragma once

#include <Windows.h>

class SimpleWindow
{
protected:
	HWND m_hWnd;
	virtual LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam);

public:
	friend static LRESULT CALLBACK __globalWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static double getDesktopDpiFactor();

	// NOTE: a SimpleWindow1 object which is already attached to an HWND should NOT be copied; use pointers instead
	SimpleWindow(const SimpleWindow&) = delete;
	SimpleWindow& operator=(const SimpleWindow&) = delete;

	SimpleWindow();
	SimpleWindow(SimpleWindow&& right) noexcept;
	SimpleWindow& operator=(SimpleWindow&& right) noexcept;

	HWND getHwnd() const;
	bool create(
		LPCSTR lpWindowName,
		DWORD dwStyle,
		int X = CW_USEDEFAULT,
		int Y = CW_USEDEFAULT,
		int nWidth = CW_USEDEFAULT,
		int nHeight = CW_USEDEFAULT,
		HWND hWndParent = nullptr,
		DWORD dwExStyle = 0,
		HMENU hMenu = nullptr,
		LPVOID lpParam = nullptr
	);
	void centerWindow() const;

	void show(int nCmdShow);

#if 0
	// starts message loop ; blocks the thread calling this
	// NOT ACTUALLY USED TILL NOW
	int messageLoop();
#endif

	void detach();
	virtual ~SimpleWindow();
};

// 防止那些你用不到的基类函数干扰你，我直接以protected方式继承
class BitmapWindow : protected SimpleWindow
{
protected:
	BYTE* m_bits;
	BITMAPINFO m_bmpInfo;
	int m_xSrc, m_ySrc;
	// doubled-buffered painting
	// NOTE: this is useful in some cases; don't delete these code
	HDC m_hMemDC;
	HBITMAP m_hMemBitmap;
	HGDIOBJ m_hOldBitmap;

	static constexpr int waitKeyTimerId = 150;
	static constexpr int WM_WAITKEYTIMEOUT = WM_USER + 150;

	// only finds identical-named windows in current process
	static BitmapWindow* findOCVMWindowByName(LPCSTR windowName);
	void drawBitmap(int horz = -1, int vert = -1);
	virtual LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	void updateBitmapData(const BYTE* bmpFileData);
	void updateScrollBarStatus();

public:
	using SimpleWindow::SimpleWindow;
	BitmapWindow();
	virtual ~BitmapWindow();
	using SimpleWindow::getHwnd;

	static void show(LPCSTR windowName, const BYTE* bmpFileData);
	// NOTE: only manages the windows created by current thread
	static int waitKey(UINT delay = 0);
};
