#include "NetJetController.h"
#include <windows.h>

HINSTANCE originalNetJetController;
HINSTANCE originalXbox360Controller;
NetJetSimulator netJetSimulator;








LRESULT __stdcall NetJetSimulator::Keyboard::backgroundThread(int nCode, WPARAM wParam, LPARAM lParam) {
	// event handler for when keyboard does something
	if (lParam && nCode >= 0) {
		// if onKeyDown or onKeyUp
		if (wParam == WM_KEYDOWN || wParam == WM_KEYUP) {
			// get reference to the system's keyboard
			netJetSimulator.keyboard.keyboardDLLHookStruct = *(KBDLLHOOKSTRUCT*)lParam;

			for (size_t i = 0;i < KEYS_SIZE;i++) {
				// if the keyCode matches one of our keycodes
				if (netJetSimulator.keyboard.keyCodes[i] == netJetSimulator.keyboard.keyboardDLLHookStruct.vkCode) {
					// reflect this in our keysDown array
					netJetSimulator.keyboard.keysDown[i] = (wParam == WM_KEYDOWN);
					break;
				}
			}
		}
	}
	// continue watching the keyboard
	return CallNextHookEx(netJetSimulator.keyboard.backgroundThreadHook, nCode, wParam, lParam);
}

inline void NetJetSimulator::fixThumbstick(PDWORD thumbRX, PDWORD thumbRY) {
	if (thumbRX) {
		if (*thumbRX < 0) {
			*thumbRX = 0;
		}

		if (*thumbRX > 64) {
			*thumbRX = 64;
		}
	}

	if (thumbRY) {
		if (*thumbRY < 0) {
			*thumbRY = 0;
		}

		if (*thumbRY > 64) {
			*thumbRY = 64;
		}
	}
}

inline void NetJetSimulator::centerThumbstick(PDWORD thumbRX, PDWORD thumbRY, bool thumbstickDown = false) {
	// centre the thumbstick
	// considering the controller was apparently not inserted
	const DWORD THUMBSTICK_CENTER = 0x0000001F;

	if (thumbRX) {
		setState(thumbRX, thumbstickDown, THUMBSTICK_CENTER, true);
	}

	if (thumbRY) {
		setState(thumbRY, thumbstickDown, THUMBSTICK_CENTER, true);
	}
}

inline void NetJetSimulator::setState(PDWORD a, bool down, DWORD mapping, bool replace = false) {
	// if the key/button is down
	if (a && down) {
		if (replace) {
			// set our mapping, ignoring previous value (i.e. don't go faster when both arrow keys and DPad is pressed etc.)
			// don't use this on buttons, it will have the effect of clearing all other buttons
			*a = mapping;
		} else {
			// set our mapping
			*a |= mapping;
		}
	}
}

inline void NetJetSimulator::setControllerInserted(PDWORD buttons, PDWORD thumbRX, PDWORD thumbRY, BOOL originalResult = TRUE, bool replace = false, bool thumbstickDown = false) {
	if (buttons) {
		bool down = (originalResult || (*buttons & 0x00010000) != 0x00010000);
		// replace previous value
		// considering the controller is apparently not inserted and it could have been anything
		setState(buttons, down, 0x00010000, replace);
	}

	centerThumbstick(thumbRX, thumbRY, thumbstickDown);
}

inline void NetJetSimulator::setCartridgeInserted(PDWORD buttons, bool replace = false) {
	if (buttons) {
		bool down = (*buttons & 0x00100080) != 0x00100080;
		setState(buttons, down, 0x00100080, replace);
	}
}

