#include "Tools.h"

extern vector<PResultInfo> results;
extern ARG_CONFIG c;
std::mutex mtx;

string wstring2string(wstring wstr)
{
    string result;
    //��ȡ��������С��������ռ䣬��������С�°��ֽڼ����  
    int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
    char* buffer = new char[len + 1];
    //���ֽڱ���ת���ɶ��ֽڱ���  
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
    buffer[len] = '\0';
    //ɾ��������������ֵ  
    result.append(buffer);
    delete[] buffer;
    return result;
}

DWORD rvaToFOA(LPVOID buf, int rva)
{
    PIMAGE_DOS_HEADER  pDH = (PIMAGE_DOS_HEADER)buf;
    IMAGE_SECTION_HEADER* sectionHeader;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);

        sectionHeader = IMAGE_FIRST_SECTION(pNtH32);
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);

        sectionHeader = IMAGE_FIRST_SECTION(pNtH64);
    }

    while (sectionHeader->VirtualAddress != 0)
    {
        if (rva >= sectionHeader->VirtualAddress && rva < sectionHeader->VirtualAddress + sectionHeader->Misc.VirtualSize) {
            return rva - sectionHeader->VirtualAddress + sectionHeader->PointerToRawData;
        }

        sectionHeader++;
    }

    return 0;
}

DWORD foaToRVA(LPVOID lpBuffer, DWORD FOA) {
    PIMAGE_DOS_HEADER pDH = (PIMAGE_DOS_HEADER)lpBuffer;
    PIMAGE_FILE_HEADER pFile;
    PIMAGE_SECTION_HEADER pFirstSection;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;
        pFile = (PIMAGE_FILE_HEADER)((size_t)pNtH32 + 4);
        pFirstSection = PIMAGE_SECTION_HEADER((size_t)pOH32 + pFile->SizeOfOptionalHeader);

        if (FOA < pOH32->SizeOfHeaders || pOH32->FileAlignment == pOH32->SectionAlignment) {
            return FOA;
        }
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;
        pFile = (PIMAGE_FILE_HEADER)((size_t)pNtH64 + 4);
        pFirstSection = PIMAGE_SECTION_HEADER((size_t)pOH64 + pFile->SizeOfOptionalHeader);

        if (FOA < pOH64->SizeOfHeaders || pOH64->FileAlignment == pOH64->SectionAlignment) {
            return FOA;
        }
    }

    PIMAGE_SECTION_HEADER pSectionHeader = pFirstSection;

    for (int i = 0; pFile->NumberOfSections; i++) {
        if (FOA >= pSectionHeader->PointerToRawData &&
            FOA < pSectionHeader->PointerToRawData + pSectionHeader->SizeOfRawData) {
            /*��ȡFOA�ͽ����ļ���ַ��ƫ��*/
            DWORD relSectionFileAdd = FOA - pSectionHeader->PointerToRawData;
            /*ƫ�Ƽӽ�����VA�õ�RVA*/
            return relSectionFileAdd + pSectionHeader->VirtualAddress;
        }
        /*ָ����һ���ڱ�*/
        pSectionHeader = (PIMAGE_SECTION_HEADER)((size_t)pSectionHeader + IMAGE_SIZEOF_SECTION_HEADER);
    }

    return 0;
}

bool containsIgnoreCase(const std::string& str1, const std::string& str2) {
    std::string str1Lower = str1;
    std::string str2Lower = str2;

    // �������ַ���ת��ΪСд
    std::transform(str1Lower.begin(), str1Lower.end(), str1Lower.begin(), ::tolower);
    std::transform(str2Lower.begin(), str2Lower.end(), str2Lower.begin(), ::tolower);

    // ��ת������ַ����в���
    return str1Lower.find(str2Lower) != std::string::npos;
}

BYTE* readSectionData(BYTE* buffer, PDWORD rdataLength, char* secName) {
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(buffer);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "Invalid DOS header." << std::endl;
        return 0;
    }

    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(buffer) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "Invalid NT headers." << std::endl;
        return 0;
    }

    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        if (strcmp(secName, (char*)sectionHeader[i].Name) == 0) {
            *rdataLength = sectionHeader[i].SizeOfRawData;
            return reinterpret_cast<BYTE*>(buffer) + sectionHeader[i].PointerToRawData;
        }
    }

    return 0;
}

LPSTR ConvertWideToMultiByte(LPCWSTR wideString) {
    int wideLength = wcslen(wideString);

    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideString, wideLength, NULL, 0, NULL, NULL);

    LPSTR multiByteString = new char[bufferSize + 1];
    memset(multiByteString, 0, bufferSize + 1);

    WideCharToMultiByte(CP_ACP, 0, wideString, wideLength, multiByteString, bufferSize, NULL, NULL);

    return multiByteString;
}

