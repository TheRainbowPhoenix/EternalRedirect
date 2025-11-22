/*
 *  File: EternalRedirect.cpp
 *  Copyright (c) 2025 Sinflower
 *
 *  MIT License
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#include <fstream>
#include <stdio.h>
#include <vector>
#include <windows.h>

#include <detours.h>
#include <json.hpp>

#include "Utils.hpp"

#include "Logging.hpp"

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

VOID DetAttach(PVOID* ppbReal, PVOID pbMine, const char* psz);
VOID DetDetach(PVOID* ppbReal, PVOID pbMine, const char* psz);

#define ATTACH(x) DetAttach(&(PVOID&)Real_##x, Mine_##x, #x)
#define DETACH(x) DetDetach(&(PVOID&)Real_##x, Mine_##x, #x)

struct TranslationEntry
{
	TranslationEntry() = default;
	TranslationEntry(const std::string& t, const uint32_t& pl) :
		text(t), pixelLength(pl) {}

	std::string text     = "";
	uint32_t pixelLength = 0;

	void clear()
	{
		text        = "";
		pixelLength = 0;
	}

	bool operator<(const uint32_t& other) const
	{
		return pixelLength < other;
	}

	bool operator>(const uint32_t& other) const
	{
		return pixelLength > other;
	}
};

nlohmann::json g_translations;
TranslationEntry g_largestCopiedStrSinceResize = {};

static const std::string TRANSLATIONS_FILE = "tr.json";

static const std::vector<BYTE> DRAW_FORMAT_VSTRING_FUNC          = { 0x40, 0x53, 0x55, 0x56, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81 };
static const std::vector<BYTE> COPY_FUNC                         = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF9, 0x48, 0xC7, 0xC3 };
static const std::vector<BYTE> GET_DRAW_FORMAT_STRING_WIDTH_FUNC = { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89, 0x4C, 0x24, 0x20, 0x53, 0x56 };

//////////////////////////////////////////////////////////////////////////////
//
// Real function pointers for detoured functions

extern "C"
{
	typedef int(WINAPI* DrawFormatVStringToHandle)(int x, int y, unsigned int Color, int FontHandle, const char* FormatString, ...);
	DrawFormatVStringToHandle Real_DrawFormatVStringToHandle = nullptr;

	typedef VOID*(WINAPI* CopyFunc)(void* a1, uint8_t* a2, int64_t a3);
	CopyFunc Real_CopyFunc = nullptr;

	int64_t(WINAPI* Real_GetDrawFormatStringWidth)(const char* FormatString, ...) = nullptr;
}

//
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// Detours
//

int64_t WINAPI Mine_GetDrawFormatStringWidth(const char* FormatString, ...)
{
	int64_t result = -1;

	std::string utf8String = sjis2utf8(FormatString);

	// Check if this string exists in the translations
	if (g_translations.contains(utf8String))
	{
		const nlohmann::json& translationEntry   = g_translations[utf8String];
		std::string tStr                         = translationEntry["text"];
		const std::vector<uint32_t> pixelLengths = translationEntry["pixel_lengths"].get<std::vector<uint32_t>>();
		// This should only have a single entry so just take the first -- Maybe expand later if needed
		const uint32_t pixelLength = pixelLengths.empty() ? 0 : pixelLengths[0];

#if INCLUDE_DEBUG_LOGGING
		// Syelog(SYELOG_SEVERITY_INFORMATION, "Calculating width for translated string: \"%s\" (expected pixel length: %d)\n", tStr.c_str(), pixelLength);
#endif

		// Now determine which is the largest string
		if (g_largestCopiedStrSinceResize > pixelLength)
		{
#if INCLUDE_DEBUG_LOGGING
			// Syelog(SYELOG_SEVERITY_INFORMATION, "Using largest copied string since resize for width calculation: \"%s\" instead of \"%s\"\n", g_largestCopiedStrSinceResize.text.c_str(), tStr.c_str());
#endif
			tStr = g_largestCopiedStrSinceResize.text;
		}

		// Clear the largest string since resize after using it
		g_largestCopiedStrSinceResize.clear();

		result = Real_GetDrawFormatStringWidth(tStr.c_str());
	}
	else
	{
		result = Real_GetDrawFormatStringWidth(FormatString);
	}

	return result;
}

VOID* WINAPI Mine_CopyFunc(void* a1, uint8_t* a2, int64_t a3)
{
	VOID* result = nullptr;

	std::string utf8String = sjis2utf8(reinterpret_cast<const char*>(a2));
	// Check if this string exists in the translations
	if (g_translations.contains(utf8String))
	{
		const nlohmann::json& translationEntry = g_translations[utf8String];

		const std::string tStr             = translationEntry["text"];
		std::vector<uint32_t> pixelLengths = translationEntry["pixel_lengths"].get<std::vector<uint32_t>>();
		std::vector<std::string> lines     = splitString(tStr, '\n');

		// Find the largest line by pixel length
		for (size_t i = 0; i < lines.size(); i++)
		{
			if (i < pixelLengths.size() && g_largestCopiedStrSinceResize < pixelLengths[i])
				g_largestCopiedStrSinceResize = TranslationEntry(lines[i], pixelLengths[i]);
		}

		uint8_t* pBuffer = new uint8_t[tStr.size() + 1]();
		memcpy(pBuffer, tStr.c_str(), tStr.size());
		result = Real_CopyFunc(a1, pBuffer, a3);
		delete[] pBuffer;
	}
	else
	{
		result = Real_CopyFunc(a1, a2, a3);
	}

	return result;
}

int WINAPI Mine_DrawFormatVStringToHandle(int x, int y, unsigned int Color, int FontHandle, const char* FormatString, ...)
{
	int result = -1;

	char buffer[4096];
	va_list args;
	va_start(args, FormatString);
	vsnprintf(buffer, sizeof(buffer), FormatString, args);
	va_end(args);

	std::string utf8String = sjis2utf8(buffer);

	g_largestCopiedStrSinceResize.clear();

	// Check if this string exists in the translations
	if (g_translations.contains(utf8String))
	{
		std::string translatedString = g_translations[utf8String]["text"];
		result                       = Real_DrawFormatVStringToHandle(x, y, Color, FontHandle, translatedString.c_str());
	}
	else
	{
		result = Real_DrawFormatVStringToHandle(x, y, Color, FontHandle, buffer);
	}

	return result;
}

//
// Detours
//////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
// AttachDetours
//
PCHAR DetRealName(const char* psz)
{
	PCHAR locPsz = const_cast<PCHAR>(psz);
	PCHAR pszBeg = const_cast<PCHAR>(psz);
	// Move to end of name.
	while (*locPsz)
		locPsz++;

	// Move back through A-Za-z0-9 names.
	while (locPsz > pszBeg && ((locPsz[-1] >= 'A' && locPsz[-1] <= 'Z') || (locPsz[-1] >= 'a' && locPsz[-1] <= 'z') || (locPsz[-1] >= '0' && locPsz[-1] <= '9')))
		locPsz--;

	return locPsz;
}

VOID DetAttach(PVOID* ppbReal, PVOID pbMine, const char* psz)
{
	LONG l = DetourAttach(ppbReal, pbMine);
#if INCLUDE_DEBUG_LOGGING
	if (l != 0)
		Syelog(SYELOG_SEVERITY_NOTICE, "Attach failed: `%s': error %d\n", DetRealName(psz), l);
#endif
}

VOID DetDetach(PVOID* ppbReal, PVOID pbMine, const char* psz)
{
	LONG l = DetourDetach(ppbReal, pbMine);
#if INCLUDE_DEBUG_LOGGING
	if (l != 0)
		Syelog(SYELOG_SEVERITY_NOTICE, "Detach failed: `%s': error %d\n", DetRealName(psz), l);
#endif
}

LONG AttachDetours(VOID)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	ATTACH(DrawFormatVStringToHandle);
	ATTACH(CopyFunc);
	ATTACH(GetDrawFormatStringWidth);

	return DetourTransactionCommit();
}

LONG DetachDetours(VOID)
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DETACH(DrawFormatVStringToHandle);
	DETACH(CopyFunc);
	DETACH(GetDrawFormatStringWidth);

	return DetourTransactionCommit();
}

//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// DLL module information
//
BOOL ThreadAttach([[maybe_unused]] HMODULE hDll)
{
#if INCLUDE_DEBUG_LOGGING
	logging::ThreadAttach();
#endif

	return TRUE;
}

BOOL ThreadDetach([[maybe_unused]] HMODULE hDll)
{
#if INCLUDE_DEBUG_LOGGING
	logging::ThreadDetach();
#endif

	return TRUE;
}

template<typename T>
void SetupHook(T& realFuncPtr, const std::vector<BYTE>& funcBytes, const char* funcName)
{
	uintptr_t funcAddr = findFunction(funcBytes);

	if (funcAddr == ~0)
	{
#if INCLUDE_DEBUG_LOGGING
		Syelog(SYELOG_SEVERITY_FATAL, "### Error: Unable to find the %s function\n", funcName);
#endif
		return;
	}

	realFuncPtr = reinterpret_cast<T>(funcAddr);
}

BOOL ProcessAttach(HMODULE hDll)
{
#if INCLUDE_DEBUG_LOGGING
	WCHAR wzExeName[MAX_PATH];

	GetModuleFileNameW(NULL, wzExeName, ARRAYSIZE(wzExeName));

	SyelogOpen("eternal" DETOURS_STRINGIFY(DETOURS_BITS), SYELOG_FACILITY_APPLICATION);
	Syelog(SYELOG_SEVERITY_INFORMATION, "##################################################################\n");
	Syelog(SYELOG_SEVERITY_INFORMATION, "### %ls\n", wzExeName);

	Syelog(SYELOG_SEVERITY_INFORMATION, "### Loading translations...\n");
#endif

	std::ifstream i(TRANSLATIONS_FILE);
	if (i.is_open())
	{
		i >> g_translations;

#if INCLUDE_DEBUG_LOGGING
		Syelog(SYELOG_SEVERITY_INFORMATION, "### Loaded %d translations.\n", g_translations.size());
#endif
	}
	else
	{
#if INCLUDE_DEBUG_LOGGING
		Syelog(SYELOG_SEVERITY_WARNING, "### Warning: Could not open %s\n", TRANSLATIONS_FILE.c_str());
#endif
	}

	SetupHook(Real_DrawFormatVStringToHandle, DRAW_FORMAT_VSTRING_FUNC, "DrawFormatVStringToHandle");
	SetupHook(Real_CopyFunc, COPY_FUNC, "CopyFunc");
	SetupHook(Real_GetDrawFormatStringWidth, GET_DRAW_FORMAT_STRING_WIDTH_FUNC, "GetDrawFormatStringWidth");

	LONG error = AttachDetours();

#if INCLUDE_DEBUG_LOGGING
	if (error != NO_ERROR)
		Syelog(SYELOG_SEVERITY_FATAL, "### Error attaching detours: %d\n", error);

	Syelog(SYELOG_SEVERITY_NOTICE, "### Attached.\n");
#endif

	ThreadAttach(hDll);

	logging::SetBLog(TRUE);

	return TRUE;
}

BOOL ProcessDetach(HMODULE hDll)
{
	ThreadDetach(hDll);

	logging::SetBLog(FALSE);

	LONG error = DetachDetours();

#if INCLUDE_DEBUG_LOGGING
	if (error != NO_ERROR)
		Syelog(SYELOG_SEVERITY_FATAL, "### Error detaching detours: %d\n", error);

	Syelog(SYELOG_SEVERITY_NOTICE, "### Closing.\n");
	SyelogClose(FALSE);

	logging::Cleanup();
#endif

	return TRUE;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
	(void)hModule;
	(void)lpReserved;

	if (DetourIsHelperProcess())
		return TRUE;

	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			DetourRestoreAfterWith();
			return ProcessAttach(hModule);
		case DLL_PROCESS_DETACH:
			return ProcessDetach(hModule);
		case DLL_THREAD_ATTACH:
			return ThreadAttach(hModule);
		case DLL_THREAD_DETACH:
			return ThreadDetach(hModule);
	}

	return TRUE;
}
//
///////////////////////////////////////////////////////////////// End of File.