void NetJetSimulator::callNetJetControllerGetState(PDWORD buttons, PDWORD thumbRX, PDWORD thumbRY, BOOL originalResult) {
	// call intercepted
	//POINT curpos;
	//int myWidth = GetSystemMetrics(SM_CXSCREEN);
	//int myHeight = GetSystemMetrics(SM_CYSCREEN);

	// ensure the game thinks a controller is inserted
	setControllerInserted(buttons, thumbRX, thumbRY, originalResult, true, true);
	// ensure the game thinks a cartridge is inserted and valid
	setCartridgeInserted(buttons);

	// if the controller isn't suspended...
	if (!suspended) {
		// ignore 360 Controller if user doesn't have XInput
		if (originalXbox360Controller) {
			_XInputGetState originalXInputGetState;
			XINPUT_STATE xbox360ControllerState;
			DWORD xbox360ControllerInserted;

			// get the 360 Controller state
			ZeroMemory(&xbox360ControllerState, sizeof(XINPUT_STATE));
			originalXInputGetState = (_XInputGetState)GetProcAddress(originalXbox360Controller, "XInputGetState");

			if (originalXInputGetState) {
				xbox360ControllerInserted = originalXInputGetState(0, &xbox360ControllerState);
			} else {
				// do this here to avoid out of date values
				xbox360ControllerInserted = 1167L;
			}

			// if the 360 Controller is inserted
			if (xbox360ControllerInserted == 0L) {
				// set currently pressed buttons as down on NetJet Controller
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x8000, 0x00000001);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x2000, 0x00000002);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x4000, 0x00000004);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x1000, 0x00000008);
				setState(buttons, xbox360ControllerState.Gamepad.bLeftTrigger  > 0x0000, 0x00000010);
				setState(buttons, xbox360ControllerState.Gamepad.bRightTrigger > 0x0000, 0x00000020);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0100, 0x00000010);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0200, 0x00000020);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0010, 0x00000100);
				setState(buttons, xbox360ControllerState.Gamepad.sThumbLY      >   7849, 0x00000200);
				setState(buttons, xbox360ControllerState.Gamepad.sThumbLY      <  -7849, 0x00000400);
				setState(buttons, xbox360ControllerState.Gamepad.sThumbLX      <  -7849, 0x00000800);
				setState(buttons, xbox360ControllerState.Gamepad.sThumbLX      >   7849, 0x00001000);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0001, 0x00000200);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0002, 0x00000400);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0004, 0x00000800);
				setState(buttons, xbox360ControllerState.Gamepad.wButtons      & 0x0008, 0x00001000);

				setState(thumbRX, xbox360ControllerState.Gamepad.sThumbRX < -8689 || xbox360ControllerState.Gamepad.sThumbRX > 8689, ( xbox360ControllerState.Gamepad.sThumbRX / 65536.0 + 0.5) * 64.0, true);
				setState(thumbRY, xbox360ControllerState.Gamepad.sThumbRY < -8689 || xbox360ControllerState.Gamepad.sThumbRY > 8689, (-xbox360ControllerState.Gamepad.sThumbRY / 65536.0 + 0.5) * 64.0, true);
			}
		}

		// if the game isn't already key mapping for us...
		if (!keyMapping) {
			// set currently pressed keys as down on NetJet Controller
			// Button 4
			setState(buttons, netJetSimulator.keyboard.keysDown[15], 0x00000001);
			setState(buttons, netJetSimulator.keyboard.keysDown[0] , 0x00000001);
			// Button 2
			setState(buttons, netJetSimulator.keyboard.keysDown[18], 0x00000002);
			setState(buttons, netJetSimulator.keyboard.keysDown[16], 0x00000002);
			setState(buttons, netJetSimulator.keyboard.keysDown[1] , 0x00000002);
			// Button 3
			setState(buttons, netJetSimulator.keyboard.keysDown[17], 0x00000004);
			setState(buttons, netJetSimulator.keyboard.keysDown[2] , 0x00000004);
			// Button 1
			setState(buttons, netJetSimulator.keyboard.keysDown[20], 0x00000008);
			setState(buttons, netJetSimulator.keyboard.keysDown[19], 0x00000008);
			setState(buttons, netJetSimulator.keyboard.keysDown[3] , 0x00000008);
			// Left Shoulder
			setState(buttons, netJetSimulator.keyboard.keysDown[21], 0x00000010);
			setState(buttons, netJetSimulator.keyboard.keysDown[4] , 0x00000010);
			// Right Shoulder
			setState(buttons, netJetSimulator.keyboard.keysDown[22], 0x00000020);
			setState(buttons, netJetSimulator.keyboard.keysDown[5] , 0x00000020);
			// Start
			setState(buttons, netJetSimulator.keyboard.keysDown[23], 0x00000100);
			setState(buttons, netJetSimulator.keyboard.keysDown[6] , 0x00000100);
			// DPad Up
			setState(buttons, netJetSimulator.keyboard.keysDown[7] , 0x00000200);
			// DPad Down
			setState(buttons, netJetSimulator.keyboard.keysDown[8] , 0x00000400);
			// DPad Left
			setState(buttons, netJetSimulator.keyboard.keysDown[9] , 0x00000800);
			// DPad Right
			setState(buttons, netJetSimulator.keyboard.keysDown[10], 0x00001000);
			// Right Thumbstick
			setState(thumbRY, netJetSimulator.keyboard.keysDown[11], 0x00000000, true);
			setState(thumbRX, netJetSimulator.keyboard.keysDown[12], 0x00000000, true);
			setState(thumbRY, netJetSimulator.keyboard.keysDown[13], 0x00000040, true);
			setState(thumbRX, netJetSimulator.keyboard.keysDown[14], 0x00000040, true);
		}

		// ignore mouse mapping because game conflicts with it
		/*
		if (!mousemapping) {
			if (GetCursorPos(&curpos)) {
				if (myWidth > 0 && (curpos.x - (myWidth / 2)) != 0) {
					*thumbRX = 31 + ((curpos.x - (myWidth / 2)) / (double)myWidth * 31);
				}
				if (myHeight > 0 && (curpos.y - (myHeight / 2)) != 0) {
					*thumbRY = 31 + ((curpos.y - (myHeight / 2)) / (double)myHeight * 31);
				}
			}
			SetCursorPos(myWidth / 2, myHeight / 2);
		}
		*/
	}

	// ensure thumbsticks are not further up/down/left/right than they can be on the controller
	fixThumbstick(thumbRX, thumbRY);
	// ensure the game thinks a controller is inserted and a cartridge is inserted and valid one more time just in case
	setControllerInserted(buttons, thumbRX, thumbRY);
	setCartridgeInserted(buttons);
}

