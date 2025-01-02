#pragma once
#include "lnk.h"

#define LNK_PE_DOS_MAGIC 0x5A4D
#define LNK_PE_NT_SIGNATURE 0x00004550
#define LNK_PE_OPT_MAGIC 0x020B

#define LNK_PE_SECT_NAME_SIZE 8

struct lnk_pe_dos_hdr {
	uint16_t magic;
	uint16_t cblp;
	uint16_t cp;
	uint16_t crlc;
	uint16_t cparhdr;
	uint16_t minalloc;
	uint16_t maxalloc;
	uint16_t ss;
	uint16_t sp;
	uint16_t csum;
	uint16_t ip;
	uint16_t cs;
	uint16_t lfarlc;
	uint16_t ovno;
	uint16_t res[4];
	uint16_t oemid;
	uint16_t oeminfo;
	uint16_t res2[10];
	uint32_t lfanew;
};

enum lnk_pe_mach {
	LnkPeMachUnknown = 0x0000,
	LnkPeMachTargetHost = 0x0001,
	LnkPeMachI386 = 0x014C,
	LnkPeMachAmd64 = 0x8664
};
enum lnk_pe_chars {
	LnkPeCharsRelocsStripped = 0x0001,
	LnkPeCharsExecutableImage = 0x0002,
	LnkPeCharsLineNumsStripped = 0x0004,
	LnkPeCharsLocalSymsStripped = 0x0008,
	LnkPeCharsAgressiveWsTrim = 0x0010,
	LnkPeCharsLargeAddressAware = 0x0020,
	LnkPeCharsBytesReservedLo = 0x0080,
	LnkPeChars32BitMachine = 0x0100,
	LnkPeCharsDebugStripped = 0x0200,
	LnkPeCharsRemovableRunFromSwap = 0x0400,
	LnkPeCharsNetRunFromSwap = 0x0800,
	LnkPeCharsSystem = 0x1000,
	LnkPeCharsDll = 0x2000,
	LnkPeCharsUpSystemOnly = 0x4000,
	LnkPeCharsBytesReservedHigh = 0x8000
};
struct lnk_pe_file_hdr {
	lnk_pe_mach_t machine;
	uint16_t number_of_sections;
	uint32_t time_date_stamp;
	uint32_t pointer_to_symbol_table;
	uint32_t number_of_symbols;
	uint16_t size_of_optional_header;
	lnk_pe_chars_t characteristics;
};

struct lnk_pe_data_dir {
	uint32_t virtual_address;
	uint32_t size;
};
enum lnk_pe_data_dir_type {
	LnkPeDataDirExport = 0,
	LnkPeDataDirImport = 1,
	LnkPeDataDirResource = 2,
	LnkPeDataDirException = 3,
	LnkPeDataDirSecurity = 4,
	LnkPeDataDirBaseReloc = 5,
	LnkPeDataDirDebug = 6,
	LnkPeDataDirArchitecture = 7,
	LnkPeDataDirGlobalPtr = 8,
	LnkPeDataDirTls = 9,
	LnkPeDataDirLoadConfig = 10,
	LnkPeDataDirBoundImport = 11,
	LnkPeDataDirIat = 12,
	LnkPeDataDirDelayImport = 13,
	LnkPeDataDirComDescriptor = 14,

	LnkPeDataDir_Count = 16
};

