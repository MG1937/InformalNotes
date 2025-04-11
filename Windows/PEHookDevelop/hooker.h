#pragma once
#pragma comment(lib, "Dbghelp.lib")
#include <iostream>
#include <windows.h>
#include <dbghelp.h>

#define INVALID_SEC_INFO SECTION_INFO {true}

// tool macro, align memory for new section
#define ALIGNMENT( ADDR, ALIGN ) \
	(((DWORD)(ADDR) + (DWORD)(ALIGN) - 1) \
	& ~((DWORD)(ALIGN) - 1))

struct SECTION_INFO {
	BOOL Invalid = false;
	DWORD SectionFOAAddr;
	DWORD SectionVAAddr;
	LPVOID ContentBuffer;
	DWORD ContentSize;
	DWORD AlignSize; // For fileAlignment
};

class PEHandler {
private:
	LPVOID lpPeBuffer;
	LPCSTR peFileName;
	DWORD peSize;

	PIMAGE_NT_HEADERS peNtImageHeader; // Mapping to correspond pos in lpPeBuffer.
	WORD peABI;
	DWORD peFileAlignment;
	DWORD peSectionAlignment;
	WORD peNumberOfSections;

public:
	static PEHandler& getInstance() {
		static PEHandler peHandler;
		return peHandler;
	}

	// Attribute Init
	VOID setFileName(LPCSTR& fileName) { this->peFileName = fileName; }
	LPCSTR& getFileName() { return this->peFileName; }

	VOID setPEBuffer(LPVOID& lpPeBuffer) { this->lpPeBuffer = lpPeBuffer; }
	LPVOID& getPEBuffer() { return this->lpPeBuffer; }

	VOID setNtImageHeader(PIMAGE_NT_HEADERS& ntHeader) { this->peNtImageHeader = ntHeader; }
	PIMAGE_NT_HEADERS& getNtImageHeader() { return this->peNtImageHeader; }

	VOID updateNumberOfSections(WORD number);
	WORD& getNumberOfSections() { return this->peNumberOfSections; }

	WORD& getPEAbi() { return this->peABI; }
	DWORD& getPEFileSize() { return this->peSize; }
	DWORD& getFileAlignment() { return this->peFileAlignment; }
	DWORD& getSectionAlignment() { return this->peSectionAlignment; }

	// PE Handler
	UINT readFile(LPCSTR fileName);
	VOID initAnalyse();
	// return FOA address of added section
	SECTION_INFO addSection(LPCSTR sectionName, LPVOID sectionContent, DWORD sectionSize);
	VOID outputPEFile(LPCSTR fileName);
};