void NetJetSimulator::callNetJetControllerGetKey(PVOID key) {
	// call intercepted
	// zero key, this is not a keygen
	// replicating real keys is outside the scope of this project
	if (key) {
		ZeroMemory(key, 0x20);
	}
}








BOOL WINAPI DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hInst);
		originalNetJetController = LoadLibraryA("NetJetController_orig.DLL");

		if (!originalNetJetController) {
			return FALSE;
		}

		netJetSimulator.keyboard.backgroundThreadHook = SetWindowsHookEx(WH_KEYBOARD_LL, netJetSimulator.keyboard.backgroundThread, NULL, 0);
		ShowCursor(FALSE);
		break;
		case DLL_PROCESS_DETACH:
		UnhookWindowsHookEx(netJetSimulator.keyboard.backgroundThreadHook);
		ShowCursor(TRUE);
		break;
	}
	return TRUE;
}

// duplicate functions which call the original and return false
// we always want these calls to appear successful
// returning false or zero means no error occured
typedef DWORD(*_NetJetControllerEnableKeyMapping)();
extern "C" DWORD callNetJetControllerEnableKeyMapping() {
	_NetJetControllerEnableKeyMapping originalNetJetControllerEnableKeyMapping;
	originalNetJetControllerEnableKeyMapping = (_NetJetControllerEnableKeyMapping)GetProcAddress(originalNetJetController, "NetJetControllerEnableKeyMapping");
	
	if (originalNetJetControllerEnableKeyMapping) {
		originalNetJetControllerEnableKeyMapping();
	}

	netJetSimulator.keyMapping = true;
	UnhookWindowsHookEx(netJetSimulator.keyboard.backgroundThreadHook);
	return 0;
}

