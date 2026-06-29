/*
	Configuration options used by both platform specific and platform
	independent Mini vMac code. This Rockbox variation is based on the
	standard Mini vMac 36.04 Macintosh Plus generated configuration.
*/

#define MayInline inline __attribute__((always_inline))
#define MayNotInline __attribute__((noinline))
#define SmallGlobals 0
#define cIncludeUnused 0
#define UnusedParam(p) (void) p

typedef unsigned char ui3b;
#define HaveRealui3b 1

typedef signed char si3b;
#define HaveRealsi3b 1

typedef unsigned short ui4b;
#define HaveRealui4b 1

typedef short si4b;
#define HaveRealsi4b 1

typedef unsigned int ui5b;
#define HaveRealui5b 1

typedef int si5b;
#define HaveRealsi5b 1

#define HaveRealui6b 0
#define HaveRealsi6b 0

typedef ui3b ui3r;
#define ui3beqr 1

typedef si3b si3r;
#define si3beqr 1

typedef ui4b ui4r;
#define ui4beqr 1

typedef si4b si4r;
#define si4beqr 1

typedef ui5b ui5r;
#define ui5beqr 1

typedef si5b si5r;
#define si5beqr 1

#define MySoundEnabled 0

#define MySoundRecenterSilence 0
#define kLn2SoundSampSz 3

#define dbglog_HAVE 0
#define WantAbnormalReports 0

#define NumDrives 6
#define IncludeSonyRawMode 0
#define IncludeSonyGetName 0
#define IncludeSonyNew 0
#define IncludeSonyNameNew 0

#define vMacScreenHeight 342
#define vMacScreenWidth 512
#define vMacScreenDepth 0

#define kROM_Size 0x00020000

#define IncludePbufs 0
#define NumPbufs 0

#define EnableMouseMotion 1

#define IncludeHostTextClipExchange 0
#define EnableAutoSlow 1
#define EmLocalTalk 0
#define AutoLocation 1
#define AutoTimeZone 1
