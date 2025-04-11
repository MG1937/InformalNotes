#include "hooker.h"

UINT PEHandler::readFile(LPCSTR fileName) {
	HANDLE hPeFile = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hPeFile == INVALID_HANDLE_VALUE) {
		printf_s("CreateFile Failed %s\n", fileName);
		return -1;
	}

	DWORD fSize = GetFileSize(hPeFile, NULL);
	this->peSize = fSize;

	if (fSize <= 0) {
		printf_s("GetFileSize Failed!\n");
		return -1;
	}

	LPVOID lpBuffer = malloc(fSize);

	if (!ReadFile(hPeFile, lpBuffer, fSize, NULL, NULL)) {
		printf_s("ReadFile Failed %d\n", GetLastError());
		CloseHandle(hPeFile);
		return -1;
	}

	CloseHandle(hPeFile);
	setPEBuffer(lpBuffer);
	setFileName(fileName);

	return fSize;
}

VOID PEHandler::updateNumberOfSections(WORD number) {
	this->peNumberOfSections = number;
	// change ntheader in memory in runtime (Dangerous!)
	getNtImageHeader()->FileHeader.NumberOfSections = number;
	printf_s("Change NumberOfSections to %d of lpPeBuffer in memory runtime\n", number);
}

VOID PEHandler::initAnalyse() {
	PIMAGE_NT_HEADERS ntHeader = ImageNtHeader(lpPeBuffer);
	setNtImageHeader(ntHeader);
	this->peABI = ntHeader->OptionalHeader.Magic;
	this->peFileAlignment = ntHeader->OptionalHeader.FileAlignment;
	this->peSectionAlignment = ntHeader->OptionalHeader.SectionAlignment;
	this->peNumberOfSections = ntHeader->FileHeader.NumberOfSections;
	
	printf_s(
		"PE File Magic 0x%x\n"
		"PE FileAlignment 0x%x\n"
		"PE SectionAlignment 0x%x\n"
		"PE NumberOfSections 0x%x\n", getPEAbi(), getFileAlignment(), getSectionAlignment(), getNumberOfSections());
}

SECTION_INFO PEHandler::addSection(LPCSTR sectionName, LPVOID sectionContent, DWORD sectionSize) {
	if (strlen(sectionName) > IMAGE_SIZEOF_SHORT_NAME) {
		printf_s("Section Name too long: %s\n", sectionName);
		return INVALID_SEC_INFO;
	}

	if (sectionSize <= 0 || sectionContent == nullptr) {
		printf_s("Failed to addSection, sectionSize <= 0 or sectionContent invalid!\n");
		return INVALID_SEC_INFO;
	}

	SECTION_INFO sectionInfo;
	sectionInfo.ContentBuffer = sectionContent;
	sectionInfo.ContentSize = sectionSize;

	PIMAGE_SECTION_HEADER section = &IMAGE_FIRST_SECTION(getNtImageHeader())[getNumberOfSections() - 1];
	printf_s("Last Section Of current PE: %s\n", section->Name);

	DWORD leftSpace = getNtImageHeader()->OptionalHeader.SizeOfHeaders -
		(((DWORD_PTR)section) + sizeof(IMAGE_SECTION_HEADER) - (DWORD)getPEBuffer());
	printf_s("Left Space for Section: 0x%x\n", leftSpace);
	if (leftSpace < sizeof(IMAGE_SECTION_HEADER)) {
		printf_s("Failed! Not Enough for next Section!!\n");
		return INVALID_SEC_INFO;
	}

	// Buggy! We assume the last section's VA is biggest
	PIMAGE_SECTION_HEADER newSection = (PIMAGE_SECTION_HEADER)malloc(sizeof(IMAGE_SECTION_HEADER));
	if (newSection == nullptr) {
		printf_s("New Section Alloced Failed!\n");
		return INVALID_SEC_INFO;
	}

	memset(newSection, 0x0, sizeof(IMAGE_SECTION_HEADER));
	memcpy(newSection + offsetof(IMAGE_SECTION_HEADER, Name), sectionName, strlen(sectionName));
	
	newSection->Misc.VirtualSize = sectionSize;
	newSection->SizeOfRawData = sectionSize;

	// Handle for section memory alignment
	DWORD newSectionVA = ALIGNMENT(section->VirtualAddress + section->SizeOfRawData, getSectionAlignment());
	newSection->VirtualAddress = newSectionVA;
	sectionInfo.SectionVAAddr = newSectionVA;

	// Handle for file alignment
	DWORD aligned = ALIGNMENT(getPEFileSize(), getFileAlignment());
	newSection->PointerToRawData = aligned;
	sectionInfo.SectionFOAAddr = aligned;

	DWORD needAlign = aligned - getPEFileSize();
	if (needAlign != 0) {
		printf_s("Need to handle file align, still 0x%x byte need to fill!\n", needAlign);
		sectionInfo.AlignSize = needAlign;
	}

	// Make sure section permission is RWX
	newSection->Characteristics = 0xe2000040;

	// Adjust PE Buffer
	// 1. update NumberOfSections
	// 2. Increase SizeOfImage
	// 3. Add new section to SectionHeaders
	updateNumberOfSections(getNumberOfSections() + 1);
	getNtImageHeader()->OptionalHeader.SizeOfImage += ALIGNMENT(sectionSize, getSectionAlignment());
	IMAGE_FIRST_SECTION(getNtImageHeader())[getNumberOfSections() - 1] = *newSection;

	// Insert new Section Content to PE Buffer
	LPVOID newPEBuffer = realloc(getPEBuffer(), aligned + sectionSize);
	if (newPEBuffer == nullptr) {
		printf_s("New PE Buffer Alloced Failed!\n");
		exit(0); // No RollBack! Just exit!
		return INVALID_SEC_INFO;
	}
	memcpy((PBYTE)newPEBuffer + aligned, sectionContent, sectionSize);

	// Relocate PE Buffer, reAnalyse PE File
	this->peSize = aligned + sectionSize;
	setPEBuffer(newPEBuffer);
	initAnalyse();

	printf_s(
		"New Section Added\n"
		"New Section Name %s\n"
		"New Section VA 0x%x\n"
		"New Section FOA 0x%x\n"
		"New PE File Size 0x%x\n", sectionName, newSectionVA, aligned, getPEFileSize());

	return sectionInfo;
}

VOID PEHandler::outputPEFile(LPCSTR fileName) {
	HANDLE hFile = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf_s("CreateFile Failed! %d\n", GetLastError());
		return;
	}

	if (!WriteFile(hFile, getPEBuffer(), getPEFileSize(), NULL, NULL)) {
		printf_s("WriteFile Failed! %d\n", GetLastError());
		return;
	}

	printf_s("Success Output PE File to %s\n", fileName);
}

int main(int argc, char* argv[]) {

	if (argc == 1) {
		std::cout << "Command Format Incorrect\n";
		//return 0;
	}
	
	LPSTR PE = argv[1];

#ifdef _DEBUG
	PE = (LPSTR)"E:/IONinja/ioninja.exe";
#endif // _DEBUG

	PEHandler peHandler = PEHandler::getInstance();

	UINT peSize = peHandler.readFile(PE);
	printf_s("Buffer Size %d for PE %s\n", peSize, PE);

	peHandler.initAnalyse();

#ifdef _DEBUG
	LPVOID debugSection = malloc(30);
	memset(debugSection, 0x90, 30);
	((PBYTE)debugSection)[29] = 0xcc;
	peHandler.addSection(".hoked", debugSection, 30);
	peHandler.outputPEFile("pepatched.exe");
#endif // _DEBUG
	return 0;
}