typedef DWORD(*_NetJetControllerDisableKeyMapping)();
extern "C" DWORD callNetJetControllerDisableKeyMapping() {
	_NetJetControllerDisableKeyMapping originalNetJetControllerDisableKeyMapping;
	originalNetJetControllerDisableKeyMapping = (_NetJetControllerDisableKeyMapping)GetProcAddress(originalNetJetController, "NetJetControllerDisableKeyMapping");
	
	if (originalNetJetControllerDisableKeyMapping) {
		originalNetJetControllerDisableKeyMapping();
	}

	netJetSimulator.keyMapping = false;
	netJetSimulator.keyboard.backgroundThreadHook = SetWindowsHookEx(WH_KEYBOARD_LL, netJetSimulator.keyboard.backgroundThread, NULL, 0);
	return 0;
}

typedef DWORD(*_NetJetControllerEnableMouseMapping)();
extern "C" DWORD callNetJetControllerEnableMouseMapping() {
	_NetJetControllerEnableMouseMapping originalNetJetControllerEnableMouseMapping;
	originalNetJetControllerEnableMouseMapping = (_NetJetControllerEnableMouseMapping)GetProcAddress(originalNetJetController, "NetJetControllerEnableMouseMapping");
	
	if (originalNetJetControllerEnableMouseMapping) {
		originalNetJetControllerEnableMouseMapping();
	}

	netJetSimulator.mouseMapping = true;
	ShowCursor(TRUE);
	return 0;
}

typedef DWORD(*_NetJetControllerDisableMouseMapping)();
extern "C" DWORD callNetJetControllerDisableMouseMapping() {
	_NetJetControllerDisableMouseMapping originalNetJetControllerDisableMouseMapping;
	originalNetJetControllerDisableMouseMapping = (_NetJetControllerDisableMouseMapping)GetProcAddress(originalNetJetController, "NetJetControllerDisableMouseMapping");
	
	if (originalNetJetControllerDisableMouseMapping) {
		originalNetJetControllerDisableMouseMapping();
	}

	netJetSimulator.mouseMapping = false;
	ShowCursor(FALSE);
	return 0;
}

typedef DWORD(*_NetJetControllerInitialize)();
extern "C" DWORD callNetJetControllerInitialize() {
	_NetJetControllerInitialize originalNetJetControllerInitialize;
	originalNetJetControllerInitialize = (_NetJetControllerInitialize)GetProcAddress(originalNetJetController, "NetJetControllerInitialize");
	
	if (originalNetJetControllerInitialize) {
		originalNetJetControllerInitialize();
	}

	originalXbox360Controller = LoadLibrary(L"XINPUT9_1_0.DLL");
	return 0;
}

typedef BOOL(*_NetJetControllerSuspend)();
extern "C" BOOL callNetJetControllerSuspend() {
	_NetJetControllerSuspend originalNetJetControllerSuspend;
	originalNetJetControllerSuspend = (_NetJetControllerSuspend)GetProcAddress(originalNetJetController, "NetJetControllerSuspend");

	if (originalNetJetControllerSuspend) {
		originalNetJetControllerSuspend();
	}

	netJetSimulator.suspended = true;
	return FALSE;
}

typedef BOOL(*_NetJetControllerResume)();
extern "C" BOOL callNetJetControllerResume() {
	_NetJetControllerResume originalNetJetControllerResume;
	originalNetJetControllerResume = (_NetJetControllerResume)GetProcAddress(originalNetJetController, "NetJetControllerResume");

	if (originalNetJetControllerResume) {
		originalNetJetControllerResume();
	}

	netJetSimulator.suspended = false;
	return FALSE;
}

typedef DWORD(*_NetJetControllerShutdown)();
extern "C" DWORD callNetJetControllerShutdown() {
	_NetJetControllerShutdown originalNetJetControllerShutdown;
	originalNetJetControllerShutdown = (_NetJetControllerShutdown)GetProcAddress(originalNetJetController, "NetJetControllerShutdown");

	if (originalNetJetControllerShutdown) {
		originalNetJetControllerShutdown();
	}
	return 0;
}

