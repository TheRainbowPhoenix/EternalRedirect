/*
 *  File: Logging.cpp
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

#include "Logging.hpp"

//////////////////////////////////////////////////////////////////////////////
//
// Required for syelog to work cannot be if defed without also modding syelog.cpp

extern "C"
{
	HANDLE(WINAPI* Real_CreateFileW)(LPCWSTR a0,
									 DWORD a1,
									 DWORD a2,
									 LPSECURITY_ATTRIBUTES a3,
									 DWORD a4,
									 DWORD a5,
									 HANDLE a6) = CreateFileW;

	BOOL(WINAPI* Real_WriteFile)(HANDLE hFile,
								 LPCVOID lpBuffer,
								 DWORD nNumberOfBytesToWrite,
								 LPDWORD lpNumberOfBytesWritten,
								 LPOVERLAPPED lpOverlapped) = WriteFile;

	BOOL(WINAPI* Real_FlushFileBuffers)(HANDLE hFile) = FlushFileBuffers;

	BOOL(WINAPI* Real_CloseHandle)(HANDLE hObject) = CloseHandle;

	BOOL(WINAPI* Real_WaitNamedPipeW)(LPCWSTR lpNamedPipeName, DWORD nTimeOut) = WaitNamedPipeW;

	BOOL(WINAPI* Real_SetNamedPipeHandleState)(HANDLE hNamedPipe,
											   LPDWORD lpMode,
											   LPDWORD lpMaxCollectionCount,
											   LPDWORD lpCollectDataTimeout) = SetNamedPipeHandleState;

	DWORD(WINAPI* Real_GetCurrentProcessId)(VOID) = GetCurrentProcessId;

	VOID(WINAPI* Real_GetSystemTimeAsFileTime)(LPFILETIME lpSystemTimeAsFileTime) = GetSystemTimeAsFileTime;

	VOID(WINAPI* Real_InitializeCriticalSection)(LPCRITICAL_SECTION lpSection) = InitializeCriticalSection;

	VOID(WINAPI* Real_EnterCriticalSection)(LPCRITICAL_SECTION lpSection) = EnterCriticalSection;

	VOID(WINAPI* Real_LeaveCriticalSection)(LPCRITICAL_SECTION lpSection) = LeaveCriticalSection;
}

//
//
//////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////// Logging System.
//
#if INCLUDE_DEBUG_LOGGING
static BOOL s_bLog       = 1;
static LONG s_nTlsIndent = -1;
static LONG s_nTlsThread = -1;
static LONG s_nThreadCnt = 0;

VOID _PrintEnter(const CHAR* psz, ...)
{
	DWORD dwErr = GetLastError();

	LONG nIndent = 0;
	LONG nThread = 0;
	if (s_nTlsIndent >= 0)
	{
		nIndent = (LONG)(LONG_PTR)TlsGetValue(s_nTlsIndent);
		TlsSetValue(s_nTlsIndent, (PVOID)(LONG_PTR)(nIndent + 1));
	}

	if (s_nTlsThread >= 0)
		nThread = (LONG)(LONG_PTR)TlsGetValue(s_nTlsThread);

	if (s_bLog && psz)
	{
		CHAR szBuf[1024];
		PCHAR pszBuf = szBuf;
		PCHAR pszEnd = szBuf + ARRAYSIZE(szBuf) - 1;
		LONG nLen    = (nIndent > 0) ? (nIndent < 35 ? nIndent * 2 : 70) : 0;
		*pszBuf++    = (CHAR)('0' + ((nThread / 100) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 10) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 1) % 10));
		*pszBuf++    = ' ';
		while (nLen-- > 0)
			*pszBuf++ = ' ';

		va_list args;
		va_start(args, psz);

		while ((*pszBuf++ = *psz++) != 0 && pszBuf < pszEnd)
		{
			// Copy characters.
		}

		*pszEnd = '\0';
		SyelogV(SYELOG_SEVERITY_INFORMATION, szBuf, args);

		va_end(args);
	}

	SetLastError(dwErr);
}

VOID _PrintExit(const CHAR* psz, ...)
{
	DWORD dwErr = GetLastError();

	LONG nIndent = 0;
	LONG nThread = 0;
	if (s_nTlsIndent >= 0)
	{
		nIndent = (LONG)(LONG_PTR)TlsGetValue(s_nTlsIndent) - 1;
		TlsSetValue(s_nTlsIndent, (PVOID)(LONG_PTR)nIndent);
	}

	if (s_nTlsThread >= 0)
		nThread = (LONG)(LONG_PTR)TlsGetValue(s_nTlsThread);

	if (s_bLog && psz)
	{
		CHAR szBuf[1024];
		PCHAR pszBuf = szBuf;
		PCHAR pszEnd = szBuf + ARRAYSIZE(szBuf) - 1;
		LONG nLen    = (nIndent > 0) ? (nIndent < 35 ? nIndent * 2 : 70) : 0;
		*pszBuf++    = (CHAR)('0' + ((nThread / 100) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 10) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 1) % 10));
		*pszBuf++    = ' ';
		while (nLen-- > 0)
			*pszBuf++ = ' ';

		va_list args;
		va_start(args, psz);

		while ((*pszBuf++ = *psz++) != 0 && pszBuf < pszEnd)
		{
			// Copy characters.
		}

		*pszEnd = '\0';
		SyelogV(SYELOG_SEVERITY_INFORMATION, szBuf, args);

		va_end(args);
	}

	SetLastError(dwErr);
}

VOID _Print(const CHAR* psz, ...)
{
	DWORD dwErr = GetLastError();

	LONG nIndent = 0;
	LONG nThread = 0;
	if (s_nTlsIndent >= 0)
		nIndent = (LONG)(LONG_PTR)TlsGetValue(s_nTlsIndent);

	if (s_nTlsThread >= 0)
		nThread = (LONG)(LONG_PTR)TlsGetValue(s_nTlsThread);

	if (s_bLog && psz)
	{
		CHAR szBuf[1024];
		PCHAR pszBuf = szBuf;
		PCHAR pszEnd = szBuf + ARRAYSIZE(szBuf) - 1;
		LONG nLen    = (nIndent > 0) ? (nIndent < 35 ? nIndent * 2 : 70) : 0;
		*pszBuf++    = (CHAR)('0' + ((nThread / 100) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 10) % 10));
		*pszBuf++    = (CHAR)('0' + ((nThread / 1) % 10));
		*pszBuf++    = ' ';
		while (nLen-- > 0)
			*pszBuf++ = ' ';

		va_list args;
		va_start(args, psz);

		while ((*pszBuf++ = *psz++) != 0 && pszBuf < pszEnd)
		{
			// Copy characters.
		}

		*pszEnd = '\0';
		SyelogV(SYELOG_SEVERITY_INFORMATION, szBuf, args);

		va_end(args);
	}

	SetLastError(dwErr);
}

namespace logging
{
void Setup()
{
	s_bLog       = FALSE;
	s_nTlsIndent = TlsAlloc();
	s_nTlsThread = TlsAlloc();
}

void Cleanup()
{
	if (s_nTlsIndent >= 0)
		TlsFree(s_nTlsIndent);

	if (s_nTlsThread >= 0)
		TlsFree(s_nTlsThread);
}

void SetBLog(BOOL bLog)
{
	s_bLog = bLog;
}

void ThreadAttach()
{
	if (s_nTlsIndent >= 0)
		TlsSetValue(s_nTlsIndent, (PVOID)0);
	if (s_nTlsThread >= 0)
	{
		LONG nThread = InterlockedIncrement(&s_nThreadCnt);
		TlsSetValue(s_nTlsThread, (PVOID)(LONG_PTR)nThread);
	}
}

void ThreadDetach()
{
	if (s_nTlsIndent >= 0)
		TlsSetValue(s_nTlsIndent, (PVOID)0);
	if (s_nTlsThread >= 0)
		TlsSetValue(s_nTlsThread, (PVOID)0);
}
} // namespace logging
#else
namespace logging
{

void Setup()
{
	// No logging
}

void Cleanup()
{
	// No logging
}

void SetBLog([[maybe_unused]] BOOL bLog)
{
	// No logging
}

void ThreadAttach()
{
	// No logging
}

void ThreadDetach()
{
	// No logging
}

} // namespace logging
#endif