enum lnk_pe_subsys {
	LnkPeSubsysUnknown = 0,
	LnkPeSubsysNative = 1,
	LnkPeSubsysWindowsGui = 2,
	LnkPeSubsysWindowsCui = 3,
	LnkPeSubsysOs2Cui = 5,
	LnkPeSubsysPosixCui = 7,
	LnkPeSubsysNativeWindows = 8,
	LnkPeSubsysWindowsCeGui = 9,
	LnkPeSubsysEfiApplication = 10,
	LnkPeSubsysEfiBootServiceDriver = 11,
	LnkPeSubsysEfiRuntimeDriver = 12,
	LnkPeSubsysEfiRom = 13,
	LnkPeSubsysXbox = 14,
	LnkPeSubsysWindowsBootApplication = 16,
	LnkPeSubsysXboxCodeCatalog = 17
};
enum lnk_pe_dll_chars {
	LnkPeDllCharsHighEntropyVa = 0x0020,
	LnkPeDllCharsDynamicBase = 0x0040,
	LnkPeDllCharsForceIntegrity = 0x0080,
	LnkPeDllCharsNxCompat = 0x0100,
	LnkPeDllCharsNoIsolation = 0x0200,
	LnkPeDllCharsNoSeh = 0x0400,
	LnkPeDllCharsNoBind = 0x0800,
	LnkPeDllCharsAppcontainer = 0x1000,
	LnkPeDllCharsWdmDriver = 0x2000,
	LnkPeDllCharsGuardCf = 0x4000,
	LnkPeDllCharsTerminalServerAware = 0x8000
};
struct lnk_pe_opt_hdr {
	uint16_t magic;
	uint8_t major_linker_version;
	uint8_t minor_linker_version;
	uint32_t size_of_code;
	uint32_t size_of_initialized_data;
	uint32_t size_of_uninitialized_data;
	uint32_t address_of_entry_point;
	uint32_t base_of_code;
	uint64_t image_base;
	uint32_t section_alignment;
	uint32_t file_alignment;
	uint16_t major_operating_system_version;
	uint16_t minor_operating_system_version;
	uint16_t major_image_version;
	uint16_t minor_image_version;
	uint16_t major_subsystem_version;
	uint16_t minor_subsystem_version;
	uint32_t win32_version_value;
	uint32_t size_of_image;
	uint32_t size_of_headers;
	uint32_t check_sum;
	lnk_pe_subsys_t subsystem;
	lnk_pe_dll_chars_t dll_characteristics;
	uint64_t size_of_stack_reserve;
	uint64_t size_of_stack_commit;
	uint64_t size_of_heap_reserve;
	uint64_t size_of_heap_commit;
	uint32_t loader_flags;
	uint32_t number_of_rva_and_sizes;
	lnk_pe_data_dir_t data_directory[LnkPeDataDir_Count];
};

struct lnk_pe_nt_hdrs {
	uint32_t signature;
	lnk_pe_file_hdr_t file_header;
	lnk_pe_opt_hdr_t optional_header;
};

enum lnk_pe_sect_chars {
	LnkPeSectCharsCode = 0x00000020,
	LnkPeSectCharsInitializedData = 0x00000040,
	LnkPeSectCharsUninitializedData = 0x00000080,

	LnkPeSectCharsMemDiscardable = 0x02000000,
	LnkPeSectCharsMemNotCached = 0x04000000,
	LnkPeSectCharsMemNotPaged = 0x08000000,
	LnkPeSectCharsMemShared = 0x10000000,
	LnkPeSectCharsMemExecute = 0x20000000,
	LnkPeSectCharsMemRead = 0x40000000,
	LnkPeSectCharsMemWrite = 0x80000000,
};
struct lnk_pe_sect_hdr {
	uint8_t name[LNK_PE_SECT_NAME_SIZE];
	union {
		uint32_t physical_address;
		uint32_t virtual_size;
	} misc;
	uint32_t virtual_address;
	uint32_t size_of_raw_data;
	uint32_t pointer_to_raw_data;
	uint32_t pointer_to_relocations;
	uint32_t pointer_to_linenumbers;
	uint16_t number_of_relocations;
	uint16_t number_of_linenumbers;
	lnk_pe_sect_chars_t characteristics;
};

struct lnk_pe_rt_func {
	uint32_t begin_address;
	uint32_t end_address;
	union {
		uint32_t unwind_info_address;
		uint32_t unwind_data;
	};
};

enum lnk_pe_base_reloc_type {
	LnkPeBaseRelocAbsolute = 0,
	LnkPeBaseRelocHigh = 1,
	LnkPeBaseRelocLow = 2,
	LnkPeBaseRelocHighlow = 3,
	LnkPeBaseRelocHighadj = 4,
	LnkPeBaseRelocMachineSpecific5 = 5,
	LnkPeBaseRelocReserved = 6,
	LnkPeBaseRelocMachineSpecific7 = 7,
	LnkPeBaseRelocMachineSpecific8 = 8,
	LnkPeBaseRelocMachineSpecific9 = 9,
	LnkPeBaseRelocDir64 = 10
};
struct lnk_pe_base_reloc {
	uint32_t virtual_address;
	uint32_t size_of_block;
};
struct lnk_pe_base_reloc_entry {
	uint16_t offset : 12;
	uint16_t type : 4;
};

struct lnk_pe_impt_desc {
	union {
		uint32_t characteristics;
		uint32_t original_first_thunk;
	};
	uint32_t time_date_stamp;
	uint32_t forwarder_chain;
	uint32_t name;
	uint32_t first_thunk;
};
