// Version.h — Sürüm metni TEK YERDE: CMakeLists.txt'deki project(VERSION).
//
// CMake numarayı derleyiciye CRISP_VERSION_* tanımları olarak geçirir (bkz.
// target_compile_definitions); res/app.rc de aynı tanımlardan VERSIONINFO'yu
// kurar. Buradaki iş yalnızca biçimlendirmek. Elle yazılan bir kopya iki
// sürüm sonra bayatlıyordu: Hakkında kutusu 0.3.0 derken paket 0.8.0'dı.
#pragma once

#ifndef CRISP_VERSION_MAJOR
#define CRISP_VERSION_MAJOR 0
#endif
#ifndef CRISP_VERSION_MINOR
#define CRISP_VERSION_MINOR 0
#endif
#ifndef CRISP_VERSION_PATCH
#define CRISP_VERSION_PATCH 0
#endif

#define CRISP_WSTRINGIFY2(x) L## #x
#define CRISP_WSTRINGIFY(x) CRISP_WSTRINGIFY2(x)

// L"0.8.0" biçiminde geniş dize sabiti.
#define CRISP_VERSION_TEXT                                                     \
    CRISP_WSTRINGIFY(CRISP_VERSION_MAJOR) L"." CRISP_WSTRINGIFY(CRISP_VERSION_MINOR) \
        L"." CRISP_WSTRINGIFY(CRISP_VERSION_PATCH)
