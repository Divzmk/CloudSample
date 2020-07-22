// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

class Utilities
{
public:
	inline static LARGE_INTEGER LongLongToLargeInteger(_In_ const LONGLONG longlong)
	{
		LARGE_INTEGER largeInteger;
		largeInteger.QuadPart = longlong;
		return largeInteger;
	}
    static HRESULT AddFolderToSearchIndexer(_In_ PCWSTR folder);
	static void ApplyTransferStateToFile(_In_ PCWSTR fullPath, _In_ CF_CALLBACK_INFO& callbackInfo, UINT64 total, UINT64 completed);

    static winrt::com_array<wchar_t> ConvertSidToStringSid(_In_ PSID sid)
    {
        winrt::com_array<wchar_t> string;
        if (::ConvertSidToStringSid(sid, winrt::put_abi(string)))
        {
            return string;
        }
        else
        {
            throw std::bad_alloc();
        }
    };
};
