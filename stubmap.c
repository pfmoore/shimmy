/*
   This program demonstrates file mapping, especially how to align a
   view with the system file allocation granularity.
*/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

int main(void)
{
  HANDLE hMapFile;      // handle for the file's memory-mapped region
  HANDLE hFile;         // the file handle
  BOOL bFlag;           // a result holder
  DWORD dwFileSize;     // temporary storage for file sizes
  LPVOID lpMapAddress;  // pointer to the base address of the
                        // memory-mapped region

  hFile = CreateFileW(L"footer", GENERIC_READ, 0, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
  {
    wprintf(L"hFile is NULL\n");
    return 4;
  }

  dwFileSize = GetFileSize(hFile,  NULL);
  wprintf(L"hFile size: %10d\n", dwFileSize);

  // Create a file mapping object for the file
  // Note that it is a good idea to ensure the file size is not zero
  hMapFile = CreateFileMapping( hFile,          // current file handle
                NULL,           // default security
                PAGE_READONLY, // read/write permission
                0,              // size of mapping object, high
                0,  // size of mapping object, low
                NULL);          // name of mapping object

  if (hMapFile == NULL)
  {
    wprintf(L"hMapFile is NULL: last error: %d\n", GetLastError());
    return (2);
  }

  // Map the view and test the results.

  lpMapAddress = MapViewOfFile(hMapFile,            // handle to
                                                    // mapping object
                               FILE_MAP_READ, // read/write
                               0,                   // high-order 32
                                                    // bits of file
                                                    // offset
                               0,      // low-order 32
                                                    // bits of file
                                                    // offset
                               0);      // number of bytes
                                                    // to map
  if (lpMapAddress == NULL)
  {
    wprintf(L"lpMapAddress is NULL: last error: %d\n", GetLastError());
    return 3;
  }

  printf("Content of file: %s", (char *)lpMapAddress);
  // Close the file mapping object and the open file

  bFlag = UnmapViewOfFile(lpMapAddress);
  bFlag = CloseHandle(hMapFile); // close the file mapping object

  if(!bFlag)
  {
    wprintf(L"\nError %ld occurred closing the mapping object!",
             GetLastError());
  }

  bFlag = CloseHandle(hFile);   // close the file itself

  if(!bFlag)
  {
    wprintf(L"\nError %ld occurred closing the file!",
           GetLastError());
  }

  return 0;
}
