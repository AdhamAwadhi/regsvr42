/************************************************************************/
/* Copyright (c) 2018 CBrain A/S. Version modified from original version by Cristian Adam
 * Copyright (c) 2008 Cristian Adam.

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	distribution.

/************************************************************************/

#include "stdafx.h"
#include "Interceptor.h"
#include "ManifestWriter.h"

void PrintUsage()
{
	std::wcout << L"Regsvr42 (c) 2018 CBrain A/S" << std::endl;
	std::wcout << L"         (c) 2008 Cristian Adam" << std::endl << std::endl;
	std::wcout << L"This tool will spy on COM registration process and create side by side manifest file(s)" << std::endl;
	std::wcout << L"-- Modified from original version by Cristian Adam --";
	std::wcout << std::endl << std::endl;

	std::wcout << L"Usage: regsvr42 [parameters] <com_dll>" << std::endl;
	std::wcout << L"Parameters:" << std::endl;
	std::wcout << L"  -trace\t\tDisplay a trace of monitored registry functions" << std::endl;
	std::wcout << L"  -version:<version>\tAssembly version. Default is 1.0.0.0" << std::endl;
	std::wcout << L"    <version> can alternatively be one of:" << std::endl;
	std::wcout << "       ProductVersion: PRODUCTVERSION from fixed-info part of VERSIONINFO." << std::endl;
	std::wcout << "       FileVersion: FILEVERSION from fixed-info part of VERSIONINFO." << std::endl;
	std::wcout << "       ProductVersion*, FileVersion*: Same but without build number." << std::endl;
	std::wcout << L"  -name:<name>\t\tAssembly name, default is <com_dll_without_ext>.sxs" << std::endl;
	std::wcout << L"  -arch\t\t\tInclude processor architectecture in assembly identity." << std::endl;
	std::wcout << L"  -hash\t\t\tAdd SHA1 hash for file declarations" << std::endl;
	std::wcout << L"  -sha256\t\tAdd SHA256 hash for file declarations" << std::endl;
	std::wcout << L"  -size\t\t\tAdd file size for file declarations" << std::endl;
	std::wcout << L"  -client:<client>\tGenerates a client manifest file " << std::endl;
	std::wcout << L"          \t\t-client:test.exe => test.exe.manifst" << std::endl;
	std::wcout << L"  -batch:<input_file>\tProcessiong of multiple com_dlls." << std::endl;
	std::wcout << L"          \t\tDlls are read line by line (only ANSI file)." << std::endl;
	std::wcout << L"          \t\tAssembly name and version can be put on the same line" << std::endl;
	std::wcout << L"          \t\twith the dll name, separated by | character" << std::endl;
	std::wcout << L"          \t\tAssembly name is ignored when used with directories" << std::endl;
	std::wcout << L"  -dir:<directory name>\tAll the files from that directory are" << std::endl;
	std::wcout << L"          \t\tprocessed into one manifest file named" << std::endl;
	std::wcout << L"          \t\t<directory name>.manifest" << std::endl;
}

struct ComInitializer
{
	ComInitializer()
	{
		::CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	}

	~ComInitializer()
	{
		::CoUninitialize();
	}
};

