/*
 *  File: Utils.hpp
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

#pragma once

#include <string>
#include <vector>
#include <windows.h>

std::string sjis2utf8(const char* sjis)
{
	int len = MultiByteToWideChar(932, 0, sjis, -1, NULL, 0);
	std::wstring wstr;
	wstr.resize(len);
	MultiByteToWideChar(932, 0, sjis, -1, &wstr[0], len);

	len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	std::string utf8;
	utf8.resize(len);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8[0], len, NULL, NULL);

	// Remove the null terminator added by WideCharToMultiByte
	if (!utf8.empty() && utf8.back() == '\0')
		utf8.pop_back();

	return utf8;
}

std::string utf82sjis(const std::string& utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
	std::wstring wstr;
	wstr.resize(len);
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);

	len = WideCharToMultiByte(932, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	std::string sjis;
	sjis.resize(len);
	WideCharToMultiByte(932, 0, wstr.c_str(), -1, &sjis[0], len, NULL, NULL);

	// Remove the null terminator added by WideCharToMultiByte
	if (!sjis.empty() && sjis.back() == '\0')
		sjis.pop_back();

	return sjis;
}

std::string replaceAll(const std::string& str, const std::string& from, const std::string& to)
{
	std::string result = str;
	size_t start_pos   = 0;
	while ((start_pos = result.find(from, start_pos)) != std::string::npos)
	{
		result.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Move past the replacement
	}

	return result;
}

std::vector<std::string> splitString(const std::string& str, const char& delimiter = '\n')
{
	std::vector<std::string> tokens;
	size_t start = 0;
	size_t end   = str.find(delimiter);

	while (end != std::string::npos)
	{
		tokens.push_back(str.substr(start, end - start));
		start = end + 1;
		end   = str.find(delimiter, start);
	}

	tokens.push_back(str.substr(start));
	return tokens;
}

//
// Determine the offset for the given function
//
uintptr_t findFunction(const std::vector<BYTE>& tarBytes)
{
	const uintptr_t startAddress = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	MEMORY_BASIC_INFORMATION info;
	uintptr_t endAddress = startAddress;

	do
	{
		VirtualQuery((void*)endAddress, &info, sizeof(info));
		endAddress = (uintptr_t)info.BaseAddress + info.RegionSize;
	} while (info.Protect > PAGE_NOACCESS);

	endAddress -= info.RegionSize;

	const DWORD procMemLength = static_cast<DWORD>(endAddress - startAddress);
	const DWORD tarLength     = static_cast<DWORD>(tarBytes.size());
	const BYTE* pProcData     = reinterpret_cast<BYTE*>(startAddress);

	for (DWORD i = 0; i < procMemLength - tarLength; i++)
	{
		for (DWORD j = 0; j <= tarLength; j++)
		{
			if (j == tarLength)
				return startAddress + i;
			else if (pProcData[i + j] != tarBytes[j])
				break;
		}
	}

	return static_cast<uintptr_t>(-1);
}
