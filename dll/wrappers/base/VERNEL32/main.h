/* MAIN INCLUDES **********************************************************/

#include <wine/config.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS

#include <wine/debug.h>

#include <winbase.h>

/* PSEH for SEH Support */
#include <pseh/pseh2.h>
#define NDEBUG

#include <cmfuncs.h>
#include <psfuncs.h>
#include <rtlfuncs.h>
#define NTOS_MODE_USER
#include <iofuncs.h>
#include <csr/csr.h>

#include <config.h>
#include <port.h>
#include <ntdllbase.h>
#include <winnls.h>
#include <unicode.h>
#include <base.h>
#include <wincon.h>
#include <strsafe.h>

#define WIN32_NO_STATUS
#include <winbase.h>

#define FIND_DATA_SIZE 0x4000
#define BASESRV_SERVERDLL_INDEX 1
#define LOCALE_NAME_USER_DEFAULT    NULL
// #define CREATE_EVENT_MANUAL_RESET 1
// #define CREATE_EVENT_INITIAL_SET  2
#define SYMBOLIC_LINK_FLAG_DIRECTORY  0x1
#define REPARSE_DATA_BUFFER_HEADER_SIZE   FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#define APPMODEL_ERROR_NO_PACKAGE                          15700
#define APPMODEL_ERROR_PACKAGE_RUNTIME_CORRUPT             15701
#define APPMODEL_ERROR_PACKAGE_IDENTITY_CORRUPT            15702
#define APPMODEL_ERROR_NO_APPLICATION                      15703
#define PROC_THREAD_ATTRIBUTE_PARENT_PROCESS (ProcThreadAttributeParentProcess | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST (ProcThreadAttributeHandleList | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY (ProcThreadAttributeGroupAffinity | PROC_THREAD_ATTRIBUTE_THREAD | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_IDEAL_PROCESSOR (ProcThreadAttributeIdealProcessor | PROC_THREAD_ATTRIBUTE_THREAD | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_UMS_THREAD (ProcThreadAttributeUmsThread | PROC_THREAD_ATTRIBUTE_THREAD | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_MIGITATION_POLICY (ProcThreadAttributeMitigationPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES (ProcThreadAttributeSecurityCapabilities | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL (ProcThreadAttributeProtectionLevel | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_JOB_LIST (ProcThreadAttributeJobList | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY (ProcThreadAttributeChildProcessPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY (ProcThreadAttributeAllApplicationPackagesPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_WIN32K_FILTER (ProcThreadAttributeWin32kFilter | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_NUMBER   0x0000ffff
#define PROC_THREAD_ATTRIBUTE_THREAD   0x00010000
#define PROC_THREAD_ATTRIBUTE_INPUT    0x00020000
#define PROC_THREAD_ATTRIBUTE_ADDITIVE 0x00040000

#define NLS_DEFAULT_LANGID        0x0409

#ifndef FileIdInformation
#define FileIdInformation (enum _FILE_INFORMATION_CLASS)59
#endif

#define ARGUMENT_PRESENT(x) ((x) != NULL)

PBASE_STATIC_SERVER_DATA BaseStaticServerData;
extern BOOL bIsFileApiAnsi;
extern HMODULE kernel32_handle DECLSPEC_HIDDEN;

/* TYPE DEFINITIONS **********************************************************/
typedef UINT(WINAPI * PPROCESS_START_ROUTINE)(VOID);
typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;
typedef NTSTATUS(NTAPI * PRTL_CONVERT_STRING)(IN PUNICODE_STRING UnicodeString, IN PANSI_STRING AnsiString, IN BOOLEAN AllocateMemory);

/* STRUCTS DEFINITIONS ******************************************************/
typedef struct _FIBER                                    /* Field offsets:    */
{                                                        /* i386  arm   x64   */
    PVOID FiberData;                                     /* 0x000 0x000 0x000 */
    struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;/* 0x004 0x004 0x008 */
    PVOID StackBase;                                     /* 0x008 0x008 0x010 */
    PVOID StackLimit;                                    /* 0x00C 0x00C 0x018 */
    PVOID DeallocationStack;                             /* 0x010 0x010 0x020 */
    CONTEXT FiberContext;                                /* 0x014 0x018 0x030 */
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
    PVOID Wx86Tib;                                       /* 0x2E0 0x1b8 0x500 */
    struct _ACTIVATION_CONTEXT_STACK *ActivationContextStackPointer; /* 0x2E4 0x1bc 0x508 */
    PVOID FlsData;                                       /* 0x2E8 0x1c0 0x510 */
    ULONG GuaranteedStackBytes;                          /* 0x2EC 0x1c4 0x518 */
    ULONG TebFlags;                                      /* 0x2F0 0x1c8 0x51C */
#else
    ULONG GuaranteedStackBytes;                          /* 0x2E0         */
    PVOID FlsData;                                       /* 0x2E4         */
    struct _ACTIVATION_CONTEXT_STACK *ActivationContextStackPointer;
#endif
} FIBER, *PFIBER;

typedef enum _FIND_DATA_TYPE
{
    FindFile   = 1,
    FindStream = 2
} FIND_DATA_TYPE;

typedef enum _PIPE_ATTRIBUTE_TYPE {
  PipeAttribute = 0,
  PipeConnectionAttribute = 1,
  PipeHandleAttribute = 2
}PIPE_ATTRIBUTE_TYPE, *PPIPE_ATTRIBUTE_TYPE;

typedef union _DIR_INFORMATION
{
    PVOID DirInfo;
    PFILE_FULL_DIR_INFORMATION FullDirInfo;
    PFILE_BOTH_DIR_INFORMATION BothDirInfo;
} DIR_INFORMATION;

typedef enum _DEP_SYSTEM_POLICY_TYPE
{
	OptOut = 1
}DEP_SYSTEM_POLICY_TYPE, *PDEP_SYSTEM_POLICY_TYPE;

typedef struct _FIND_FILE_DATA
{
    HANDLE Handle;
    FINDEX_INFO_LEVELS InfoLevel;
    FINDEX_SEARCH_OPS SearchOp;

    /*
     * For handling STATUS_BUFFER_OVERFLOW errors emitted by
     * NtQueryDirectoryFile in the FildNextFile function.
     */
    BOOLEAN HasMoreData;

    /*
     * "Pointer" to the next file info structure in the buffer.
     * The type is defined by the 'InfoLevel' parameter.
     */
    DIR_INFORMATION NextDirInfo;

    BYTE Buffer[FIND_DATA_SIZE];
} FIND_FILE_DATA, *PFIND_FILE_DATA;

typedef struct _FIND_STREAM_DATA
{
    STREAM_INFO_LEVELS InfoLevel;
    PFILE_STREAM_INFORMATION FileStreamInfo;
    PFILE_STREAM_INFORMATION CurrentInfo;
} FIND_STREAM_DATA, *PFIND_STREAM_DATA;