std::string GetDirectoryFromPath(const std::string& filePath) {
    // ���ļ�·��ת��Ϊ·������
    std::filesystem::path pathObj(filePath);

    // �����ļ�����Ŀ¼���ַ�����ʾ
    return pathObj.parent_path().string();
}

void searchDll(BYTE* buffer, PResultInfo result, LPCWSTR filePath, char* dllsName, string fileDir) {
    DWORD rdataLength;
    char fileFullPath[0x255] = { 0 };
    strcat(fileFullPath, fileDir.c_str());
    int fileDirLength = fileDir.length();
    map<string, bool> postDllMap;
    char** secNames;
    int cnt = 0;

    if (c.isAllSectionSearch) {
        PIMAGE_DOS_HEADER pDH = (PIMAGE_DOS_HEADER)buffer;
        _IMAGE_SECTION_HEADER* sectionHeader;
        IMAGE_FILE_HEADER fh;
        
        if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
            sectionHeader = (_IMAGE_SECTION_HEADER*)((size_t)pNtH32 + sizeof(_IMAGE_NT_HEADERS));
            fh = pNtH32->FileHeader;
        }
        else {
            PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
            sectionHeader = (_IMAGE_SECTION_HEADER*)((size_t)pNtH64 + sizeof(_IMAGE_NT_HEADERS64));
            fh = pNtH64->FileHeader;
        }

        char* temp[0x10];
        secNames = (char**)malloc(sizeof(size_t) * fh.NumberOfSections);
        while (cnt < fh.NumberOfSections) {
            _IMAGE_SECTION_HEADER* section;
            section = (_IMAGE_SECTION_HEADER*)((size_t)sectionHeader + sizeof(_IMAGE_SECTION_HEADER) * cnt);
            temp[cnt++] = (char*)(section->Name);
        }
        secNames = temp;
    }
    else {
        char* temp[] = { ".rdata", ".rsrc" };
        secNames = temp;
        cnt = 2;
    }

    for (int i = 0; i < cnt; i++) {
        BYTE* rdata = readSectionData(buffer, &rdataLength, secNames[i]);
        if (rdata != 0) {
            DWORD vaule, vaule1;
            char* str;
            int strLength;
            char ch;
            int index = 0;

            for (int i = rdataLength - 8; i > 0; --i, index = 0) {
                vaule = *(PDWORD)((PBYTE)rdata + i);
                vaule1 = *(PDWORD)((PBYTE)rdata + i + 4);

                if (vaule == 0x6c6c642e)
                    index = 1;
                else if (vaule1 == 0x6c && vaule == 0x6c0064)
                    index = 2;

                if (index > 0) {
                    i -= index;
                    ch = rdata[i];
                    while (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-')) {
                        i -= index;
                        ch = rdata[i];
                    }

                    if (ch != 0)
                        continue;

                    if (index == 1)
                        str = (char*)(rdata + i + 1);
                    else
                        str = ConvertWideToMultiByte((wchar_t*)(rdata + i + 2));

                    strLength = strlen(str);
                    if (str[strLength - 1] != 'l')
                        continue;

                    memcpy(fileFullPath + fileDirLength, str, strLength + 1);

                    try {
                        if (postDllMap[str] == false && filesystem::exists(filesystem::path(fileFullPath)) && containsIgnoreCase(dllsName, str) == NULL) {
                            result->postLoadDlls.push_back(_strdup(str));
                            postDllMap[str] = true;
                        }
                    }
                    catch (const std::exception& e) {
                    }
                }
            }
        }
    }
}

bool hasWritePermission(const std::string& directoryPath) {
    std::string tempFilePath = directoryPath + "\\tmp";
    {
        std::lock_guard<std::mutex> lock(mtx);
        std::ofstream tempFile(tempFilePath);

        if (!tempFile.is_open()) {
            return false;  // �����ļ�ʧ�ܣ�Ŀ¼û��дȨ��
        }
        tempFile.close();
        std::filesystem::remove(tempFilePath);  // �����ļ�������ɾ��
    }
    return true;  // �����ļ��ɹ���Ŀ¼��дȨ��
}

