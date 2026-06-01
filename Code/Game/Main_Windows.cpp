#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in VERY few places (and .CPPs only)
#include <Engine/Core/EngineCommon.h>
#include <math.h>
#include <cassert>
#include <crtdbg.h>
#include "App.h"
#include "Engine/Input/InputSystem.h"

extern HDC g_displayDeviceContext;
extern App* g_theApp;				// Created and owned by Main_Windows.cpp

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	UNUSED(commandLineString);
	UNUSED(applicationInstanceHandle);

	g_theApp = new App();
	g_theApp->Startup();

	// Program main loop; keep running frames until it's time to quit
	g_theApp->RunMainLoop();

	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}


