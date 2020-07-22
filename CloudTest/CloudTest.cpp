// CloudTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdafx.h"

#include "CloudTest.h"
#include <iostream>

#include "Utilities.h"
#include "CloudProviderRegistrar.h"
#include <filesystem>
#include <conio.h>
#define CHUNKSIZE 4096
#define FIELD_SIZE( type, field ) ( sizeof( ( (type*)0 )->field ) )
#define CF_SIZE_OF_OP_PARAM( field )                                           \
    ( FIELD_OFFSET( CF_OPERATION_PARAMETERS, field ) +                         \
      FIELD_SIZE( CF_OPERATION_PARAMETERS, field ) )
#define CHUNKDELAYMS 250

CF_CONNECTION_KEY s_transferCallbackConnectionKey;
HRESULT Init(std::wstring localRoot);
HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, _Out_ USN& usn);
void DisconnectSyncRootTransferCallbacks();
void ConnectSyncRootTransferCallbacks(std::wstring localRoot);
static void CALLBACK OnFetchData(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters);
static void CALLBACK OnCancelFetchData(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters);

static void CopyFromServerToClient(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters,
	_In_ LPCWSTR serverFolder);
static void CopyFromServerToClientWorker(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_opt_ CONST CF_PROCESS_INFO* processInfo,
	_In_ LARGE_INTEGER requiredFileOffset,
	_In_ LARGE_INTEGER requiredLength,
	_In_ LARGE_INTEGER optionalFileOffset,
	_In_ LARGE_INTEGER optionalLength,
	_In_ CF_CALLBACK_FETCH_DATA_FLAGS fetchFlags,
	_In_ UCHAR priorityHint,
	_In_ LPCWSTR serverFolder);
static void CancelCopyFromServerToClient(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters);
static void CancelCopyFromServerToClientWorker(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ LARGE_INTEGER cancelFileOffset,
	_In_ LARGE_INTEGER cancelLength,
	_In_ CF_CALLBACK_CANCEL_FLAGS cancelFlags);
static void TransferData(
	_In_ CF_CONNECTION_KEY connectionKey,
	_In_ LARGE_INTEGER transferKey,
	_In_reads_bytes_opt_(length.QuadPart) LPCVOID transferData,
	_In_ LARGE_INTEGER startingOffset,
	_In_ LARGE_INTEGER length,
	_In_ NTSTATUS completionStatus);
static void WINAPI OverlappedCompletionRoutine(
	_In_ DWORD errorCode,
	_In_ DWORD numberOfBytesTransfered,
	_Inout_ LPOVERLAPPED overlapped);