typedef struct _FIND_DATA_HANDLE
{
    FIND_DATA_TYPE Type;
    RTL_CRITICAL_SECTION Lock;

    /*
     * Pointer to the following finding data, located at
     * (this + 1). The type is defined by the 'Type' parameter.
     */
    union
    {
        PFIND_FILE_DATA FindFileData;
        PFIND_STREAM_DATA FindStreamData;
    } u;

} FIND_DATA_HANDLE, *PFIND_DATA_HANDLE;

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

typedef struct _PROCESSOR_RELATIONSHIP {
  BYTE           Flags;
  BYTE           EfficiencyClass;
  BYTE           Reserved[21];
  WORD           GroupCount;
  GROUP_AFFINITY GroupMask[ANYSIZE_ARRAY];
} PROCESSOR_RELATIONSHIP, *PPROCESSOR_RELATIONSHIP;

typedef struct _NUMA_NODE_RELATIONSHIP {
  DWORD          NodeNumber;
  BYTE           Reserved[20];
  GROUP_AFFINITY GroupMask;
} NUMA_NODE_RELATIONSHIP, *PNUMA_NODE_RELATIONSHIP;

typedef struct _PROCESSOR_GROUP_INFO {
  BYTE      MaximumProcessorCount;
  BYTE      ActiveProcessorCount;
  BYTE      Reserved[38];
  KAFFINITY ActiveProcessorMask;
} PROCESSOR_GROUP_INFO, *PPROCESSOR_GROUP_INFO;

typedef struct _GROUP_RELATIONSHIP {
  WORD                 MaximumGroupCount;
  WORD                 ActiveGroupCount;
  BYTE                 Reserved[20];
  PROCESSOR_GROUP_INFO GroupInfo[ANYSIZE_ARRAY];
} GROUP_RELATIONSHIP, *PGROUP_RELATIONSHIP;

typedef struct _CACHE_RELATIONSHIP {
  BYTE                 Level;
  BYTE                 Associativity;
  WORD                 LineSize;
  DWORD                CacheSize;
  PROCESSOR_CACHE_TYPE Type;
  BYTE                 Reserved[20];
  GROUP_AFFINITY       GroupMask;
} CACHE_RELATIONSHIP, *PCACHE_RELATIONSHIP;

typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
  LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
  DWORD                          Size;
  union {
    PROCESSOR_RELATIONSHIP Processor;
    NUMA_NODE_RELATIONSHIP NumaNode;
    CACHE_RELATIONSHIP     Cache;
    GROUP_RELATIONSHIP     Group;
  };
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;

typedef struct _REASON_CONTEXT {
  ULONG Version;
  DWORD Flags;
  union {
    struct {
      HMODULE LocalizedReasonModule;
      ULONG   LocalizedReasonId;
      ULONG   ReasonStringCount;
      LPWSTR  *ReasonStrings;
    } Detailed;
    LPWSTR SimpleReasonString;
  } Reason;
} REASON_CONTEXT, *PREASON_CONTEXT;

struct proc_thread_attr
{
    DWORD_PTR attr;
    SIZE_T size;
    void *value;
};

typedef struct _PROC_THREAD_ATTRIBUTE_LIST
{
    DWORD mask;  /* bitmask of items in list */
    DWORD size;  /* max number of items in list */
    DWORD count; /* number of items in list */
    DWORD pad;
    DWORD_PTR unk;
    struct proc_thread_attr attrs[1];
}PROC_THREAD_ATTRIBUTE_LIST, *PPROC_THREAD_ATTRIBUTE_LIST, *LPPROC_THREAD_ATTRIBUTE_LIST;

typedef enum _PROC_THREAD_ATTRIBUTE_NUM
{
    ProcThreadAttributeParentProcess = 0,
    ProcThreadAttributeHandleList = 2,
    ProcThreadAttributeGroupAffinity = 3,
    ProcThreadAttributeIdealProcessor = 5,
    ProcThreadAttributeUmsThread = 6,
    ProcThreadAttributeMitigationPolicy = 7,
    ProcThreadAttributeSecurityCapabilities = 9,
    ProcThreadAttributeProtectionLevel = 11,
    ProcThreadAttributeJobList = 13,
    ProcThreadAttributeChildProcessPolicy = 14,
    ProcThreadAttributeAllApplicationPackagesPolicy = 15,
    ProcThreadAttributeWin32kFilter = 16,
    ProcThreadAttributeSafeOpenPromptOriginClaim = 17,
} PROC_THREAD_ATTRIBUTE_NUM;

typedef enum _WER_REGISTER_FILE_TYPE
{
     WerRegFileTypeUserDocument = 1,
     WerRegFileTypeOther = 2,
     WerRegFileTypeMax
} WER_REGISTER_FILE_TYPE;

typedef struct _LOCALE_LCID{
	LPCWSTR localeName;
	LCID lcid;
}LOCALE_LCID;

typedef enum _POWER_REQUEST_TYPE { 
  PowerRequestDisplayRequired,
  PowerRequestSystemRequired,
  PowerRequestAwayModeRequired,
  PowerRequestExecutionRequired
} POWER_REQUEST_TYPE, *PPOWER_REQUEST_TYPE;