void printImportTableInfo(BYTE* buffer, PResultInfo result, LPCWSTR filePath)
{
    const char* known_dlls[] = {"kernel32", "wow64cpu", "wowarmhw", "xtajit", "advapi32", "clbcatq", "combase", "COMDLG32", "coml2", "difxapi", "gdi32", "gdiplus", "IMAGEHLP", "IMM32", "MSCTF", "MSVCRT", "NORMALIZ", "NSI", "ole32", "OLEAUT32", "PSAPI", "rpcrt4", "sechost", "Setupapi", "SHCORE", "SHELL32", "SHLWAPI", "user32", "WLDAP32", "wow64cpu", "wow64", "wow64base", "wow64con", "wow64win", "WS2_32", "xtajit64"};
    string fileDir = GetDirectoryFromPath(ConvertWideToMultiByte(filePath)) + "\\";
    result->fileDir = fileDir;

    if (hasWritePermission(fileDir))
        result->isWrite = true;
    else
        result->isWrite = false;

    PIMAGE_DOS_HEADER  pDH = (PIMAGE_DOS_HEADER)buffer;
    IMAGE_DATA_DIRECTORY directory;
    DWORD THUNK_DATA_SIZE;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        directory = pOH32->DataDirectory[1];
        THUNK_DATA_SIZE = 4;
        result->bit = 32;
        result->isGUIWindow = pOH32->Subsystem == 3 ? 0 : 1;
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        directory = pOH64->DataDirectory[1];
        THUNK_DATA_SIZE = 8;
        result->bit = 64;
        result->isGUIWindow = pOH64->Subsystem == 3 ? 0 : 1;
    }

    PIMAGE_IMPORT_DESCRIPTOR ImportTable = PIMAGE_IMPORT_DESCRIPTOR(rvaToFOA(buffer, directory.VirtualAddress) + buffer);
    //��ȡ����dll����
    char* dllsName = (char*)malloc(0x2000);
    memset(dllsName, 0, 0x2000);
    while (ImportTable->Name)
    {
        char* pName = (char*)(rvaToFOA(buffer, ImportTable->Name) + buffer);
        strcat(dllsName, pName);
        ImportTable++;
    }

    bool isSearchLoadLibrary = false;
    bool isSearchWindow = false;

    ImportTable = PIMAGE_IMPORT_DESCRIPTOR(rvaToFOA(buffer, directory.VirtualAddress) + buffer);
    while (ImportTable->Name)
    {
        char* pName = (char*)(rvaToFOA(buffer, ImportTable->Name) + buffer);

        PIMAGE_THUNK_DATA INT = PIMAGE_THUNK_DATA(rvaToFOA(buffer, ImportTable->OriginalFirstThunk) + buffer);
        PIMAGE_IMPORT_BY_NAME temp = { 0 };
        int count = 0;

        if (ImportTable->OriginalFirstThunk == 0)
            break;
        
        while (INT->u1.AddressOfData)//���������������һ����ʱ���ǻ�Ϊ0������������һ���ͺ�
        {
            if (!(INT->u1.Ordinal & 0x80000000))
            {
                temp = (PIMAGE_IMPORT_BY_NAME)(rvaToFOA(buffer, INT->u1.AddressOfData) + buffer);
                if ((BYTE*)temp == buffer) {
                    break;
                }
                else if (!isSearchLoadLibrary && containsIgnoreCase(temp->Name, "loadlibrary") != NULL)
                {
                    isSearchLoadLibrary = true;
                    searchDll(buffer, result, filePath, dllsName, fileDir);
                }
                else if (!isSearchWindow && (containsIgnoreCase(temp->Name, "CreateDialogParam") != NULL || containsIgnoreCase(temp->Name, "CreateWindow") != NULL || containsIgnoreCase(temp->Name, "CreateProcess") != NULL))
                {
                    isSearchWindow = true;
                    result->isCreateWindow = true;
                }
            }
            INT = PIMAGE_THUNK_DATA((PBYTE)INT + THUNK_DATA_SIZE);//INT��INT����������
            count++;
        }

        char fileFullPath[0x255] = { 0 };
        strcat(fileFullPath, fileDir.c_str());
        strcat(fileFullPath, pName);

        try {
            if (filesystem::exists(filesystem::path(fileFullPath)))
                result->preLoadDlls.push_back(_strdup(pName));
        }
        catch (const std::exception& e) {
            
        }

        ImportTable++;
    }

    free(dllsName);

    return;
}

size_t memorySum(LPVOID shellcode, DWORD fileSize)
{
    size_t sum = 0;
    for (int i = 0; i < fileSize; i+=0x10)
        sum += *(size_t*)((PBYTE)shellcode + i);
    return sum;
}

