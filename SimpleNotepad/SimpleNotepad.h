
// SimpleNotepad.h: SimpleNotepad 应用程序的主头文件
//
#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"       // 主符号


// CSimpleNotepadApp:
// 有关此类的实现，请参阅 SimpleNotepad.cpp
//

class CSimpleNotepadApp : public CWinAppEx
{
public:
	CSimpleNotepadApp() noexcept;

	CArray<HWND, HWND> m_aryFrames;
public:

// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 实现
protected:
	HMENU  m_hMDIMenu;
	HACCEL m_hMDIAccel;

public:
	BOOL  m_bHiColorIcons;

	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();

	afx_msg void OnAppAbout();
	afx_msg void OnFileNewFrame();
	DECLARE_MESSAGE_MAP()
};

extern CSimpleNotepadApp theApp;