Interceptor::ValuesListType CaptureManifestForModule(std::wstring &fileName, bool doTrace)
{
	ComInitializer comInitializer;

	Interceptor::ValuesListType capturedManifestData;

	HRESULT hr = S_OK;
	try
	{
		std::wstring relativeFileName = fileName;

		// If the file has a path add that path to the dll search list, maybe has some dependent dlls, so that LoadLibrary should not fail!
		std::wstring::size_type lastPos = fileName.rfind(L'\\');
		if (lastPos != std::wstring::npos)
		{
			std::wstring fileNamePath = fileName.substr(0, lastPos + 1);

			if (!::SetDllDirectory(fileNamePath.c_str()))
			{
				std::wcout << L"SetDllDirectory failed for: " << fileNamePath << L", GetLastError: 0x"
					<< std::hex << ::GetLastError() << std::endl;
			}

			relativeFileName = fileName.substr(fileName.rfind(L'\\') + 1);
		}
		else
		{
			int size = ::GetCurrentDirectory(0, 0);
			std::wstring currentDirectory;
			currentDirectory.resize(size - 1);

			if (!::GetCurrentDirectory(size, &*currentDirectory.begin()))
			{
				std::wcout << L"GetCurrentDirectory failed! GetLastError: 0x" << std::hex
					<< ::GetLastError() << std::endl;
			}

			if (!::SetDllDirectory(currentDirectory.c_str()))
			{
				std::wcout << L"SetDllDirectory failed for: " << currentDirectory << L", GetLastError: 0x"
					<< std::hex << ::GetLastError() << std::endl;
			}
		}

		HMODULE library = LoadLibrary(relativeFileName.c_str());
		if (!library)
		{
			std::wcout << L"LoadLibrary failed for: " << relativeFileName << L", GetLastError: 0x"
				<< std::hex << ::GetLastError() << std::endl;
			std::wcout << L"Please use Dependency Walker (http://www.dependencywalker.com/) to find out missing DLL dependencies" << std::endl;

			return capturedManifestData;
		}

		// Make sure we start clean, uninstall the library first
		typedef HRESULT(__stdcall *DllUnregisterServerType)();
		DllUnregisterServerType dllUnregisterServer;

		dllUnregisterServer = reinterpret_cast<DllUnregisterServerType>(GetProcAddress(library, "DllUnregisterServer"));

		if (dllUnregisterServer)
		{
			dllUnregisterServer();
		}

		Interceptor interceptor;
		Interceptor::m_doTrace = doTrace;

		typedef HRESULT(__stdcall *DllRegisterServerType)();
		DllRegisterServerType dllRegisterServer;

		dllRegisterServer = reinterpret_cast<DllRegisterServerType>(GetProcAddress(library, "DllRegisterServer"));

		if (!dllRegisterServer)
		{
			std::wcout << L"Could not find DllRegisterServer function!" << std::endl;
			return capturedManifestData;
		}

		hr = dllRegisterServer();

		capturedManifestData = Interceptor::m_valuesList;

		// Got the information, ask the dll to remove the information from registry
		dllUnregisterServer();
	}
	catch (...)
	{
		std::wcout << L"Exception caught!" << std::endl;
	}

	if (FAILED(hr))
	{
		std::wcout << L"Registration failed, error: " << _com_error(hr).ErrorMessage() << L" error code: 0x" << std::hex << hr << std::endl;
	}

	return capturedManifestData;
}

void CreateManifestForModule(std::wstring &fileName, bool doTrace, std::wstring assemblyName, std::wstring assemblyVersion, DigestAlgo digestAlgos, std::wstring outputFileName,
	bool addArch, bool directoryMode = false)
{
	ManifestWriter manifest(assemblyName, assemblyVersion, addArch);

	manifest.AddFileSection(fileName, digestAlgos);
	manifest.ProcessData(fileName, CaptureManifestForModule(fileName, doTrace));

	manifest.WriteToFile(outputFileName);
}

void CreateManifestForDirectory(std::wstring &directoryPath, bool doTrace, std::wstring assemblyName, std::wstring assemblyVersion, DigestAlgo digestAlgos, std::wstring outputFileName,
	bool addArch)
{
	int size = GetCurrentDirectory(0, 0);

	std::wstring initialDirectory;
	initialDirectory.resize(size - 1);

	GetCurrentDirectory(size, &*initialDirectory.begin());
	if (!SetCurrentDirectory(directoryPath.c_str()))
	{
		std::wcout << L"SetCurrentDirectory failed!, GetLastError: 0x" << std::hex << GetLastError() << std::endl;
	}

	WIN32_FIND_DATA findData = { 0 };
	HANDLE findHandle = FindFirstFile(L"*.*", &findData);

	ManifestWriter manifest(assemblyName, assemblyVersion, addArch);
	while (FindNextFile(findHandle, &findData) != 0)
	{
		std::wstring file = findData.cFileName;

		if (file == L".." || file == L".")
		{
			continue;
		}

		Interceptor::ValuesListType data = CaptureManifestForModule(file, doTrace);

		if (data.size())
		{
			manifest.AddFileSection(file, digestAlgos);
			manifest.ProcessData(file, data);
		}
	}

	manifest.WriteToFile(outputFileName);

	FindClose(findHandle);
	SetCurrentDirectory(initialDirectory.c_str());
}