BOOL VerifyFileSignature(LPCWSTR filePath) {
    DWORD dwEncoding, dwContentType, dwFormatType;
    HCERTSTORE hStore = NULL;
    HCRYPTMSG hMsg = NULL;
    BOOL bResult = FALSE;

    // Open the file and get the file handle
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    // Get the size of the file
    DWORD dwFileSize = GetFileSize(hFile, NULL);

    // Read the file into a buffer
    BYTE* pbFile = (BYTE*)malloc(dwFileSize);
    DWORD dwBytesRead;
    if (!ReadFile(hFile, pbFile, dwFileSize, &dwBytesRead, NULL)) {
        CloseHandle(hFile);
        free(pbFile);
        return FALSE;
    }

    // Verify the signature
    bResult = CryptQueryObject(CERT_QUERY_OBJECT_FILE, filePath, CERT_QUERY_CONTENT_FLAG_ALL,
        CERT_QUERY_FORMAT_FLAG_ALL, 0, &dwEncoding, &dwContentType,
        &dwFormatType, &hStore, &hMsg, NULL);
    if (!bResult) {
        CloseHandle(hFile);
        free(pbFile);
        return FALSE;
    }

    PIMAGE_DOS_HEADER  pDH = (PIMAGE_DOS_HEADER)pbFile;
    IMAGE_DATA_DIRECTORY directory;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        directory = pOH32->DataDirectory[1];
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        directory = pOH64->DataDirectory[1];
    }
    //û�е����ĳ���
    if (directory.VirtualAddress == 0) {
        CloseHandle(hFile);
        free(pbFile);
        return FALSE;
    }
    
    ResultInfo* result = new ResultInfo;
    result->filePath = wstring2string(filePath);
    result->isCreateWindow = false;
    result->isSystemDll = false;
    result->fileHash = memorySum(pbFile, dwFileSize);
    
    printImportTableInfo(pbFile, result, filePath);

    if (result->preLoadDlls.size() > 0 || result->postLoadDlls.size() > 0) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            results.push_back(result);
        }
    }

    // Clean up resources
    if (hMsg != NULL)
        CryptMsgClose(hMsg);
    if (hStore != NULL)
        CertCloseStore(hStore, CERT_CLOSE_STORE_FORCE_FLAG);
    CloseHandle(hFile);
    free(pbFile);

    return TRUE;
}

int readFileContext(string path, char** contexts)
{
    ifstream inFile(path, std::ios::binary);
    if (!inFile) {
        printf("%s open fail\n", path.c_str());
        return -1;
    }

    inFile.seekg(0, std::ios::end);
    std::streamsize payloadFileSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    *contexts = new char[payloadFileSize];

    if (!inFile.read(*contexts, payloadFileSize)) {
        printf("%s payloadBuffer read fail\n", path.c_str());
        delete[] contexts;
        return -1;
    }

    inFile.close();

    return payloadFileSize;
}

bool saveFile(string filePath, char* buffer, DWORD fileSize)
{
    std::ofstream outFile;
    outFile.open(filePath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        printf("Failed to open file for writing.\n");
        return false;
    }
    outFile.write(buffer, fileSize);
    outFile.close();

    return true;
}

void str_to_lower(char* str) {
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
}

DWORD getImportFuncAddr(char* buffer, PIMAGE_IMPORT_DESCRIPTOR ImportTable, char* name, int bit, bool isExeFile, bool isUserDll) {
    int THUNK_DATA_SIZE = 4;
    if (bit == 64)
        THUNK_DATA_SIZE = 8;

    PIMAGE_THUNK_DATA INT = PIMAGE_THUNK_DATA(rvaToFOA(buffer, ImportTable->OriginalFirstThunk) + buffer);
    //������ַ
    PIMAGE_THUNK_DATA IAT = PIMAGE_THUNK_DATA(rvaToFOA(buffer, ImportTable->FirstThunk) + buffer);
    PIMAGE_IMPORT_BY_NAME temp = { 0 };
    int count = 0;
    int index = 0;
    int hookNameLength = strlen(name);
    char* str = "";

    while (INT->u1.AddressOfData)//���������������һ����ʱ���ǻ�Ϊ0������������һ���ͺ�
    {
        if (!(INT->u1.Ordinal & 0x80000000))
        {
            temp = (PIMAGE_IMPORT_BY_NAME)(rvaToFOA(buffer, INT->u1.AddressOfData) + buffer);
            if ((char*)temp == buffer)
                break;
            if (containsIgnoreCase(temp->Name, name) != 0)
            {
                if (isExeFile) {
                    memset(temp->Name, 0, strlen(temp->Name));

                    if (isUserDll)
                        strcpy(temp->Name, "ShowWindow");
                    else
                        strcpy(temp->Name, "GetLastError");
                }
                else
                    return foaToRVA(buffer, (size_t)IAT - (size_t)buffer + count * THUNK_DATA_SIZE);
            }

            if (strlen(temp->Name) >= hookNameLength) {
                index = count;
                str = temp->Name;
            }
        }
        INT = PIMAGE_THUNK_DATA((PBYTE)INT + THUNK_DATA_SIZE);//INT��INT����������
        count++;
    }

    if (!isExeFile && index > 0) {
        memset(str, 0, strlen(str));
        strcpy(str, name);
        return foaToRVA(buffer, (size_t)IAT - (size_t)buffer + index * THUNK_DATA_SIZE);
    }

    return 0;
}