//
//  The variable structure contains the offsets to the various pieces of
//  locale information that is of variable length.  It is in the order
//  in which it is given in the file.
//
typedef struct locale_var_s
{
    WORD      SEngLanguage;            // English language name
    WORD      SAbbrevLang;             // abbreviated language name
    WORD      SAbbrevLangISO;          // ISO abbreviated language name
    WORD      SNativeLang;             // native language name
    WORD      SEngCountry;             // English country name
    WORD      SAbbrevCtry;             // abbreviated country name
    WORD      SAbbrevCtryISO;          // ISO abbreviated country name
    WORD      SNativeCtry;             // native country name
    WORD      SList;                   // list separator
    WORD      SDecimal;                // decimal separator
    WORD      SThousand;               // thousands separator
    WORD      SGrouping;               // grouping of digits
    WORD      SNativeDigits;           // native digits 0-9
    WORD      SCurrency;               // local monetary symbol
    WORD      SIntlSymbol;             // international monetary symbol
    WORD      SEngCurrName;            // English currency name
    WORD      SNativeCurrName;         // native currency name
    WORD      SMonDecSep;              // monetary decimal separator
    WORD      SMonThousSep;            // monetary thousands separator
    WORD      SMonGrouping;            // monetary grouping of digits
    WORD      SPositiveSign;           // positive sign
    WORD      SNegativeSign;           // negative sign
    WORD      STimeFormat;             // time format
    WORD      STime;                   // time separator
    WORD      S1159;                   // AM designator
    WORD      S2359;                   // PM designator
    WORD      SShortDate;              // short date format
    WORD      SDate;                   // date separator
    WORD      SYearMonth;              // year month format
    WORD      SLongDate;               // long date format
    WORD      IOptionalCal;            // additional calendar type(s)
    WORD      SDayName1;               // day name 1
    WORD      SDayName2;               // day name 2
    WORD      SDayName3;               // day name 3
    WORD      SDayName4;               // day name 4
    WORD      SDayName5;               // day name 5
    WORD      SDayName6;               // day name 6
    WORD      SDayName7;               // day name 7
    WORD      SAbbrevDayName1;         // abbreviated day name 1
    WORD      SAbbrevDayName2;         // abbreviated day name 2
    WORD      SAbbrevDayName3;         // abbreviated day name 3
    WORD      SAbbrevDayName4;         // abbreviated day name 4
    WORD      SAbbrevDayName5;         // abbreviated day name 5
    WORD      SAbbrevDayName6;         // abbreviated day name 6
    WORD      SAbbrevDayName7;         // abbreviated day name 7
    WORD      SMonthName1;             // month name 1
    WORD      SMonthName2;             // month name 2
    WORD      SMonthName3;             // month name 3
    WORD      SMonthName4;             // month name 4
    WORD      SMonthName5;             // month name 5
    WORD      SMonthName6;             // month name 6
    WORD      SMonthName7;             // month name 7
    WORD      SMonthName8;             // month name 8
    WORD      SMonthName9;             // month name 9
    WORD      SMonthName10;            // month name 10
    WORD      SMonthName11;            // month name 11
    WORD      SMonthName12;            // month name 12
    WORD      SMonthName13;            // month name 13 (if exists)
    WORD      SAbbrevMonthName1;       // abbreviated month name 1
    WORD      SAbbrevMonthName2;       // abbreviated month name 2
    WORD      SAbbrevMonthName3;       // abbreviated month name 3
    WORD      SAbbrevMonthName4;       // abbreviated month name 4
    WORD      SAbbrevMonthName5;       // abbreviated month name 5
    WORD      SAbbrevMonthName6;       // abbreviated month name 6
    WORD      SAbbrevMonthName7;       // abbreviated month name 7
    WORD      SAbbrevMonthName8;       // abbreviated month name 8
    WORD      SAbbrevMonthName9;       // abbreviated month name 9
    WORD      SAbbrevMonthName10;      // abbreviated month name 10
    WORD      SAbbrevMonthName11;      // abbreviated month name 11
    WORD      SAbbrevMonthName12;      // abbreviated month name 12
    WORD      SAbbrevMonthName13;      // abbreviated month name 13 (if exists)
    WORD      SEndOfLocale;            // end of locale information
} LOCALE_VAR, *PLOCALE_VAR;

//
//  Sortkey Structure.
//
typedef struct sortkey_s {

    union {
        struct sm_aw_s {
            BYTE   Alpha;              // alphanumeric weight
            BYTE   Script;             // script member
        } SM_AW;

        WORD  Unicode;                 // unicode weight

    } UW;

    BYTE      Diacritic;               // diacritic weight
    BYTE      Case;                    // case weight (with COMP)

} SORTKEY, *PSORTKEY;

#define MAX_FONTSIGNATURE         16   // length of font signature string

//
//  The fixed structure contains the locale information that is of
//  fixed length and in the order in which it is given in the file.
//
typedef struct locale_fixed_s
{
    WORD      DefaultACP;              // default ACP - integer format
    WORD      szILanguage[5];          // language id
    WORD      szICountry[6];           // country id
    WORD      szIGeoID[8];            // geographical location identifier
    WORD      szIDefaultLang[5];       // default language ID
    WORD      szIDefaultCtry[6];       // default country ID
    WORD      szIDefaultACP[6];        // default ansi code page ID
    WORD      szIDefaultOCP[6];        // default oem code page ID
    WORD      szIDefaultMACCP[6];      // default mac code page ID
    WORD      szIDefaultEBCDICCP[6];   // default ebcdic code page ID
    WORD      szIMeasure[2];           // system of measurement
    WORD      szIPaperSize[2];         // default paper size
    WORD      szIDigits[3];            // number of fractional digits
    WORD      szILZero[2];             // leading zeros for decimal
    WORD      szINegNumber[2];         // negative number format
    WORD      szIDigitSubstitution[2]; // digit substitution
    WORD      szICurrDigits[3];        // # local monetary fractional digits
    WORD      szIIntlCurrDigits[3];    // # intl monetary fractional digits
    WORD      szICurrency[2];          // positive currency format
    WORD      szINegCurr[3];           // negative currency format
    WORD      szIPosSignPosn[2];       // format of positive sign
    WORD      szIPosSymPrecedes[2];    // if mon symbol precedes positive
    WORD      szIPosSepBySpace[2];     // if mon symbol separated by space
    WORD      szINegSignPosn[2];       // format of negative sign
    WORD      szINegSymPrecedes[2];    // if mon symbol precedes negative
    WORD      szINegSepBySpace[2];     // if mon symbol separated by space
    WORD      szITime[2];              // time format
    WORD      szITLZero[2];            // leading zeros for time field
    WORD      szITimeMarkPosn[2];      // time marker position
    WORD      szIDate[2];              // short date order
    WORD      szICentury[2];           // century format (short date)
    WORD      szIDayLZero[2];          // leading zeros for day field (short date)
    WORD      szIMonLZero[2];          // leading zeros for month field (short date)
    WORD      szILDate[2];             // long date order
    WORD      szICalendarType[3];      // type of calendar
    WORD      szIFirstDayOfWk[2];      // first day of week
    WORD      szIFirstWkOfYr[2];       // first week of year
    WORD      szFontSignature[MAX_FONTSIGNATURE];
} LOCALE_FIXED, *PLOCALE_FIXED;

//
//  Constant Types
//
typedef  LPWORD        P844_TABLE;     // ptr to 8:4:4 table

typedef  LPWORD        PMB_TABLE;      // ptr to MB translation table
typedef  PMB_TABLE     PGLYPH_TABLE;   // ptr to GLYPH translation table
typedef  LPWORD        PDBCS_RANGE;    // ptr to DBCS range
typedef  LPWORD        PDBCS_OFFSETS;  // ptr to DBCS offset section
typedef  LPWORD        PDBCS_TABLE;    // ptr to DBCS translation table
typedef  PVOID         PWC_TABLE;      // ptr to WC translation table
typedef  P844_TABLE    PCTYPE;         // ptr to Character Type table
typedef  P844_TABLE    PCASE;          // ptr to Lower or Upper Case table
typedef  P844_TABLE    PADIGIT;        // ptr to Ascii Digits table
typedef  P844_TABLE    PCZONE;         // ptr to Fold Compat. Zone table
typedef  P844_TABLE    PKANA;          // ptr to Hiragana/Katakana table
typedef  P844_TABLE    PHALFWIDTH;     // ptr to Half Width table
typedef  P844_TABLE    PFULLWIDTH;     // ptr to Full Width table
typedef  P844_TABLE    PCHINESE;       // ptr to Traditional/Simplified Chinese table
typedef  P844_TABLE    PPRECOMP;       // ptr to PreComposed table
typedef  LPWORD        PCOMP_GRID;     // ptr to Composite table 2D grid
typedef  LPWORD        PLOC_INFO;      // ptr to locale information
typedef  LPWORD        PCAL_INFO;      // ptr to calendar information

typedef  DWORD         REVERSE_DW;     // reverse diacritic table
typedef  REVERSE_DW   *PREVERSE_DW;    // ptr to reverse diacritic table
typedef  DWORD         DBL_COMPRESS;   // double compression table
typedef  DBL_COMPRESS *PDBL_COMPRESS;  // ptr to double compression table
typedef  LPWORD        PCOMPRESS;      // ptr to compression table (2 or 3)