bool IsDirectory(const std::wstring& file)
{
	DWORD attributes = GetFileAttributes(file.c_str());
	return (attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}

void TrimSpaces(std::wstring &fileName)
{
	fileName.erase(fileName.find_last_not_of(L' ') + 1);
	fileName.erase(0, fileName.find_first_not_of(L' '));
}

void TrimQuotes(std::wstring &fileName)
{
	fileName.erase(fileName.find_last_not_of(L'\"') + 1);
	fileName.erase(0, fileName.find_first_not_of(L'\"'));
}

std::wstring ToLower(const std::wstring& str)
{
	std::wstring newString(str);
	std::transform(newString.begin(), newString.end(), newString.begin(), tolower);
	return newString;
}

std::wstring GetAssemblyVersion(std::wstring fileName, std::wstring assemblyVersion)
{
	std::wstring v = ToLower(assemblyVersion);
	bool getProductVersion = v == L"productversion" || v == L"productversion*";
	bool getFileVersion = v == L"fileversion" || v == L"fileversion*";
	bool skipBuildNumber = v[v.size() - 1] == L'*';
	if (!getProductVersion && !getFileVersion)
		return assemblyVersion;

	DWORD dummy;
	DWORD versionInfoSize;
	if (!(versionInfoSize = GetFileVersionInfoSize(fileName.c_str(), &dummy)))
	{
		std::wcout << "Failed getting version info for \"" << fileName << "\": " << GetLastError() << std::endl;
		return std::wstring();
	}
	std::vector<char> versionInfoData(versionInfoSize, 0);
	if (!GetFileVersionInfo(fileName.c_str(), 0, versionInfoSize, versionInfoData.data()))
	{
		std::wcout << "Failed getting version info for \"" << fileName << "\": " << GetLastError() << std::endl;
		return std::wstring();
	}
	VS_FIXEDFILEINFO *fixedFileInfo;
	UINT dummy2;
	if (!VerQueryValue(versionInfoData.data(), L"\\", reinterpret_cast<LPVOID*>(&fixedFileInfo), &dummy2))
	{
		std::wcout << "Failed getting version info (VerQueryValue) for \"" << fileName << "\": " << GetLastError() << std::endl;
		return std::wstring();
	}
	ULARGE_INTEGER ver = getProductVersion ?
		ULARGE_INTEGER({ fixedFileInfo->dwProductVersionLS, fixedFileInfo->dwProductVersionMS }) :
		ULARGE_INTEGER({ fixedFileInfo->dwFileVersionLS, fixedFileInfo->dwFileVersionMS });
	std::wstringstream sr;
	WORD buildNumber = skipBuildNumber ? 0 : LOWORD(ver.LowPart);
	sr << HIWORD(ver.HighPart) << L"." << LOWORD(ver.HighPart) << L"." << HIWORD(ver.LowPart) << L"." << buildNumber;
	return sr.str();
}


int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		PrintUsage();
		return 1;
	}

	bool doTrace = false;
	bool addArch = false;
	DigestAlgo digestAlgos = DigestAlgo::none;
	bool generateClientManifest = false;
	std::wstring fileName;

	std::wstring outputFileName;
	std::wstring assemblyVersion;
	std::wstring assemblyName;
	std::wstring clientFileName;

	bool batchMode = false;
	std::wstring batchFileName;

	bool directoryMode = false;
	std::wstring directoryPath;

	// Get the command line arguments
	for (int i = 1; i < argc; ++i)
	{
		std::wstring arg = argv[i];
		std::wstring argl = ToLower(arg.substr(1));

		// Hopefully the filename doesn't start with -
		if (arg[0] != L'-')
		{
			fileName = arg;

			TrimQuotes(fileName);
		}
		else if (argl == L"trace")
		{
			doTrace = true;
		}
		else if (argl.substr(0, 8) == L"version:")
		{
			assemblyVersion = arg.substr(9);
		}
		else if (argl.substr(0, 5) == L"name:")
		{
			assemblyName = arg.substr(6);

			TrimQuotes(assemblyName);
		}
		else if (argl == L"arch")
		{
			addArch = true;
		}
		else if (argl == L"hash")
		{
			digestAlgos |= DigestAlgo::sha1;
		}
		else if (argl == L"size")
		{
			digestAlgos |= DigestAlgo::size;
		}
		else if (argl == L"sha256")
		{
			digestAlgos |= DigestAlgo::sha256;
		}
		else if (argl.substr(0, 7) == L"client:")
		{
			generateClientManifest = true;
			clientFileName = arg.substr(8) + L".manifest";

			TrimQuotes(clientFileName);
		}
		else if (argl.substr(0, 6) == L"batch:")
		{
			batchMode = true;
			batchFileName = arg.substr(7);
		}
		else if (argl.substr(0, 4) == L"dir:")
		{
			directoryMode = true;
			directoryPath = arg.substr(5);

			TrimQuotes(directoryPath);
		}
		else
		{
			std::wcout << L"Parameter: " << arg << L" was not recognised" << std::endl;;
		}
	}

	// Processing and validation of command line arguments
	if (fileName.empty() && !batchMode && !directoryMode)
	{
		std::wcout << L"No com_dll was given!" << std::endl;
		return 1;
	}

	if (assemblyName.empty() && !batchMode && !directoryMode)
	{
		std::wstring localFile = fileName.substr(fileName.rfind(L'\\') + 1);
		assemblyName = localFile.substr(0, localFile.rfind(L'.')) + L".sxs";
	}
	else if (assemblyName.empty() && directoryMode)
	{
		assemblyName = directoryPath.substr(directoryPath.rfind(L'\\') + 1);
	}
	else if (!assemblyName.empty() && directoryMode)
	{
		std::wcout << L"Directory mode: assemblyName cannot be used!" << std::endl;
		return 1;
	}


	if (!assemblyName.empty() && batchMode)
	{
		std::wcout << L"Batch mode: assemblyName cannot be used!" << std::endl;
		return 1;
	}

	std::wstring clientFileNamePath = clientFileName.substr(0, clientFileName.rfind(L'\\') + 1);

	if (clientFileName.empty())
	{
		std::wstring fileNamePath = fileName.substr(0, fileName.rfind(L'\\') + 1);
		outputFileName = fileNamePath + assemblyName + L".manifest";
	}
	else if (directoryMode)
	{
		outputFileName = assemblyName + L".manifest";
	}
	else
	{
		outputFileName = clientFileNamePath + assemblyName + L".manifest";
	}


	if (assemblyVersion.empty() && !batchMode)
	{
		assemblyVersion = L"1.0.0.0";
	}
	else if (!assemblyVersion.empty() && batchMode)
	{
		std::wcout << L"Batch mode: assemblyVersion cannot be used!" << std::endl;
		return 1;
	}
	if (!batchMode)
		assemblyVersion = GetAssemblyVersion(fileName, assemblyVersion);

	if (directoryMode && batchMode)
	{
		std::wcout << L"Batch mode and Directory mode cannot be used together!" << std::endl;
		return 1;
	}

	// Generation of the manifest files
	std::vector<DependencyInfo> depencencyList;

	if (batchMode)
	{
		std::wifstream file(batchFileName.c_str());
		std::wstring line;

		while (std::getline(file, line))
		{
			std::wistringstream wis;
			wis.str(line);

			std::getline(wis, fileName, L'|');

			TrimSpaces(fileName);
			TrimQuotes(fileName);

			bool isDirectory = IsDirectory(fileName);

			if (!std::getline(wis, assemblyName, L'|') || isDirectory)
			{
				std::wstring localFile = fileName.substr(fileName.rfind(L'\\') + 1);
				assemblyName = localFile.substr(0, localFile.rfind(L'.'));

				if (!isDirectory)
				{
					assemblyName += +L".sxs";
				}
			}

			if (!std::getline(wis, assemblyVersion, L'|'))
			{
				assemblyVersion = L"1.0.0.0";
			}
			assemblyVersion = GetAssemblyVersion(fileName, assemblyVersion);

			outputFileName = clientFileNamePath + assemblyName + L".manifest";

			if (isDirectory)
			{
				CreateManifestForDirectory(fileName, doTrace, assemblyName, assemblyVersion,
					digestAlgos, outputFileName, addArch);
			}
			else
			{
				CreateManifestForModule(fileName, doTrace, assemblyName, assemblyVersion, digestAlgos,
					outputFileName, addArch);
			}

			depencencyList.push_back(DependencyInfo(assemblyName, assemblyVersion));
		}
	}
	else if (directoryMode)
	{
		CreateManifestForDirectory(directoryPath, doTrace, assemblyName, assemblyVersion, digestAlgos,
			outputFileName, addArch);

		depencencyList.push_back(DependencyInfo(assemblyName, assemblyVersion));
	}
	else
	{
		CreateManifestForModule(fileName, doTrace, assemblyName, assemblyVersion, digestAlgos,
			outputFileName, addArch);

		depencencyList.push_back(DependencyInfo(assemblyName, assemblyVersion));
	}

	if (generateClientManifest)
	{
		ManifestWriter::WriteClientManifest(clientFileName, depencencyList);
	}

	return 0;
}