void repairReloc(char* buffer, DWORD* dataRva, int count, DWORD isClearEnd)
{
    PIMAGE_DOS_HEADER  pDH = (PIMAGE_DOS_HEADER)buffer;
    PIMAGE_DATA_DIRECTORY pRelocDir;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        pRelocDir = &(pOH32->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        pRelocDir = &(pOH64->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]);
    }

    int index = 0;

    if (pRelocDir->VirtualAddress != 0 && pRelocDir->Size != 0) {
        PIMAGE_BASE_RELOCATION pRelocBlock = (PIMAGE_BASE_RELOCATION)(buffer + rvaToFOA(buffer, pRelocDir->VirtualAddress));
        while (pRelocBlock->VirtualAddress != 0 && pRelocBlock->SizeOfBlock != 0) {
            WORD* pRelocEntry = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pRelocBlock) + sizeof(IMAGE_BASE_RELOCATION));

            DWORD numRelocs = (pRelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            if (numRelocs > 0x1000) //������Щ���򲻰�Լ�������±���
                break;

            for (DWORD i = 0; i < numRelocs; i++) {
                WORD relocType = (pRelocEntry[i] & 0xF000) >> 12;
                WORD relocOffset = pRelocEntry[i] & 0x0FFF;

                if (isClearEnd > 0)
                {
                    if (dataRva[0] <= pRelocBlock->VirtualAddress + relocOffset && pRelocBlock->VirtualAddress + relocOffset <= isClearEnd)
                        pRelocEntry[i] = pRelocEntry[i] & 0xfff;
                }
                else
                {
                    if (pRelocBlock->VirtualAddress + relocOffset >= dataRva[index])
                    {
                        if (index < count)
                        {
                            pRelocEntry[i] = (dataRva[index] % 0x1000) | 0x3000;
                            index++;
                        }
                    }
                }
            }
            pRelocBlock = reinterpret_cast<PIMAGE_BASE_RELOCATION>((reinterpret_cast<BYTE*>(pRelocBlock)) + pRelocBlock->SizeOfBlock);
        }
    }
}