typedef  DWORD         NLSDEFINED;     // NLS defined codepoint table
typedef  NLSDEFINED   *PNLSDEFINED;    // ptr to NLS defined code point table

//
//  Compression Header Structure.
//  This is the header for the compression tables.
//
typedef struct compress_hdr_s {
    DWORD     Locale;                  // locale id
    DWORD     Offset;                  // offset (in words)
    WORD      Num2;                    // Number of 2 compressions
    WORD      Num3;                    // Number of 3 compressions
} COMPRESS_HDR, *PCOMPRESS_HDR;


//
//  Compression 2 Structure.
//  This is for a 2 code point compression - 2 code points
//  compress to ONE weight.
//
typedef struct compress_2_s {
    WCHAR     UCP1;                    // Unicode code point 1
    WCHAR     UCP2;                    // Unicode code point 2
    SORTKEY   Weights;                 // sortkey weights
} COMPRESS_2, *PCOMPRESS_2;

//
//  Compression 3 Structure.
//  This is for a 3 code point compression - 3 code points
//  compress to ONE weight.
//
typedef struct compress_3_s {
    WCHAR     UCP1;                    // Unicode code point 1
    WCHAR     UCP2;                    // Unicode code point 2
    WCHAR     UCP3;                    // Unicode code point 3
    WCHAR     Reserved;                // dword alignment
    SORTKEY   Weights;                 // sortkey weights
} COMPRESS_3, *PCOMPRESS_3;

//
//  Locale Hash Table Structure.
//
typedef struct loc_hash_s {
    LCID           Locale;             // locale ID
    PLOCALE_VAR    pLocaleHdr;         // ptr to locale header info
    PLOCALE_FIXED  pLocaleFixed;       // ptr to locale fixed size info
    PCASE          pUpperCase;         // ptr to Upper Case table
    PCASE          pLowerCase;         // ptr to Lower Case table
    PCASE          pUpperLinguist;     // ptr to Upper Case Linguistic table
    PCASE          pLowerLinguist;     // ptr to Lower Case Linguistic table
    PSORTKEY       pSortkey;           // ptr to sortkey table
    BOOL           IfReverseDW;        // if DW should go from right to left
    BOOL           IfCompression;      // if compression code points exist
    BOOL           IfDblCompression;   // if double compression exists
    BOOL           IfIdeographFailure; // if ideograph table failed to load
    PCOMPRESS_HDR  pCompHdr;           // ptr to compression header
    PCOMPRESS_2    pCompress2;         // ptr to 2 compression table
    PCOMPRESS_3    pCompress3;         // ptr to 3 compression table
    struct loc_hash_s *pNext;          // ptr to next locale hash node
} LOC_HASH, *PLOC_HASH;

typedef DWORD (*LPFN_CP_PROC)(DWORD, DWORD, LPSTR, int, LPWSTR, int, LPCPINFO);


//
//  CP Information Table Structure (as it is in the data file).
//
typedef struct cp_table_s {
    WORD      CodePage;                // code page number
    WORD      MaxCharSize;             // max length (bytes) of a char
    WORD      wDefaultChar;            // default character (MB)
    WORD      wUniDefaultChar;         // default character (Unicode)
    WORD      wTransDefaultChar;       // translation of wDefaultChar (Unicode)
    WORD      wTransUniDefaultChar;    // translation of wUniDefaultChar (MB)
    BYTE      LeadByte[MAX_LEADBYTES]; // lead byte ranges
} CP_TABLE, *PCP_TABLE;

//
//  Code Page Hash Table Structure.
//
typedef struct cp_hash_s {
    UINT           CodePage;           // codepage ID
    LPFN_CP_PROC   pfnCPProc;          // ptr to code page function proc
    PCP_TABLE      pCPInfo;            // ptr to CPINFO table
    PMB_TABLE      pMBTbl;             // ptr to MB translation table
    PGLYPH_TABLE   pGlyphTbl;          // ptr to GLYPH translation table
    PDBCS_RANGE    pDBCSRanges;        // ptr to DBCS ranges
    PDBCS_OFFSETS  pDBCSOffsets;       // ptr to DBCS offsets
    PWC_TABLE      pWC;                // ptr to WC table
    struct cp_hash_s *pNext;           // ptr to next CP hash node
} CP_HASH, *PCP_HASH;

//
//  Language Exception Header Structure.
//
typedef struct l_except_hdr_s {
    DWORD     Locale;                  // locale id
    DWORD     Offset;                  // offset to exception nodes (words)
    DWORD     NumUpEntries;            // number of upper case entries
    DWORD     NumLoEntries;            // number of lower case entries
} L_EXCEPT_HDR, *PL_EXCEPT_HDR;

//
//  Language Exception Structure.
//
typedef struct l_except_s
{
    WORD      UCP;                     // unicode code point
    WORD      AddAmount;               // amount to add to code point
} L_EXCEPT, *PL_EXCEPT;

//
//  Hash Table Pointers.
//
typedef  PCP_HASH  *PCP_HASH_TBL;      // ptr to a code page hash table
typedef  PLOC_HASH *PLOC_HASH_TBL;     // ptr to a locale hash table

//
//  CType Table Structure (Mapping table structure).
//
typedef struct ct_values_s {
    WORD      CType1;                  // ctype 1 value
    WORD      CType2;                  // ctype 2 value
    WORD      CType3;                  // ctype 3 value
} CT_VALUES, *PCT_VALUES;

//
//  Composite Information Structure.
//
typedef struct comp_info_s {
    BYTE           NumBase;            // number base chars in grid
    BYTE           NumNonSp;           // number non space chars in grid
    P844_TABLE     pBase;              // ptr to base char table
    P844_TABLE     pNonSp;             // ptr to nonspace char table
    PCOMP_GRID     pGrid;              // ptr to 2D grid
} COMP_INFO, *PCOMP_INFO;

//
//  Ideograph Lcid Exception Structure.
//
typedef struct ideograph_lcid_s {
    DWORD     Locale;                  // locale id
    WORD      pFileName[14];           // ptr to file name
} IDEOGRAPH_LCID, *PIDEOGRAPH_LCID;

//
//  Expansion Structure.
//
typedef struct expand_s {
    WCHAR     UCP1;                    // Unicode code point 1
    WCHAR     UCP2;                    // Unicode code point 2
} EXPAND, *PEXPAND;

//
//  Exception Header Structure.
//  This is the header for the exception tables.
//
typedef struct except_hdr_s {
    DWORD     Locale;                  // locale id
    DWORD     Offset;                  // offset to exception nodes (words)
    DWORD     NumEntries;              // number of entries for locale id
} EXCEPT_HDR, *PEXCEPT_HDR;

//
//  Exception Structure.
//
//  NOTE: May also be used for Ideograph Exceptions (4 column tables).
//
typedef struct except_s
{
    WORD      UCP;                     // unicode code point
    WORD      Unicode;                 // unicode weight
    BYTE      Diacritic;               // diacritic weight
    BYTE      Case;                    // case weight
} EXCEPT, *PEXCEPT;

