#pragma once
typedef struct _FileMetaData
{
	bool NotFound;

	wchar_t Name[255];

	BOOLEAN IsDirectory;
	INT64 FileSize;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	UINT32 FileAttributes;
} FileMetaData, * LP_FileMetaData;