int fixFile(string targetFilePath, DWORD exitCode)
{
    DWORD attributes = GetFileAttributesA(targetFilePath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        attributes &= ~FILE_ATTRIBUTE_READONLY; // ���ֻ������
        SetFileAttributesA(targetFilePath.c_str(), attributes);
    }

    bool isExeFile = targetFilePath.back() == 'e' ? true : false;

    char* targetBuffer;
    int fileSize = readFileContext(targetFilePath, &targetBuffer);
    if (fileSize == -1)
        return false;

    PIMAGE_DOS_HEADER pDH = (PIMAGE_DOS_HEADER)targetBuffer;
    IMAGE_DATA_DIRECTORY importDirectory;
    int bit;
    DWORD imageBase = 0;
    DWORD oep = 0;

    if (*(PWORD)pDH != 0x5a4d)
        return false;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        importDirectory = pOH32->DataDirectory[1];
        bit = 32;
        imageBase = pOH32->ImageBase;
        oep = pOH32->AddressOfEntryPoint;
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        importDirectory = pOH64->DataDirectory[1];
        bit = 64;
        oep = pOH64->AddressOfEntryPoint;
    }

    size_t oep_foa_addr = (size_t)targetBuffer + rvaToFOA(targetBuffer, oep);
    DWORD clear[1] = { oep };
    DWORD improtFunc;
    DWORD addr;
    bool isHook = false;

    PIMAGE_IMPORT_DESCRIPTOR ImportTable = PIMAGE_IMPORT_DESCRIPTOR((size_t)targetBuffer + rvaToFOA(targetBuffer, importDirectory.VirtualAddress));
    PIMAGE_IMPORT_DESCRIPTOR tmp_ImportTable = ImportTable;

    const char* names[] = { "kernel32.dll", "ntdll.dll", "api-ms-win-", "msvcrt.dll", "oleaut32.dll", "user32.dll", "ole32.dll", "vcruntime140.dll", "advapi32.dll", "gdiplus.dll", "shell32.dll", "shlwapi.dll", "comctl32.dll" };
    size_t indexs[100] = { 0 };
    int count = 0;
    DWORD nameSize = sizeof(names) / sizeof(size_t);

    while (ImportTable->Name)
    {
        char* pName = rvaToFOA(targetBuffer, ImportTable->Name) + targetBuffer;
        str_to_lower(pName);

        if (isExeFile) {
            //�滻���������ڵĺ���
            if (strstr(pName, "user32.dll") != NULL) {
                getImportFuncAddr(targetBuffer, ImportTable, "Create", bit, true, true);
            }
            else if (strstr(pName, "kernel32.dll") != NULL) {
                getImportFuncAddr(targetBuffer, ImportTable, "CreateProcess", bit, true, false);
            }
        }
        else {
            for (int i = 0; i < nameSize; i++)
            {
                if (strstr(pName, names[i]) != NULL)
                    indexs[count++] = (size_t)ImportTable;
            }

            DWORD offset = 0;
            if (!isHook && strstr(pName, "kernel32.dll") != NULL) {
                addr = getImportFuncAddr(targetBuffer, ImportTable, "ExitProcess", bit, false, NULL);
                if (addr != 0) {
                    if (bit == 64) {
                        unsigned char hook_data[] = { 0x48, 0x83, 0xEC, 0x28, 0xB9, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0xC8, 0xEF, 0x00, 0x00 };
                        improtFunc = addr - (oep + sizeof(hook_data));
                        memcpy((char*)oep_foa_addr, hook_data, sizeof(hook_data));
                        repairReloc(targetBuffer, clear, 0, oep + sizeof(hook_data));
                        offset = 4;
                    }
                    else {
                        unsigned char hook_data[] = { 0x83, 0xEC, 0x14, 0x68, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0x08, 0x20, 0x40, 0x00 };
                        improtFunc = imageBase + addr;
                        memcpy((char*)oep_foa_addr, hook_data, sizeof(hook_data));
                        repairReloc(targetBuffer, clear, 0, oep + sizeof(hook_data));
                        offset = 3;

                        DWORD dataRva[] = { oep + 7 + offset };
                        repairReloc(targetBuffer, dataRva, 1, 0);
                    }

                    *(PDWORD)(oep_foa_addr + 1 + offset) = exitCode;
                    *(PDWORD)(oep_foa_addr + 7 + offset) = improtFunc;
                    isHook = true;
                }
            }
            else if (!isHook && strstr(pName, "ntdll.dll") != NULL) {
                addr = getImportFuncAddr(targetBuffer, ImportTable, "NtTerminateProcess", bit, false, NULL);
                if (addr != 0) {
                    if (bit == 64) {
                        unsigned char hook_data[] = { 0x48, 0x83, 0xEC, 0x28, 0xBA, 0x00, 0x00, 0x00, 0x00, 0xB9, 0xff, 0xff, 0xff, 0xff, 0xFF, 0x15, 0xC8, 0xEF, 0x00, 0x00 };
                        improtFunc = addr - (oep + sizeof(hook_data));
                        memcpy((char*)oep_foa_addr, hook_data, sizeof(hook_data));
                        repairReloc(targetBuffer, clear, 0, oep + sizeof(hook_data));
                        offset = 4;
                    }
                    else {
                        unsigned char hook_data[] = { 0x83, 0xEC, 0x14, 0x68, 0x00, 0x00, 0x00, 0x00, 0x68, 0xff, 0xff, 0xff, 0xff, 0xFF, 0x15, 0x08, 0x20, 0x40, 0x00 };
                        improtFunc = imageBase + addr;
                        memcpy((char*)oep_foa_addr, hook_data, sizeof(hook_data));
                        repairReloc(targetBuffer, clear, 0, oep + sizeof(hook_data));
                        offset = 3;

                        DWORD dataRva[] = { oep + 12 + offset };
                        repairReloc(targetBuffer, dataRva, 1, 0);
                    }

                    *(PDWORD)(oep_foa_addr + 1 + offset) = exitCode;
                    *(PDWORD)(oep_foa_addr + 12 + offset) = improtFunc;
                    isHook = true;
                }
            }
        }
        ImportTable++;
    }

    if (!isExeFile) {
        for (int i = 0; i < count; i++)
        {
            memcpy(tmp_ImportTable, (char*)indexs[i], 0x14);
            tmp_ImportTable++;
        }
        memset(tmp_ImportTable, 0, 0x14);
    }

    bool isSucc = saveFile(targetFilePath, targetBuffer, fileSize);
    
    delete[] targetBuffer;

    return isSucc;
}