//
//  Multiple Weight Structure.
//
typedef struct multiwt_s {
    BYTE      FirstSM;                 // value of first script member
    BYTE      NumSM;                   // number of script members in range
} MULTI_WT, *PMULTI_WT;

//
//  Jamo Sequence Sorting Info.
//
typedef struct {
    BYTE m_bOld;                  // sequence occurs only in old Hangul flag
    CHAR m_chLeadingIndex;        // indices used to locate prior modern Hangul syllable
    CHAR m_chVowelIndex;
    CHAR m_chTrailingIndex;
    BYTE m_ExtraWeight;           // extra weights that distinguish this from
                                  //   other old Hangul syllables, depending
                                  //   on the jamo, this can be a weight for
                                  //   leading jamo, vowel jamo, or trailing jamo.
} JAMO_SORT_INFO, *PJAMO_SORT_INFO;

//
//  Jamo Combination Table Entry.
//
//  NOTE: Make sure this structure is WORD aligned.  Otherwise, code will
//        fail in GetDefaultSortTable().
//
typedef struct {
    WCHAR m_wcCodePoint;          // Code point value that enters this state
    JAMO_SORT_INFO m_SortInfo;    // Sequence sorting info
    BYTE m_bTransitionCount;      // # of possible transitions from this state
} JAMO_COMPOSE_STATE, *PJAMO_COMPOSE_STATE;

//
//  Jamo Index Table Entry.
//
typedef struct {
    JAMO_SORT_INFO SortInfo;      // sequence sorting info
    BYTE Index;                   // index into the composition array
    BYTE TransitionCount;         // # of possible transitions from this state
    BYTE Reserved;                // word alignment
} JAMO_TABLE, *PJAMO_TABLE;

//
//  Geo Information structure. This structure holds information about
//  a geographical location on earth.
//
typedef struct tagGeoInfo {
    GEOID       GeoId;
    WCHAR       szLatitude[12];
    WCHAR       szLongitude[12];
    GEOCLASS    GeoClass;
    GEOID       ParentGeoId;
    WCHAR       szISO3166Abbrev2[4];
    WCHAR       szISO3166Abbrev3[4];
    WORD        wISO3166;
    WORD        Reserved;              // dword alignment
} GEOINFO, *PGEOINFO;

//
//  GEOID/LCID structure. This structure is used to navigate through
//  the table that maps corresponding Language ID and Geo ID.
//
typedef struct tagGEOIDLCID {
    LCID    lcid;
    GEOID   GeoId;
    LANGID  LangId;
    WORD    Reserved;                  // dword alignment
} GEOLCID, *PGEOLCID;

//
//  Sorting Version Info Structure.
//
typedef  struct _sortverinfo{
    LCID  Locale;       // Locale identifier
    DWORD Version;      // Sort version for this locale
}SORTVERINFO, *PSORTVERINFO;

//
//  Defined Code Point Version Info Structure.
//
typedef  struct _definedverinfo{
    DWORD       Version;          // Version of the Code Point table
    DWORD       dwOffset;         // Offset to the Defined Code Point table.
}DEFVERINFO, *PDEFVERINFO;

//
//  Table Pointers Structure.  This structure contains pointers to
//  the various tables needed for the NLS APIs.  There should be only
//  ONE of these for each process, and the information should be
//  global to the process.
//
#define NUM_SM     256                  // total number of script members
#define NUM_CAL    64                   // total number calendars allowed

typedef struct tbl_ptrs_s {
    PCP_HASH_TBL    pCPHashTbl;         // ptr to Code Page hash table
    PLOC_HASH_TBL   pLocHashTbl;        // ptr to Locale hash table
    PLOC_INFO       pLocaleInfo;        // ptr to locale table (all locales)
    DWORD           NumCalendars;       // number of calendars
    PCAL_INFO       pCalendarInfo;      // ptr to beginning of calendar info
    PCAL_INFO       pCalTbl[NUM_CAL];   // ptr to calendar table array
    P844_TABLE      pDefaultLanguage;   // ptr to default language table
    P844_TABLE      pLinguistLanguage;  // ptr to default linguistic lang table
    LARGE_INTEGER   LinguistLangSize;   // size of linguistic lang table
    int             NumLangException;   // number of language exceptions
    PL_EXCEPT_HDR   pLangExceptHdr;     // ptr to lang exception table header
    PL_EXCEPT       pLangException;     // ptr to lang exception tables
    PCT_VALUES      pCTypeMap;          // ptr to Ctype Mapping table
    PCTYPE          pCType844;          // ptr to Ctype 8:4:4 table
    PADIGIT         pADigit;            // ptr to Ascii Digits table
    PCZONE          pCZone;             // ptr to Compatibility Zone table
    PKANA           pHiragana;          // ptr to Hiragana table
    PKANA           pKatakana;          // ptr to Katakana table
    PHALFWIDTH      pHalfWidth;         // ptr to Half Width table
    PFULLWIDTH      pFullWidth;         // ptr to Full Width table
    PCHINESE        pTraditional;       // ptr to Traditional Chinese table
    PCHINESE        pSimplified;        // ptr to Simplified Chinese table
    PPRECOMP        pPreComposed;       // ptr to PreComposed Table
    PCOMP_INFO      pComposite;         // ptr to Composite info structure
    DWORD           NumReverseDW;       // number of REVERSE DIACRITICS
    DWORD           NumDblCompression;  // number of DOUBLE COMPRESSION locales
    DWORD           NumIdeographLcid;   // number of IDEOGRAPH LCIDs
    DWORD           NumExpansion;       // number of EXPANSIONS
    DWORD           NumCompression;     // number of COMPRESSION locales
    DWORD           NumException;       // number of EXCEPTION locales
    DWORD           NumMultiWeight;     // number of MULTIPLE WEIGHTS
    int             NumJamoIndex;       // number of entires for Jamo Index Table
    int             NumJamoComposition; // number of entires for Jamo Composition Table
    PSORTKEY        pDefaultSortkey;    // ptr to default sortkey table
    LARGE_INTEGER   DefaultSortkeySize; // size of default sortkey section
    PREVERSE_DW     pReverseDW;         // ptr to reverse diacritic table
    PDBL_COMPRESS   pDblCompression;    // ptr to double compression table
    PIDEOGRAPH_LCID pIdeographLcid;     // ptr to ideograph lcid table
    PEXPAND         pExpansion;         // ptr to expansion table
    PCOMPRESS_HDR   pCompressHdr;       // ptr to compression table header
    PCOMPRESS       pCompression;       // ptr to compression tables
    PEXCEPT_HDR     pExceptHdr;         // ptr to exception table header
    PEXCEPT         pException;         // ptr to exception tables
    PMULTI_WT       pMultiWeight;       // ptr to multiple weights table
    BYTE            SMWeight[NUM_SM];   // script member weights
    PJAMO_TABLE     pJamoIndex;         // ptr ot Jamo Index table
    PJAMO_COMPOSE_STATE pJamoComposition;  // ptr to Jamo Composition state machine table
    long            nGeoInfo;           // number of GEOINFO entries
    PGEOINFO        pGeoInfo;           // ptr to gegraphical info location table
    long            nGeoLCID;           // number of GEOID/LCID entries
    PGEOLCID        pGeoLCID;           // ptr to GEOID/LCID mapping table
    DWORD           NumSortVersion;     // number of sorting version
    PSORTVERINFO    pSortVersion;       // ptr sorting version info
    DWORD           NumDefinedVersion;  // number of defined code point version 
    PDEFVERINFO     pDefinedVersion;    // ptr defined code point version
    LPWORD          pSortingTableFileBase;  // The base address of sorting table file
} TBL_PTRS, *PTBL_PTRS;

