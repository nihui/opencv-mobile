// Created by szx0427
// Patch by futz12
// on 2024/08/07

#pragma once

#ifdef _WIN32
#include <windows.h>
#include <map>
#include <string>

class SimpleWindow
{
protected:
	// cannot be modified externally
	HWND m_hwnd = nullptr;

public:
	friend LRESULT CALLBACK __globalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// NOTE: a SimpleWindow object which is already attached to an HWND should NOT be copied; use pointers instead
	SimpleWindow(const SimpleWindow &) = delete;
	SimpleWindow &operator=(const SimpleWindow &) = delete;

	SimpleWindow();
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
	// shows window & starts message loop; blocks the thread
	int doModal();
	void centerWindow() const;
	// DefWindowProc
	virtual LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual ~SimpleWindow();
};

// 防止那些你用不到的基类函数干扰你，我直接以protected方式继承
class BitmapWindow : protected SimpleWindow
{
private:
	virtual LRESULT windowProc(UINT msg, WPARAM wParam, LPARAM lParam);
	int m_xSrc, m_ySrc; // internally managed by drawBitmap
protected:
	const void *m_bits;
	BITMAPINFO m_bi;
	void drawBitmap(int horz = -1, int vert = -1);
public:
	using SimpleWindow::getHwnd;
	explicit BitmapWindow(const void *bmpFileData);
	void show(LPCSTR title);
};

// only returns horizontal scale factor (generally equals the vertical one which is ignored)
float getDpiFactor();

#endif