bool fixExportTable(string targetFilePath, string sourceFilePath) {
    char* targetBuffer;
    DWORD fileSize = readFileContext(targetFilePath, &targetBuffer);

    PIMAGE_DOS_HEADER pDH = (PIMAGE_DOS_HEADER)targetBuffer;
    PIMAGE_NT_HEADERS pNtH = (PIMAGE_NT_HEADERS)((DWORD)pDH + pDH->e_lfanew);
    PIMAGE_OPTIONAL_HEADER pOH = &pNtH->OptionalHeader;
    IMAGE_DATA_DIRECTORY exportDirectory;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        exportDirectory = pOH32->DataDirectory[0];
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        exportDirectory = pOH64->DataDirectory[0];
    }

    IMAGE_EXPORT_DIRECTORY* exportDir = (IMAGE_EXPORT_DIRECTORY*)(targetBuffer + rvaToFOA(targetBuffer, exportDirectory.VirtualAddress));

    DWORD* nameRVAs = (DWORD*)(targetBuffer + rvaToFOA(targetBuffer, exportDir->AddressOfNames));

    char* sourceBuffer;
    readFileContext(sourceFilePath, &sourceBuffer);

    pDH = (PIMAGE_DOS_HEADER)sourceBuffer;
    pNtH = (PIMAGE_NT_HEADERS)((size_t)pDH + pDH->e_lfanew);
    pOH = &pNtH->OptionalHeader;

    if (*(PWORD)((size_t)pDH + pDH->e_lfanew + 0x18) == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        PIMAGE_NT_HEADERS32  pNtH32 = PIMAGE_NT_HEADERS32((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER32 pOH32 = &pNtH32->OptionalHeader;

        exportDirectory = pOH32->DataDirectory[0];
    }
    else {
        PIMAGE_NT_HEADERS64 pNtH64 = PIMAGE_NT_HEADERS64((size_t)pDH + pDH->e_lfanew);
        PIMAGE_OPTIONAL_HEADER64 pOH64 = &pNtH64->OptionalHeader;

        exportDirectory = pOH64->DataDirectory[0];
    }

    IMAGE_EXPORT_DIRECTORY* exportDir_source = (IMAGE_EXPORT_DIRECTORY*)(sourceBuffer + rvaToFOA(sourceBuffer, exportDirectory.VirtualAddress));

    DWORD* nameRVAs_source = (DWORD*)(sourceBuffer + rvaToFOA(sourceBuffer, exportDir_source->AddressOfNames));

    if (exportDir_source->NumberOfNames > 100) {
        delete[] targetBuffer;
        delete[] sourceBuffer;
        return false;
    }

    for (int i = 0; i < exportDir_source->NumberOfNames; i++)
    {
        DWORD nameRVA_source = nameRVAs_source[i];
        char* exportFunctionName_source = sourceBuffer + rvaToFOA(sourceBuffer, nameRVA_source);

        DWORD nameRVA = nameRVAs[i];
        char* exportFunctionName = targetBuffer + rvaToFOA(targetBuffer, nameRVA);

        memcpy(exportFunctionName, exportFunctionName_source, strlen(exportFunctionName_source) + 1);
    }

    saveFile(targetFilePath, targetBuffer, fileSize);

    delete[] targetBuffer;
    delete[] sourceBuffer;

    return true;
}

std::string GetCurrentPath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

std::string GenerateRandomFolderName() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 1000000);

    // ��������ַ�����Ϊ�ļ�����
    const char charset[] = "ghijklmnopq_rstuvwxyz0123456789abcdef";
    const size_t charsetSize = sizeof(charset) - 1;
    const size_t folderNameLength = 10; // �ļ���������
    std::string folderName;
    for (size_t i = 0; i < folderNameLength; ++i) {
        folderName += charset[dist(gen) % charsetSize];
    }
    return folderName;
}