extern LCID             gSystemLocale;      // system locale value
extern RTL_CRITICAL_SECTION gcsTblPtrs;     // critical section for tbl ptrs
extern PLOC_HASH        gpInvLocHashN;      // ptr to invariant loc hash node
extern PLOC_HASH        gpSysLocHashN;      // ptr to system loc hash node
extern PTBL_PTRS  pTblPtrs;   // ptr to structure of table ptrs

#define IS_INVALID_LOCALE(Locale)      ( Locale & ~NLS_VALID_LOCALE_MASK )

////////////////////////////////////////////////////////////////////////////
//
//  EXIST_LOCALE_INFO
//
//  Checks to see if the locale tables have been added to the locale
//  hash node.
//
//  Must check the FIXED locale pointer, since that value is set last in
//  the hash node.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define EXIST_LOCALE_INFO(pHashN)           (pHashN->pLocaleFixed)

////////////////////////////////////////////////////////////////////////////
//
//  CHECK_SPECIAL_LOCALES
//
//  Checks for the special locale values and sets the Locale to the
//  appropriate value.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CHECK_SPECIAL_LOCALES(Locale, UseCachedLocaleId)                    \
{                                                                           \
    /*                                                                      \
     *  Check for special locale values.                                    \
     */                                                                     \
    if (Locale == LOCALE_SYSTEM_DEFAULT)                                    \
    {                                                                       \
        /*                                                                  \
         *  Get the System Default locale value.                            \
         */                                                                 \
        Locale = gSystemLocale;                                             \
    }                                                                       \
    else if ((Locale == LOCALE_NEUTRAL) || (Locale == LOCALE_USER_DEFAULT)) \
    {                                                                       \
        /*                                                                  \
         *  Get the User locale value.                                      \
         */                                                                 \
        if (!UseCachedLocaleId)                                             \
        {                                                                   \
            Locale = GetUserDefaultLCID();                                  \
        }                                                                   \
        else                                                                \
        {                                                                   \
            Locale = pNlsUserInfo->UserLocaleId;                            \
        }                                                                   \
    }                                                                       \
    /*                                                                      \
     *  Check for a valid primary language and a neutral sublanguage.       \
     */                                                                     \
    else if (SUBLANGID(LANGIDFROMLCID(Locale)) == SUBLANG_NEUTRAL)          \
    {                                                                       \
        /*                                                                  \
         *  Re-form the locale id using the primary language and the        \
         *  default sublanguage.                                            \
         */                                                                 \
        Locale = MAKELCID(MAKELANGID(PRIMARYLANGID(LANGIDFROMLCID(Locale)), \
                                     SUBLANG_DEFAULT),                      \
                          SORTIDFROMLCID(Locale));                          \
    }                                                                       \
}

////////////////////////////////////////////////////////////////////////////
//
//  VALIDATE_LOCALE
//
//  Checks that the given LCID contains a valid locale id.  It does so
//  by making sure the appropriate locale information is present.  If the
//  locale is valid, pLocHashN will be non-NULL.  Otherwise, pLocHashN
//  will be NULL.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define VALIDATE_LOCALE(Locale, pLocHashN, UseCachedLocaleId)              \
{                                                                          \
    /*                                                                     \
     *  Check the system locale first for speed.  This is the most         \
     *  likely one to be used.                                             \
     */                                                                    \
    if (Locale == gSystemLocale)                                           \
    {                                                                      \
        pLocHashN = gpSysLocHashN;                                         \
    }                                                                      \
    /*                                                                     \
     *  Check the invariant locale second for speed.  This is the second   \
     *  most likely one to be used.                                        \
     */                                                                    \
    else if (Locale == LOCALE_INVARIANT)                                   \
    {                                                                      \
        pLocHashN = gpInvLocHashN;                                         \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  Check special locale values.                                   \
         */                                                                \
        CHECK_SPECIAL_LOCALES(Locale, UseCachedLocaleId);                  \
                                                                           \
        /*                                                                 \
         *  If the locale is the system default, then the hash node        \
         *  is already stored in a global.                                 \
         */                                                                \
        if (Locale == gSystemLocale)                                       \
        {                                                                  \
            pLocHashN = gpSysLocHashN;                                     \
        }                                                                  \
        else if (IS_INVALID_LOCALE(Locale))                                \
        {                                                                  \
            pLocHashN = NULL;                                              \
        }                                                                  \
        else                                                               \
        {                                                                  \
            /*                                                             \
             *  Get locale hash node to make sure the appropriate          \
             *  locale table is in the system.                             \
             *                                                             \
             *  NOTE:  If the call fails, pLocHashN will be NULL.          \
             */                                                            \
            pLocHashN = GetLocHashNode(Locale);                            \
        }                                                                  \
    }                                                                      \
}

#define LOC_TBL_SIZE              197  // size of locale hash table (prime #)

////////////////////////////////////////////////////////////////////////////
//
//  GET_HASH_VALUE
//
//  Returns the hash value for given value and the given table size.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define GET_HASH_VALUE(Value, TblSize)      (Value % TblSize)

////////////////////////////////////////////////////////////////////////////
//
//  FIND_LOCALE_HASH_NODE
//
//  Searches for the locale hash node for the given locale.  The result is
//  put in pHashN.  If no node exists, pHashN will be NULL.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define FIND_LOCALE_HASH_NODE( Locale,                                     \
                               pHashN )                                    \
{                                                                          \
    UINT Index;                   /* hash value */                         \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Get hash value.                                                    \
     */                                                                    \
    Index = GET_HASH_VALUE(Locale, LOC_TBL_SIZE);                          \
                                                                           \
    /*                                                                     \
     *  Get hash node.                                                     \
     */                                                                    \
    pHashN = (pTblPtrs->pLocHashTbl)[Index];                               \
    while ((pHashN != NULL) && (pHashN->Locale != Locale))                 \
    {                                                                      \
        pHashN = pHashN->pNext;                                            \
    }                                                                      \
}

#define NLS_SECTION_LOCALE         L"\\NLS\\NlsSectionLocale"

//
//  Locale Information Structures.
//
//  This is the format in which the locale information is kept in the
//  locale data file.  These structures are only used for offsets into
//  the data file, not to store information.
//