typedef DWORD(*_NetJetControllerSetKeyMapping)(WORD);
extern "C" DWORD callNetJetControllerSetKeyMapping(WORD buttons) {
	_NetJetControllerSetKeyMapping originalNetJetControllerSetKeyMapping;
	originalNetJetControllerSetKeyMapping = (_NetJetControllerSetKeyMapping)GetProcAddress(originalNetJetController, "NetJetControllerSetKeyMapping");

	if (originalNetJetControllerSetKeyMapping) {
		originalNetJetControllerSetKeyMapping(buttons);
	}
	return 0;
}

typedef BOOL(__cdecl *_NetJetControllerSetOption)(WORD, WORD);
extern "C" BOOL __cdecl callNetJetControllerSetOption(WORD buttons, WORD priority) {
	_NetJetControllerSetOption originalNetJetControllerSetOption;
	originalNetJetControllerSetOption = (_NetJetControllerSetOption)GetProcAddress(originalNetJetController, "NetJetControllerSetOption");

	if (originalNetJetControllerSetOption) {
		originalNetJetControllerSetOption(buttons, priority);
	}
	return FALSE;
}

typedef BOOL(__cdecl *_NetJetControllerGetState)(PDWORD, PDWORD, PDWORD);
extern "C" BOOL __cdecl callNetJetControllerGetState(PDWORD buttons, PDWORD thumbRX, PDWORD thumbRY) {
	_NetJetControllerGetState originalNetJetControllerGetState;
	originalNetJetControllerGetState = (_NetJetControllerGetState)GetProcAddress(originalNetJetController, "NetJetControllerGetState");
	BOOL originalResult = false;

	if (originalNetJetControllerGetState) {
		originalResult = originalNetJetControllerGetState(buttons, thumbRX, thumbRY);
	}

	netJetSimulator.callNetJetControllerGetState(buttons, thumbRX, thumbRY, originalResult);
	return FALSE;
}

typedef BOOL(__cdecl *_NetJetControllerSetWindow)(HWND);
extern "C" BOOL __cdecl callNetJetControllerSetWindow(HWND hWnd) {
	_NetJetControllerSetWindow originalNetJetControllerSetWindow;
	originalNetJetControllerSetWindow = (_NetJetControllerSetWindow)GetProcAddress(originalNetJetController, "NetJetControllerSetWindow");

	if (originalNetJetControllerSetWindow) {
		originalNetJetControllerSetWindow(hWnd);
	}
	return FALSE;
}

typedef BOOL(__cdecl *_NetJetControllerGetControllerKey)(DWORD);
extern "C" BOOL __cdecl callNetJetControllerGetControllerKey(DWORD buttons) {
	_NetJetControllerGetControllerKey originalNetJetControllerGetControllerKey;
	originalNetJetControllerGetControllerKey = (_NetJetControllerGetControllerKey)GetProcAddress(originalNetJetController, "NetJetControllerGetControllerKey");
	
	if (originalNetJetControllerGetControllerKey) {
		if (originalNetJetControllerGetControllerKey(buttons)) {
			netJetSimulator.callNetJetControllerGetKey((PVOID)buttons);
		}
	}
	return FALSE;
}

typedef BOOL(__cdecl *_NetJetControlleretCartrdigeKey)(DWORD);
extern "C" BOOL __cdecl callNetJetControlleretCartrdigeKey(DWORD buttons) {
	_NetJetControlleretCartrdigeKey originalNetJetControlleretCartrdigeKey;
	originalNetJetControlleretCartrdigeKey = (_NetJetControlleretCartrdigeKey)GetProcAddress(originalNetJetController, "NetJetControlleretCartrdigeKey");
	
	if (originalNetJetControlleretCartrdigeKey) {
		if (originalNetJetControlleretCartrdigeKey(buttons)) {
			netJetSimulator.callNetJetControllerGetKey((PVOID)buttons);
		}
	}
	return FALSE;
}

typedef DWORD(*_NetJetControllerRun)();
extern "C" DWORD callNetJetControllerRun() {
	_NetJetControllerRun originalNetJetControllerRun;
	originalNetJetControllerRun = (_NetJetControllerRun)GetProcAddress(originalNetJetController, "NetJetControllerRun");

	if (originalNetJetControllerRun) {
		originalNetJetControllerRun();
	}
	return 0;
}