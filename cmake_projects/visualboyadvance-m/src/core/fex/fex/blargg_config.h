// Library configuration. Modify this file as necessary.

// File_Extractor 1.0.0
#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment a #define line below to have effect described.

// Enable RAR archive support. Doing so adds extra licensing restrictions
// to this library (see unrar/readme.txt for more information).
//#define FEX_ENABLE_RAR 1

// Accept file paths encoded as UTF-8. Currently only affects Windows,
// as Unix/Linux/Mac OS X already use UTF-8 paths.
//#define BLARGG_UTF8_PATHS 1

// Enable support for as building DLL on Windows.
//#define BLARGG_BUILD_DLL 1

#ifndef FEX_ENABLE_RAR
#define FEX_ENABLE_RAR 1
#endif

#if FEX_ENABLE_BZ2
#define FEX_TYPE_BZ2 fex_bz2_type,
#else
#define FEX_TYPE_BZ2
#endif

#if FEX_ENABLE_LZMA
#define FEX_TYPE_LZMA fex_xz_type, fex_lz_type,
#else
#define FEX_TYPE_LZMA
#endif

#define FEX_TYPE_LIST fex_7z_type, fex_gz_type, fex_zip_type, fex_tar_type, fex_rar_type, FEX_TYPE_BZ2 FEX_TYPE_LZMA

// Use standard config.h if present
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#endif