//
//  Header at the top of the locale.nls file.
//
typedef struct loc_cal_hdr_s
{
    DWORD     NumLocales;              // number of locales
    DWORD     NumCalendars;            // number of calendars
    DWORD     CalOffset;               // offset to calendar info (words)
} LOC_CAL_HDR, *PLOC_CAL_HDR;

//
//  Per entry locale header.
//
//  The locale header structure contains the information given in one entry
//  of the header for the locale information.
//
typedef struct locale_hdr_s {
    DWORD     Locale;                  // locale ID
    DWORD     Offset;                  // offset to locale information
} LOCALE_HDR, *PLOCALE_HDR;

#define LOCALE_HDR_OFFSET    (sizeof(LOC_CAL_HDR) / sizeof(WORD))

PVOID 
NTAPI 
RtlAllocateHeap( 	
	IN PVOID  	HeapHandle,
	IN ULONG  	Flags,
	IN SIZE_T  	Size 
);

#define RtlProcessHeap() (NtCurrentPeb()->ProcessHeap)

////////////////////////////////////////////////////////////////////////////
//
//  NLS_ALLOC_MEM
//
//  Allocates the given number of bytes of memory from the process heap,
//  zeros the memory buffer, and returns the handle.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define NLS_ALLOC_MEM(dwBytes)                                             \
    ( RtlAllocateHeap( RtlProcessHeap(),                                   \
                       HEAP_ZERO_MEMORY,                                   \
                       dwBytes ) )

////////////////////////////////////////////////////////////////////////////
//
//  CREATE_LOCALE_HASH_NODE
//
//  Creates a locale hash node and stores the pointer to it in pHashN.
//
//  NOTE: This macro may return if an error is encountered.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define CREATE_LOCALE_HASH_NODE( Locale,                                   \
                                 pHashN )                                  \
{                                                                          \
    /*                                                                     \
     *  Allocate LOC_HASH structure.                                       \
     */                                                                    \
    if ((pHashN = (PLOC_HASH)NLS_ALLOC_MEM(sizeof(LOC_HASH))) == NULL)     \
    {                                                                      \
        return (ERROR_OUTOFMEMORY);                                        \
    }                                                                      \
                                                                           \
    /*                                                                     \
     *  Fill in the Locale value.                                          \
     */                                                                    \
    pHashN->Locale = Locale;                                               \
}

ULONG
UnMapSection(
    PVOID pBaseAddr);
	
#define SORTKEY_HEADER            2    // size of SORTKEY table header	
	
////////////////////////////////////////////////////////////////////////////
//
//  NLS_FREE_MEM
//
//  Frees the memory of the given handle from the process heap.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define NLS_FREE_MEM(hMem)                                                 \
    ( (hMem) ? (RtlFreeHeap( RtlProcessHeap(),                             \
                             0,                                            \
                             (PVOID)hMem ))                                \
             : 0 )	

////////////////////////////////////////////////////////////////////////////
//
//  INSERT_LOC_HASH_NODE
//
//  Inserts a LOC hash node into the global LOC hash table.  It assumes
//  that all unused hash values in the table are pointing to NULL.  If
//  there is a collision, the new node will be added FIRST in the list.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define INSERT_LOC_HASH_NODE( pHashN,                                      \
                              pBaseAddr )                                  \
{                                                                          \
    UINT Index;                   /* hash value */                         \
    PLOC_HASH pSearch;            /* ptr to LOC hash node for search */    \
                                                                           \
                                                                           \
    /*                                                                     \
     *  Get hash value.                                                    \
     */                                                                    \
    Index = GET_HASH_VALUE(pHashN->Locale, LOC_TBL_SIZE);                  \
                                                                           \
    /*                                                                     \
     *  Enter table pointers critical section.                             \
     */                                                                    \
    RtlEnterCriticalSection(&gcsTblPtrs);                                  \
                                                                           \
    /*                                                                     \
     *  Make sure the hash node still doesn't exist in the table.          \
     */                                                                    \
    pSearch = (pTblPtrs->pLocHashTbl)[Index];                              \
    while ((pSearch != NULL) && (pSearch->Locale != pHashN->Locale))       \
    {                                                                      \
        pSearch = pSearch->pNext;                                          \
    }                                                                      \
                                                                           \
    /*                                                                     \
     *  If the hash node does not exist, insert the new one.               \
     *  Otherwise, free it.                                                \
     */                                                                    \
    if (pSearch == NULL)                                                   \
    {                                                                      \
        /*                                                                 \
         *  Insert hash node into hash table.                              \
         */                                                                \
        pHashN->pNext = (pTblPtrs->pLocHashTbl)[Index];                    \
        (pTblPtrs->pLocHashTbl)[Index] = pHashN;                           \
    }                                                                      \
    else                                                                   \
    {                                                                      \
        /*                                                                 \
         *  Free the resources allocated.                                  \
         */                                                                \
        if (pBaseAddr)                                                     \
        {                                                                  \
            UnMapSection(pBaseAddr);                                       \
        }                                                                  \
        if ((pHashN->pSortkey != pTblPtrs->pDefaultSortkey) &&             \
            (pHashN->pSortkey != NULL))                                    \
        {                                                                  \
            UnMapSection(((LPWORD)(pHashN->pSortkey)) - SORTKEY_HEADER);   \
        }                                                                  \
        NLS_FREE_MEM(pHashN);                                              \
    }                                                                      \
                                                                           \
    /*                                                                     \
     *  Leave table pointers critical section.                             \
     */                                                                    \
    RtlLeaveCriticalSection(&gcsTblPtrs);                                  \
}

//
//  String Constants.
//
#define MAX_PATH_LEN              512  // max length of path name

//
//  Size of stack buffer for PKEY_VALUE_FULL_INFORMATION pointer.
//
#define MAX_KEY_VALUE_FULLINFO                                             \
    ( FIELD_OFFSET( KEY_VALUE_FULL_INFORMATION, Name ) + MAX_PATH_LEN )
	
ULONG
OpenRegKey(
    PHANDLE phKeyHandle,
    LPWSTR pBaseName,
    LPWSTR pKey,
    ULONG fAccess);	
	
//
//  Paths to registry keys.
//
#define NLS_HKLM_SYSTEM    L"\\Registry\\Machine\\System\\CurrentControlSet\\Control"
#define NLS_HKLM_SOFTWARE  L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion"


//
//  Names of Registry Key Entries.
//
#define NLS_CODEPAGE_KEY           L"\\Nls\\Codepage"
#define NLS_LANGUAGE_GROUPS_KEY    L"\\Nls\\Language Groups"
#define NLS_LOCALE_KEY             L"\\Nls\\Locale"
#define NLS_ALT_SORTS_KEY          L"\\Nls\\Locale\\Alternate Sorts"
#define NLS_MUILANG_KEY            L"\\Nls\\MUILanguages"

//
//  User Info.
//
#define NLS_CTRL_PANEL_KEY         L"Control Panel\\International"
#define NLS_CALENDARS_KEY          L"Control Panel\\International\\Calendars"
#define NLS_TWO_DIGIT_YEAR_KEY     L"Control Panel\\International\\Calendars\\TwoDigitYearMax"
#define NLS_POLICY_TWO_DIGIT_YEAR_KEY L"Software\\Policies\\Microsoft\\Control Panel\\International\\Calendars\\TwoDigitYearMax"