string CreateRandomFolder(const std::string& basePath) {
    std::string randomFolderName = GenerateRandomFolderName();
    std::string folderPath = basePath + "\\" + randomFolderName;

    if (CreateDirectoryA(folderPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return folderPath;
    }

    return 0;
}

std::wstring ConvertToWideString(const char* input) {
    int length = strlen(input) + 1;
    int requiredLength = MultiByteToWideChar(CP_ACP, 0, input, length, NULL, 0);
    wchar_t* buffer = new wchar_t[requiredLength];
    MultiByteToWideChar(CP_ACP, 0, input, length, buffer, requiredLength);
    std::wstring result(buffer);
    delete[] buffer;
    return result;
}

string CopyFileToFolder(const std::string& sourceFilePath, const std::string& targetFolderPath, bool isNeedHook, DWORD exitCode) {
    std::string targetFilePath = targetFolderPath + "\\" + sourceFilePath.substr(sourceFilePath.find_last_of("\\/") + 1);

    CopyFileA(sourceFilePath.c_str(), targetFilePath.c_str(), FALSE);

    if (isNeedHook) {
        std::lock_guard<std::mutex> lock(mtx);
        bool isSucc = fixFile(targetFilePath, exitCode);
        if (!isSucc)
            return "";
    }

    return targetFilePath;
}

bool DeleteDirectory(const string& path) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
            string filePath = path + "\\" + findData.cFileName;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // �ݹ�ɾ����Ŀ¼
                if (!DeleteDirectory(filePath)) {
                    FindClose(hFind);
                    return false;
                }
            }
            else {
                // ɾ���ļ�
                DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
                !SetFileAttributesA(filePath.c_str(), fileAttributes & ~FILE_ATTRIBUTE_READONLY);

                if (!DeleteFileA(filePath.c_str())) {
                    FindClose(hFind);
                    return false;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

    // ɾ����Ŀ¼
    if (!RemoveDirectoryA(path.c_str())) {
        return false;
    }

    return true;
}

int TestCreateProcess(string runFilePath, DWORD dwMilliseconds) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(
        nullptr,                        // ָ���ִ���ļ�����ָ�루�����nullptr��ʾʹ�õ�ǰ��ִ���ļ���
        (char*)runFilePath.c_str(),     // ��ִ���ļ���·��
        nullptr,                        // ��ȫ����
        nullptr,                        // ��ȫ����
        FALSE,                          // ָ���Ƿ�̳о��
        CREATE_NO_WINDOW,               // ָ��������ʾ��ʽ������ָ��Ϊ�޴��ڣ�
        nullptr,                        // ָ���½��̵Ļ�����
        nullptr,                        // ָ���½��̵ĵ�ǰĿ¼
        &si,                            // STARTUPINFO �ṹ��
        &pi)) {                         // �����½�����Ϣ�� PROCESS_INFORMATION �ṹ��
        return 0;
    }

    // �ȴ����̽���
    WaitForSingleObject(pi.hProcess, dwMilliseconds);

    TerminateProcess(pi.hProcess, 0);

    // ��ȡ���̵��˳���
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);  

    // �رս��̺��߳̾��
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}

void RunPE(PResultInfo result) {
    std::string currentPath = GetCurrentPath();

    string folderPath = CreateRandomFolder(currentPath);

    string runFilePath = CopyFileToFolder(result->filePath, folderPath, result->isCreateWindow, NULL);

    map<DWORD, std::string> hookDllMap;
    bool flag;
    DWORD exitCode = 0x22222222;

    for (const auto& dll : result->preLoadDlls) {
        CopyFileToFolder(result->fileDir + dll, folderPath, true, exitCode);
        hookDllMap[exitCode] = dll;
        exitCode++;
    }

    exitCode = 0x33333333;
    for (const auto& dll : result->postLoadDlls) {
        CopyFileToFolder(result->fileDir + dll, folderPath, true, exitCode);
        hookDllMap[exitCode] = dll;
        exitCode++;
    }

    DWORD retExitCode = TestCreateProcess(runFilePath, 1500);
    result->exploitDllPath = hookDllMap[retExitCode];

    if (result->exploitDllPath != "") {
        if (retExitCode >= 0x33333333)
            result->loadType = 2;
        else
            result->loadType = 1;

        string hookFilePath = currentPath + "\\TestLoad_x86.dll";
        if (result->bit == 64)
            hookFilePath = currentPath + "\\TestLoad_x64.dll";

        string targetFilePath = folderPath + "\\" + result->exploitDllPath;
        CopyFileA(hookFilePath.c_str(), targetFilePath.c_str(), FALSE);
        bool isSucc = fixExportTable(targetFilePath, result->fileDir + result->exploitDllPath);

        if (isSucc) {
            targetFilePath = folderPath + "\\" + result->filePath.substr(result->filePath.find_last_of("\\/") + 1);

            TestCreateProcess(targetFilePath, 3000);
        }

        if (!std::filesystem::exists(folderPath +  "\\test.txt"))
            result->exploitDllPath = "";

        if (std::filesystem::exists("C:\\Windows\\SysWOW64\\" + result->exploitDllPath) || std::filesystem::exists("C:\\Windows\\System32\\" + result->exploitDllPath))
            result->isSystemDll = true;
    }
    
    if (c.isSaveFile) {
        if(result->exploitDllPath == "" || (c.isPassSystemDll && result->isSystemDll))
            while (!DeleteDirectory(folderPath.c_str())) {}
    }
    else
        while (!DeleteDirectory(folderPath.c_str())) {}
}