int main()
{
	CoInitialize(nullptr);
	std::cout << "Cloud sample test!\n";

	Init(L"C:\\Test\\Cloud");



	auto fullPath = L"C:\\Test\\Cloud\\test1.png";
	auto fullPath2 = L"C:\\Test\\Cloud\\test2.txt";
	// cleanup previous run
	if (std::filesystem::exists(fullPath)) {
		int result = _wremove(fullPath);
		if (result != 0) {
			std::cout << "Failed to delete old file\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return result;
		}
	}
	if (std::filesystem::exists(fullPath2)) {
		int result = _wremove(fullPath2);
		if (result != 0) {
			std::cout << "Failed to delete old file2\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return result;
		}
	}

	// SET IN SYNC
	std::cout << "Try Set In Sync\n";
	std::cout << "\n";

	USN usn;
	HANDLE hFileHandle1;
	WIN32_FIND_DATA findData1;
	std::wstring sourceSubDir(L"");
	if (sourceSubDir[0])
	{
		sourceSubDir.append(L"\\");
	}

	std::wstring fileName(L"C:\\Test\\CloudTest");
	fileName.append(sourceSubDir);
	fileName.append(L"\\*");

	hFileHandle1 =
		FindFirstFileEx(
			fileName.data(),
			FindExInfoStandard,
			&findData1,
			FindExSearchNameMatch,
			NULL,
			FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY);
	std::cout <<"file handle value"<< hFileHandle1 << ".\n";
	std::wcout << "Findnextfile:"<<FindNextFile(hFileHandle1, &findData1) << ".\n" <<" find data value:"<< &findData1 << ".\n";
	FindNextFile(hFileHandle1, &findData1);
	std::wstring relativeName(sourceSubDir);
	relativeName.append(findData1.cFileName);
	std::wcout << "before calling create placeholder :relative path " << relativeName << ".\n";
		auto hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"", relativeName, false, usn);
		// We got our USN
		std::cout << "Created placeholder test1.txt with USN " << usn << ".\n";

		if (hr != S_OK) {
			std::cout << "Failed to create placeholder\n";

			DisconnectSyncRootTransferCallbacks();
			CoUninitialize();
			_getch();
			return hr;
		}

	HANDLE fileHandle;
	auto fullPath3 = L"C:\\Test\\Cloud\\Testdoc.txt";
	//auto fullPath3 = L"C:\\Test\\TestFolder\\Testdoc.txt";

	hr = CfOpenFileWithOplock(fullPath3, CF_OPEN_FILE_FLAGS::CF_OPEN_FILE_FLAG_WRITE_ACCESS, &fileHandle);
	std::cout << "After opening file hr value:"<<hr<<"\n";
	if (S_OK != hr)
	{
		std::cout << "Failed to open file\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}
	USN tmpUsn;
	tmpUsn = 0;
	// At this point USN is still valid.
	std::cout << "Try to set Sync state IN_SYNC\n";
	hr = CfSetInSyncState(fileHandle, CF_IN_SYNC_STATE::CF_IN_SYNC_STATE_IN_SYNC, CF_SET_IN_SYNC_FLAGS::CF_SET_IN_SYNC_FLAG_NONE, &tmpUsn);
	// Now the USN changed and the method failed.
	if (hr == ERROR_CLOUD_FILE_NOT_IN_SYNC) {
		std::cout << "Unable to set in sync with usn. Returned USN was " << tmpUsn << "\n";
	}
	else if (hr != S_OK)
	{
		std::cout << hr;
		std::cout << "Failed to set InSyncState\n";
	}
	else
	{
		std::cout << "Everything OK (will not be called).\n";
	}

	if (tmpUsn == usn) {
		std::cout << "USN was NOT changed.\n";
	}
	else {
		std::cout << "USN was changed now " << tmpUsn << ".\n";
	}


	CfCloseHandle(fileHandle);


	std::cout << "\n";
	std::cout << "\n";

	// SET NOT IN SYNC
	std::cout << "Try Set NOT In Sync\n";
	std::cout << "\n";

	hr = CreatePlaceHolder(L"C:\\Test\\Cloud", L"", L"test2.txt", true, usn);
	std::cout << "Created placeholder test2.txt with the USN " << usn << ".\n";

	usn = -1;
	std::cout << "setting USN variable to " << usn << " But will still work for NOT_IN_SYNC.\n";

	// We got our USN

	if (hr != S_OK) {
		std::cout << "Failed to create placeholder\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}


	hr = CfOpenFileWithOplock(fullPath2, CF_OPEN_FILE_FLAGS::CF_OPEN_FILE_FLAG_WRITE_ACCESS, &fileHandle);
	if (S_OK != hr)
	{
		std::cout << "Failed to open file\n";

		DisconnectSyncRootTransferCallbacks();
		CoUninitialize();
		_getch();
		return hr;
	}

	tmpUsn = usn;
	// At this point USN is still valid.
	hr = CfSetInSyncState(fileHandle, CF_IN_SYNC_STATE::CF_IN_SYNC_STATE_NOT_IN_SYNC, CF_SET_IN_SYNC_FLAGS::CF_SET_IN_SYNC_FLAG_NONE, &tmpUsn);
	// Now the USN changed and the method failed.
	if (hr == ERROR_CLOUD_FILE_NOT_IN_SYNC) {
		std::cout << "unable to set in sync with usn.\n";
	}
	else if (hr != S_OK)
	{
		std::cout << "Faild to set InSyncState\n";
	}
	else
	{
		std::cout << hr;
		std::cout << "Seting Sync state to NOT IN SYNC.\n";
	}

	if (tmpUsn == usn) {
		std::cout << "USN was NOT changed.\n";
	}
	else {
		std::cout << "USN was changed now " << tmpUsn << ".\n";
	}


	CfCloseHandle(fileHandle);




	DisconnectSyncRootTransferCallbacks();
	CoUninitialize();

	_getch();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file


HRESULT Mount(std::wstring localRoot) {

	// Stage 1: Setup
	//--------------------------------------------------------------------------------------------
	// The client folder (syncroot) must be indexed in order for states to properly display
	Utilities::AddFolderToSearchIndexer(localRoot.c_str());


	concurrency::create_task([localRoot] {
		// Register the provider with the shell so that the Sync Root shows up in File Explorer
		CloudProviderRegistrar::RegisterWithShell(localRoot, L"CloudTest");
	}).wait();

	return S_OK;
}

HRESULT IsSyncRoot(LPCWSTR path, _Out_ bool& isSyncRoot) {
	CF_SYNC_ROOT_BASIC_INFO info = { 0 };
	DWORD returnedLength;
	auto hr = CfGetSyncRootInfoByPath(path, CF_SYNC_ROOT_INFO_CLASS::CF_SYNC_ROOT_INFO_BASIC, &info, sizeof(info), &returnedLength);
	isSyncRoot = hr == S_OK && info.SyncRootFileId.QuadPart != 0;
	return S_OK;

	if (hr == 0x80070186) { // 0x80070186: This operation is only supported in a SyncRoot.
		return S_OK;
	}
	return hr;
}


HRESULT Init(std::wstring localRoot) {
	bool isAlreadySyncRoot;
	auto hr = IsSyncRoot(localRoot.c_str(), isAlreadySyncRoot);
	if (hr != S_OK) {
		return hr;
	}

	if (!isAlreadySyncRoot) {
		hr = Mount(localRoot);
		if (hr != S_OK) {
			return hr;
		}
	}

	// Hook up callback methods (in this class) for transferring files between client and server
	ConnectSyncRootTransferCallbacks(localRoot);


	return S_OK;
}

// This is a list of callbacks our fake provider support. This
// class has the callback methods, which are then delegated to
// helper classes
CF_CALLBACK_REGISTRATION s_MirrorCallbackTable[] =
{
{ CF_CALLBACK_TYPE_FETCH_DATA, OnFetchData },
	{ CF_CALLBACK_TYPE_CANCEL_FETCH_DATA, OnCancelFetchData },

	//{ CF_CALLBACK_TYPE_NOTIFY_DELETE_COMPLETION, CloudFolder::OnDeleteCompletion_C },

	//{ CF_CALLBACK_TYPE_NOTIFY_RENAME_COMPLETION, CloudFolder::OnRenameCompletion_C },
	CF_CALLBACK_REGISTRATION_END
};
void CopyFromServerToClient(
	_In_ CONST CF_CALLBACK_INFO* lpCallbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* lpCallbackParameters,
	_In_ LPCWSTR serverFolder)
{
	try
	{
		std::cout << "Inside Copy from server to client\n";
		CopyFromServerToClientWorker(
			lpCallbackInfo,
			lpCallbackInfo->ProcessInfo,
			lpCallbackParameters->FetchData.RequiredFileOffset,
			lpCallbackParameters->FetchData.RequiredLength,
			lpCallbackParameters->FetchData.OptionalFileOffset,
			lpCallbackParameters->FetchData.OptionalLength,
			lpCallbackParameters->FetchData.Flags,
			lpCallbackInfo->PriorityHint,
			serverFolder);
	}
	catch (...)
	{
		TransferData(
			lpCallbackInfo->ConnectionKey,
			lpCallbackInfo->TransferKey,
			NULL,
			lpCallbackParameters->FetchData.RequiredFileOffset,
			lpCallbackParameters->FetchData.RequiredLength,
			STATUS_UNSUCCESSFUL);
		std::cout << "transferdata catch\n";
	}
}
void CALLBACK OnFetchData(_In_ CONST CF_CALLBACK_INFO* callbackInfo, _In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
	std::cout << "onfetch data\n";
	CopyFromServerToClient(callbackInfo, callbackParameters, L"C:\\Test\\CloudTest");
}
void CALLBACK OnCancelFetchData(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
	CancelCopyFromServerToClient(callbackInfo, callbackParameters);
}
struct READ_COMPLETION_CONTEXT
{
	OVERLAPPED Overlapped;
	CF_CALLBACK_INFO CallbackInfo;
	TCHAR FullPath[MAX_PATH];
	HANDLE Handle;
	CHAR PriorityHint;
	LARGE_INTEGER StartOffset;
	LARGE_INTEGER RemainingLength;
	ULONG BufferSize;
	BYTE Buffer[1];
};

void CopyFromServerToClientWorker(
	_In_ CONST CF_CALLBACK_INFO* callbackInfo,
	_In_opt_ CONST CF_PROCESS_INFO* processInfo,
	_In_ LARGE_INTEGER requiredFileOffset,
	_In_ LARGE_INTEGER requiredLength,
	_In_ LARGE_INTEGER /*optionalFileOffset*/,
	_In_ LARGE_INTEGER /*optionalLength*/,
	_In_ CF_CALLBACK_FETCH_DATA_FLAGS /*fetchFlags*/,
	_In_ UCHAR priorityHint,
	_In_ LPCWSTR serverFolder)
{
	HANDLE serverFileHandle;

	std::wstring fullServerPath(serverFolder);
	fullServerPath.append(L"\\");
	fullServerPath.append(reinterpret_cast<wchar_t const*>(callbackInfo->FileIdentity));

	std::wstring fullClientPath(callbackInfo->VolumeDosName);
	fullClientPath.append(callbackInfo->NormalizedPath);

	READ_COMPLETION_CONTEXT* readCompletionContext;
	DWORD chunkBufferSize;
	std::cout << "Inside Copy from server to client worker\n";
	wprintf(L"[%04x:%04x] - Received data request from %s for %s%s, priority %d, offset %08x`%08x length %08x`%08x\n",
		GetCurrentProcessId(),
		GetCurrentThreadId(),
		(processInfo && processInfo->ImagePath) ? processInfo->ImagePath : L"UNKNOWN",
		callbackInfo->VolumeDosName,
		callbackInfo->NormalizedPath,
		priorityHint,
		requiredFileOffset.HighPart,
		requiredFileOffset.LowPart,
		requiredLength.HighPart,
		requiredLength.LowPart);

	serverFileHandle =
		CreateFile(
			fullServerPath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
			NULL);

	if (serverFileHandle == INVALID_HANDLE_VALUE)
	{
		HRESULT hr = NTSTATUS_FROM_WIN32(GetLastError());

		wprintf(L"[%04x:%04x] - Failed to open %s for read, hr %x\n",
			GetCurrentProcessId(),
			GetCurrentThreadId(),
			fullServerPath.c_str(),
			hr);

		winrt::check_hresult(hr);
	}

	// Allocate the buffer used in the overlapped read.
	chunkBufferSize = (ULONG)min(requiredLength.QuadPart, CHUNKSIZE);

	readCompletionContext = (READ_COMPLETION_CONTEXT*)
		HeapAlloc(
			GetProcessHeap(),
			0,
			chunkBufferSize + FIELD_OFFSET(READ_COMPLETION_CONTEXT, Buffer));

	if (readCompletionContext == NULL)
	{
		HRESULT hr = E_OUTOFMEMORY;

		wprintf(L"[%04x:%04x] - Failed to allocate read buffer for %s, hr %x\n",
			GetCurrentProcessId(),
			GetCurrentThreadId(),
			fullServerPath.c_str(),
			hr);

		CloseHandle(serverFileHandle);
		winrt::check_hresult(hr);
	}

	// Tell the read completion context where to copy the chunk(s)
	wcsncpy_s(
		readCompletionContext->FullPath,
		fullClientPath.data(),
		wcslen(fullClientPath.data()));

	// Set up the remainder of the overlapped stuff
	readCompletionContext->CallbackInfo = *callbackInfo;
	readCompletionContext->Handle = serverFileHandle;
	readCompletionContext->PriorityHint = priorityHint;
	readCompletionContext->Overlapped.Offset = requiredFileOffset.LowPart;
	readCompletionContext->Overlapped.OffsetHigh = requiredFileOffset.HighPart;
	readCompletionContext->StartOffset = requiredFileOffset;
	readCompletionContext->RemainingLength = requiredLength;
	readCompletionContext->BufferSize = chunkBufferSize;

	wprintf(L"[%04x:%04x] - Downloading data for %s, priority %d, offset %08x`%08x length %08x\n",
		GetCurrentProcessId(),
		GetCurrentThreadId(),
		readCompletionContext->FullPath,
		priorityHint,
		requiredFileOffset.HighPart,
		requiredFileOffset.LowPart,
		chunkBufferSize);

	// Initiate the read for the first chunk. When this async operation
	// completes (failure or success), it will call the OverlappedCompletionRoutine
	// above with that chunk. That OverlappedCompletionRoutine is responsible for
	// subsequent ReadFileEx calls to read subsequent chunks. This is only for the
	// first one
	if (!ReadFileEx(
		serverFileHandle,
		readCompletionContext->Buffer,
		chunkBufferSize,
		&readCompletionContext->Overlapped,
		OverlappedCompletionRoutine))
	{
		HRESULT hr = NTSTATUS_FROM_WIN32(GetLastError());
		wprintf(L"[%04x:%04x] - Failed to perform async read for %s, Status %x\n",
			GetCurrentProcessId(),
			GetCurrentThreadId(),
			fullServerPath.c_str(),
			hr);

		CloseHandle(serverFileHandle);
		HeapFree(GetProcessHeap(), 0, readCompletionContext);

		winrt::check_hresult(hr);
	}
}
void CancelCopyFromServerToClientWorker(
	_In_ CONST CF_CALLBACK_INFO* lpCallbackInfo,
	_In_ LARGE_INTEGER liCancelFileOffset,
	_In_ LARGE_INTEGER liCancelLength,
	_In_ CF_CALLBACK_CANCEL_FLAGS /*dwCancelFlags*/)
{
	// Yeah, a whole lotta noting happens here, because sample.
	wprintf(L"[%04x:%04x] - Cancelling read for %s%s, offset %08x`%08x length %08x`%08x\n",
		GetCurrentProcessId(),
		GetCurrentThreadId(),
		lpCallbackInfo->VolumeDosName,
		lpCallbackInfo->NormalizedPath,
		liCancelFileOffset.HighPart,
		liCancelFileOffset.LowPart,
		liCancelLength.HighPart,
		liCancelLength.LowPart);
}
void CancelCopyFromServerToClient(
	_In_ CONST CF_CALLBACK_INFO* lpCallbackInfo,
	_In_ CONST CF_CALLBACK_PARAMETERS* lpCallbackParameters)
{
	CancelCopyFromServerToClientWorker(lpCallbackInfo,
		lpCallbackParameters->Cancel.FetchData.FileOffset,
		lpCallbackParameters->Cancel.FetchData.Length,
		lpCallbackParameters->Cancel.Flags);
}
void TransferData(
	_In_ CF_CONNECTION_KEY connectionKey,
	_In_ LARGE_INTEGER transferKey,
	_In_reads_bytes_opt_(length.QuadPart) LPCVOID transferData,
	_In_ LARGE_INTEGER startingOffset,
	_In_ LARGE_INTEGER length,
	_In_ NTSTATUS completionStatus)
{
	CF_OPERATION_INFO opInfo = { 0 };
	CF_OPERATION_PARAMETERS opParams = { 0 };

	opInfo.StructSize = sizeof(opInfo);
	opInfo.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
	opInfo.ConnectionKey = connectionKey;
	opInfo.TransferKey = transferKey;
	opParams.ParamSize = CF_SIZE_OF_OP_PARAM(TransferData);
	opParams.TransferData.CompletionStatus = completionStatus;
	opParams.TransferData.Buffer = transferData;
	opParams.TransferData.Offset = startingOffset;
	opParams.TransferData.Length = length;
	std::cout << "Inside TransferData\n";
	winrt::check_hresult(CfExecute(&opInfo, &opParams));
}

void WINAPI OverlappedCompletionRoutine(
	_In_ DWORD errorCode,
	_In_ DWORD numberOfBytesTransfered,
	_Inout_ LPOVERLAPPED overlapped)
{
	READ_COMPLETION_CONTEXT* readContext =
		CONTAINING_RECORD(overlapped, READ_COMPLETION_CONTEXT, Overlapped);

	// There is the possibility that this code will need to be retried, see end of loop
	auto keepProcessing{ false };

	do
	{
		// Determine how many bytes have been "downloaded"
		if (errorCode == 0)
		{
			if (!GetOverlappedResult(readContext->Handle, overlapped, &numberOfBytesTransfered, TRUE))
			{
				errorCode = GetLastError();
			}
			std::cout << "Error code is 0 now\n";
		}

		//
		// Fix up bytes transfered for the failure case
		//
		if (errorCode != 0)
		{
			wprintf(L"[%04x:%04x] - Async read failed for %s, Status %x\n",
				GetCurrentProcessId(),
				GetCurrentThreadId(),
				readContext->FullPath,
				NTSTATUS_FROM_WIN32(errorCode));

			numberOfBytesTransfered = (ULONG)(min(readContext->BufferSize, readContext->RemainingLength.QuadPart));
			std::cout << "Error code is non zero \n";
		}

		assert(numberOfBytesTransfered != 0);

		// Simulate passive progress. Note that the completed portion
		// should be less than the total or we will end up "completing"
		// the hydration request prematurely.
		LONGLONG total = readContext->CallbackInfo.FileSize.QuadPart + readContext->BufferSize;
		LONGLONG completed = readContext->StartOffset.QuadPart + readContext->BufferSize;

		// Update the transfer progress
		Utilities::ApplyTransferStateToFile(readContext->FullPath, readContext->CallbackInfo, total, completed);

		// Slow it down so we can see it happening
		Sleep(CHUNKDELAYMS);

		// Complete whatever range returned
		wprintf(L"[%04x:%04x] - Executing download for %s, Status %08x, priority %d, offset %08x`%08x length %08x\n",
			GetCurrentProcessId(),
			GetCurrentThreadId(),
			readContext->FullPath,
			NTSTATUS_FROM_WIN32(errorCode),
			readContext->PriorityHint,
			readContext->StartOffset.HighPart,
			readContext->StartOffset.LowPart,
			numberOfBytesTransfered);

		// This helper function tells the Cloud File API about the transfer,
		// which will copy the data to the local syncroot
		TransferData(
			readContext->CallbackInfo.ConnectionKey,
			readContext->CallbackInfo.TransferKey,
			errorCode == 0 ? readContext->Buffer : NULL,
			readContext->StartOffset,
			Utilities::LongLongToLargeInteger(numberOfBytesTransfered),
			errorCode);

		// Move the values in the read context to the next chunk
		readContext->StartOffset.QuadPart += numberOfBytesTransfered;
		readContext->RemainingLength.QuadPart -= numberOfBytesTransfered;

		// See if there is anything left to read
		if (readContext->RemainingLength.QuadPart > 0)
		{
			// Cap it at chunksize
			DWORD bytesToRead = (DWORD)(min(readContext->RemainingLength.QuadPart, readContext->BufferSize));

			// And call ReadFileEx to start the next chunk read
			readContext->Overlapped.Offset = readContext->StartOffset.LowPart;
			readContext->Overlapped.OffsetHigh = readContext->StartOffset.HighPart;

			wprintf(L"[%04x:%04x] - Downloading data for %s, priority %d, offset %08x`%08x length %08x\n",
				GetCurrentProcessId(),
				GetCurrentThreadId(),
				readContext->FullPath,
				readContext->PriorityHint,
				readContext->Overlapped.OffsetHigh,
				readContext->Overlapped.Offset,
				bytesToRead);

			// In the event of ReadFileEx succeeding, the while loop
			// will complete, this chunk is done, and whenever the OS
			// has completed this new ReadFileEx again, then this entire
			// method will be called again with the new chunk.
			// In that case, the handle and buffer need to remain intact
			if (!ReadFileEx(
				readContext->Handle,
				readContext->Buffer,
				bytesToRead,
				&readContext->Overlapped,
				OverlappedCompletionRoutine))
			{
				// In the event the ReadFileEx failed,
				// we want to loop through again to try and
				// process this again
				errorCode = GetLastError();
				numberOfBytesTransfered = 0;

				keepProcessing = true;
			}
		}
		else
		{
			// Close the read file handle and free the buffer,
			// because we are done.
			CloseHandle(readContext->Handle);
			HeapFree(GetProcessHeap(), 0, readContext);
		}
	} while (keepProcessing);
}

// Registers the callbacks in the table at the top of this file so that the methods above
// are called for our fake provider
void ConnectSyncRootTransferCallbacks(std::wstring localRoot)
{
	// Connect to the sync root using Cloud File API
	auto hr = CfConnectSyncRoot(
		localRoot.c_str(),
		s_MirrorCallbackTable,
		nullptr,
		CF_CONNECT_FLAGS::CF_CONNECT_FLAG_NONE,
		&s_transferCallbackConnectionKey);
	std::wcout << "mirror call back " << s_MirrorCallbackTable << ".\n";
	if (hr != S_OK)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		//LOG(LOG_LEVEL::Error, L"Could not connect to sync root, hr %08x", hr);
	}
}


// Unregisters the callbacks in the table at the top of this file so that 
// the client doesn't Hindenburg
void DisconnectSyncRootTransferCallbacks()
{
	//LOG(LOG_LEVEL::Info, L"Shutting down");
	auto hr = CfDisconnectSyncRoot(s_transferCallbackConnectionKey);

	if (hr != S_OK)
	{
		// winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
		// otherwise the exception will get rethrown and this method will crash out as it should
		//LOG(LOG_LEVEL::Error, L"Could not disconnect the sync root, hr %08x", hr);
	}
}






















HRESULT CreatePlaceHolder(_In_ std::wstring localRoot, _In_ PCWSTR parentPath, _In_ std::wstring fileName, bool inSync, _Out_ USN& usn)
{
	std::wstring relativePath(parentPath);
	if (relativePath.size() > 0)
		if (relativePath.at(relativePath.size() - 1) != L'\\')
		{
			relativePath.append(L"\\");
		}
	relativePath.append(fileName);
		std::wcout << "relative path " << relativePath << ".\n";
		std::wcout << "Before metadata:relative path " << relativePath << ".\n";

	FileMetaData metadata = {};

	metadata.FileSize = 0;
	metadata.IsDirectory = false;

	fileName.copy(metadata.Name, fileName.length());
	std::wcout << "After metadata:filename " << fileName << ".\n";

	CF_PLACEHOLDER_CREATE_INFO cloudEntry;
	auto fileIdentety = L"F";
	cloudEntry.FileIdentity = &fileIdentety;
	cloudEntry.FileIdentityLength = sizeof(fileIdentety);

	cloudEntry.RelativeFileName = relativePath.data();
	std::wcout << "relative path data " << relativePath.data() << ".\n";
	cloudEntry.Flags = inSync
		? CF_PLACEHOLDER_CREATE_FLAGS::CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC
		: CF_PLACEHOLDER_CREATE_FLAGS::CF_PLACEHOLDER_CREATE_FLAG_NONE;
	cloudEntry.FsMetadata.FileSize.QuadPart = metadata.FileSize;
	cloudEntry.FsMetadata.BasicInfo.FileAttributes = metadata.FileAttributes;
	cloudEntry.FsMetadata.BasicInfo.CreationTime = metadata.CreationTime;
	cloudEntry.FsMetadata.BasicInfo.LastWriteTime = metadata.LastWriteTime;
	cloudEntry.FsMetadata.BasicInfo.LastAccessTime = metadata.LastAccessTime;
	cloudEntry.FsMetadata.BasicInfo.ChangeTime = metadata.ChangeTime;

	if (metadata.IsDirectory)
	{
		cloudEntry.FsMetadata.BasicInfo.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
		cloudEntry.Flags |= CF_PLACEHOLDER_CREATE_FLAG_DISABLE_ON_DEMAND_POPULATION;
		cloudEntry.FsMetadata.FileSize.QuadPart = 0;
	}

	auto hr = CfCreatePlaceholders(localRoot.c_str(), &cloudEntry, 1, CF_CREATE_FLAGS::CF_CREATE_FLAG_NONE, NULL);
	if (hr != S_OK) {
		return hr;
	}



	//std::wstring absolutePath(>localRoot);
	//if (absolutePath.size() > 0 && absolutePath.at(absolutePath.size() - 1) != L'\\')
	//	absolutePath.append(L"\\");

	//absolutePath.append(relativePath);
	//DWORD attrib = GetFileAttributes(absolutePath.c_str());
	usn = cloudEntry.CreateUsn;
	return cloudEntry.Result;
}
