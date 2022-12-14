#define VERSION "1.2.1"

// see https://github.com/cpredef/predef
#if (defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(_X86_) || defined(__I86__) || defined(__INTEL__)) && !CRYPTOPP_BOOL_X32
#define CPUSUPPORT_X86_CPUID 1
#define CPUSUPPORT_X86_SSE2 1
#define CPUSUPPORT_X86_AESNI 1
#endif

#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define STDC_HEADERS 1

#ifdef _WIN32
#define WIN_ALIGNED_MALLOC 1
#define WIN_CPUID 1
#else
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_MMAP 1
#define HAVE_STRINGS_H 1
#define HAVE_STRUCT_SYSINFO 1
#define HAVE_STRUCT_SYSINFO_MEM_UNIT 1
#define HAVE_STRUCT_SYSINFO_TOTALRAM 1
#define HAVE_SYS_SYSINFO_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYSINFO 1
#define HAVE_UNISTD_H 1
#endif



