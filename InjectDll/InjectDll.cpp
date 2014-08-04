// This file is part of IE11SandboxEsacapes.
//
// IE11SandboxEscapes is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// IE11SandboxEscapes is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with IE11SandboxEscapes.  If not, see <http://www.gnu.org/licenses/>.

#include "stdafx.h"
#include <vector>
#include <map>
#include <memory>

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if(!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
	{
		printf("Error 1 %d\n", GetLastError());
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if(bEnablePrivilege)
	{
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	}
	else
	{
		tp.Privileges[0].Attributes = 0;
	}

	if(!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
	{
		printf("Error adjusting privilege %d\n", GetLastError());
		return FALSE;
	}

	if(GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		printf("Not all privilges available\n");
		return FALSE;
	}

	return TRUE;
}

struct IEProcessEntry
{
	DWORD pid;
	DWORD ppid;
	std::vector<std::shared_ptr<IEProcessEntry>> children;
	bool low_integrity;
};

bool IsLowIntegrity(DWORD pid)
{
	HANDLE hToken;
	HANDLE hProcess;

	DWORD dwLengthNeeded;
	DWORD dwError = ERROR_SUCCESS;

	PTOKEN_MANDATORY_LABEL pTIL = NULL;
	DWORD dwIntegrityLevel = SECURITY_MANDATORY_MEDIUM_RID;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (hProcess)
	{
		if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
		{
			// Get the Integrity level.
			if (!GetTokenInformation(hToken, TokenIntegrityLevel,
				NULL, 0, &dwLengthNeeded))
			{
				dwError = GetLastError();
				if (dwError == ERROR_INSUFFICIENT_BUFFER)
				{
					pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc(0,
						dwLengthNeeded);
					if (pTIL != NULL)
					{
						if (GetTokenInformation(hToken, TokenIntegrityLevel,
							pTIL, dwLengthNeeded, &dwLengthNeeded))
						{
							dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid,
								(DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid) - 1));
						}
						LocalFree(pTIL);
					}
				}
			}
			CloseHandle(hToken);
		}
		CloseHandle(hProcess);
	}

	return dwIntegrityLevel < SECURITY_MANDATORY_MEDIUM_RID;
}

void ListIEProcesses()
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	DWORD current_session;
	
	ProcessIdToSessionId(GetCurrentProcessId(), &current_session);

	if(hSnapshot == INVALID_HANDLE_VALUE)
	{
		printf("Error listing processes: %d\n", GetLastError());
		return;
	}

	PROCESSENTRY32 pe32;
	std::map<DWORD, std::shared_ptr<IEProcessEntry>> ie_processes;

	pe32.dwSize = sizeof(pe32);

	if (Process32First(hSnapshot, &pe32))
	{
		do
		{
			DWORD session_id;

			if (!ProcessIdToSessionId(pe32.th32ProcessID, &session_id) || (session_id != current_session))
			{
				continue;
			}

			LPCWSTR lastSlash = wcsrchr(pe32.szExeFile, L'\\');
			if (lastSlash == nullptr)
			{
				lastSlash = pe32.szExeFile;
			}
			else
			{
				lastSlash++;
			}

			if (_wcsicmp(lastSlash, L"iexplore.exe") == 0)
			{
				std::shared_ptr<IEProcessEntry> entry(new IEProcessEntry());

				entry->pid = pe32.th32ProcessID;
				entry->ppid = pe32.th32ParentProcessID;
				entry->low_integrity = IsLowIntegrity(entry->pid);
				ie_processes[pe32.th32ProcessID] = entry;
			}

		} while (Process32Next(hSnapshot, &pe32));
	}
	
	CloseHandle(hSnapshot);

	if (!ie_processes.empty())
	{
		for (auto i : ie_processes)
		{
			auto found = ie_processes.find(i.second->ppid);
			if (found != ie_processes.end())
			{
				found->second->children.push_back(i.second);
			}
		}

		// Print the tree
		for (auto i : ie_processes)
		{
			if (!i.second->children.empty())
			{
				printf("[%d] - %s\n", i.second->pid, i.second->low_integrity ? "Low" : "Medium");
				for (auto c : i.second->children)
				{
					printf(" |-- [%d] - %s\n", c->pid, c->low_integrity ? "Low" : "Medium");
				}
			}
		}
	}
}

void PrintHelp()
{
	printf("Usage: InjectDll -l|pid PathToDll\n");
	printf("Specify -l to list all IE processes running in the current session\n");
}

int _tmain(int argc, _TCHAR* argv[])
{
	if((argc < 2) || (_tcscmp(argv[1], L"-l") && (argc < 3)))
	{
		PrintHelp();
		return 1;
	}

	if (_tcscmp(argv[1], L"-l") == 0)
	{
		ListIEProcesses();
		return 0;
	}

	WCHAR path[MAX_PATH];

	GetFullPathName(argv[2], MAX_PATH, path, nullptr);
	int pid = wcstoul(argv[1], 0, 0);

	printf("Injecting DLL: %ls into PID: %d\n", path, pid);

	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);

	SetPrivilege(hToken, SE_DEBUG_NAME, TRUE);

	HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
	if(hProcess)
	{
		size_t strSize = (wcslen(path) + 1) * sizeof(WCHAR);		
		LPVOID pBuf = VirtualAllocEx(hProcess, 0, strSize, MEM_COMMIT, PAGE_READWRITE);
		if(pBuf == NULL)
		{
			printf("Couldn't allocate memory in process\n");
			return 1;
		}
		SIZE_T written;
		if (!WriteProcessMemory(hProcess, pBuf, path, strSize, &written))
		{
			printf("Couldn't write to process memory\n");
			return 1;
		}

		LPVOID pLoadLibraryW = GetProcAddress(GetModuleHandle(L"kernel32"), "LoadLibraryW");

		if(!CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pBuf, 0, NULL))
		{
			printf("Couldn't create remote thread %d\n", GetLastError());
		}
	}
	else
	{
		printf("Couldn't open process %d\n", GetLastError());
	}

	return 0;
}