//
//  Get the wide character count from a byte count.
//
#define GET_WC_COUNT(bc)          ((bc) / sizeof(WCHAR))

#define NLS_CHAR_ZERO           L'0'   // digit 0 character	
	
////////////////////////////////////////////////////////////////////////////
//
//  OPEN_ALT_SORTS_KEY
//
//  Opens the key for the alternate sorts section of the registry for read
//  access.
//
//  DEFINED AS A MACRO.
//
//  11-15-96    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define OPEN_ALT_SORTS_KEY(ReturnVal)                                      \
{                                                                          \
    /*                                                                     \
     *  Make sure alternate sorts key is open.                             \
     */                                                                    \
    if (hAltSortsKey == NULL)                                              \
    {                                                                      \
        RtlEnterCriticalSection(&gcsTblPtrs);                              \
        if (hAltSortsKey == NULL)                                          \
        {                                                                  \
            if (OpenRegKey( &hAltSortsKey,                                 \
                            NLS_HKLM_SYSTEM,                               \
                            NLS_ALT_SORTS_KEY,                             \
                            KEY_READ ))                                    \
            {                                                              \
                SetLastError(ERROR_BADDB);                                 \
                RtlLeaveCriticalSection(&gcsTblPtrs);                      \
                return (ReturnVal);                                        \
            }                                                              \
        }                                                                  \
        RtlLeaveCriticalSection(&gcsTblPtrs);                              \
    }                                                                      \
}	

////////////////////////////////////////////////////////////////////////////
//
//  NLS_REG_BUFFER_ALLOC
//
//  Allocates the buffer used by the registry enumeration and query calls
//  and sets the pKeyValueFull variable to point at the newly created buffer.
//
//  NOTE: This macro may return if an error is encountered.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define NLS_REG_BUFFER_ALLOC( pKeyValueFull,                               \
                              BufSize,                                     \
                              pBuffer,                                     \
                              CritSect )                                   \
{                                                                          \
    if ((pBuffer = (PVOID)NLS_ALLOC_MEM(BufSize)) == NULL)                 \
    {                                                                      \
        DbgPrint("NLSAPI: Could NOT Allocate Memory.\n");                  \
        if (CritSect)                                                      \
        {                                                                  \
            RtlLeaveCriticalSection(&gcsTblPtrs);                          \
        }                                                                  \
        return ((ULONG)STATUS_NO_MEMORY);                                  \
    }                                                                      \
                                                                           \
    pKeyValueFull = (PKEY_VALUE_FULL_INFORMATION)pBuffer;                  \
}


////////////////////////////////////////////////////////////////////////////
//
//  NLS_REG_BUFFER_FREE
//
//  Frees the buffer used by the registry enumeration and query calls.
//
//  DEFINED AS A MACRO.
//
//  05-31-91    JulieB    Created.
////////////////////////////////////////////////////////////////////////////

#define NLS_REG_BUFFER_FREE(pBuffer)        (NLS_FREE_MEM(pBuffer))

extern HANDLE           hAltSortsKey;       // handle to Locale\Alternate Sorts key

typedef struct _nlsversioninfoex {
  DWORD dwNLSVersionInfoSize;
  DWORD dwNLSVersion;
  DWORD dwDefinedVersion;
  DWORD dwEffectiveId;
  GUID  guidCustomVersion;
} NLSVERSIONINFOEX, *LPNLSVERSIONINFOEX;

typedef struct _TIME_DYNAMIC_ZONE_INFORMATION
{
     LONG Bias;
     WCHAR StandardName[32];
     SYSTEMTIME StandardDate;
     LONG StandardBias;
     WCHAR DaylightName[32];
     SYSTEMTIME DaylightDate;
     LONG DaylightBias;
     WCHAR TimeZoneKeyName[128];
     BOOLEAN DynamicDaylightTimeDisabled;
} DYNAMIC_TIME_ZONE_INFORMATION, *PDYNAMIC_TIME_ZONE_INFORMATION;

typedef DYNAMIC_TIME_ZONE_INFORMATION RTL_DYNAMIC_TIME_ZONE_INFORMATION;

NTSTATUS WINAPI RtlQueryDynamicTimeZoneInformation(RTL_DYNAMIC_TIME_ZONE_INFORMATION*);

/* helper for kernel32->ntdll timeout format conversion */
static inline PLARGE_INTEGER get_nt_timeout( PLARGE_INTEGER pTime, DWORD timeout )
{
    if (timeout == INFINITE) return NULL;
    pTime->QuadPart = (ULONGLONG)timeout * -10000;
    return pTime;
}

int 
wine_compare_string(
	int flags, 
	const WCHAR *str1, 
	int len1,
    const WCHAR *str2, 
	int len2
);

int 
wine_get_sortkey(
	int flags, 
	const WCHAR *src, 
	int srclen, 
	char *dst, 
	int dstlen
);

ULONG
WINAPI
BaseSetLastNTError(
    IN NTSTATUS Status
);

PUNICODE_STRING
WINAPI
Basep8BitStringToStaticUnicodeString(
	IN LPCSTR String
);

PWCHAR 
FilenameA2W(
	LPCSTR NameA, 
	BOOL alloc
);

LCID 
WINAPI 
LocaleNameToLCID( 
	LPCWSTR name, 
	DWORD flags 
);

INT 
WINAPI 
LCIDToLocaleName( 
	LCID lcid, 
	LPWSTR name, 
	INT count, 
	DWORD flags 
);

POBJECT_ATTRIBUTES 
WINAPI 
BaseFormatObjectAttributes( 	
	OUT POBJECT_ATTRIBUTES  	ObjectAttributes,
	IN PSECURITY_ATTRIBUTES SecurityAttributes  	OPTIONAL,
	IN PUNICODE_STRING  	ObjectName 
);

NTSTATUS 
NTAPI
NtPowerInformation(
  _In_      POWER_INFORMATION_LEVEL InformationLevel,
  _In_opt_  PVOID                   InputBuffer,
  _In_      ULONG                   InputBufferLength,
  _Out_opt_ PVOID                   OutputBuffer,
  _In_      ULONG                   OutputBufferLength
);

HANDLE 
WINAPI 
BaseGetNamedObjectDirectory(VOID);

NTSTATUS 
WINAPI 
RtlGetUserPreferredUILanguages(
	DWORD dwFlags, 
	BOOL verification,
	PULONG pulNumLanguages, 
	PZZWSTR pwszLanguagesBuffer, 
	PULONG pcchLanguagesBuffer
);

NTSTATUS 
WINAPI 
RtlGetThreadPreferredUILanguages(
	_In_       DWORD dwFlags,
	_Out_      PULONG pulNumLanguages,
	_Out_opt_  PZZWSTR pwszLanguagesBuffer,
	_Inout_    PULONG pcchLanguagesBuffer
);

ULONG GetLocaleFileInfo(
    LCID Locale,
    PLOC_HASH *ppNode,
    BOOLEAN fCreateNode);