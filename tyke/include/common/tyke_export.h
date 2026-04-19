/**
 * @file tyke_export.h
 * @brief 动态库导出宏定义。根据编译配置生成TYKE_API宏用于DLL符号导出/导入。
 * @author Nick
 * @date 2026/04/19
 */

#ifndef TYKE_EXPORT_H
#define TYKE_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef TYKE_BUILDING_DLL
        #define TYKE_API __declspec(dllexport)
    #elif defined(TYKE_USING_DLL)
        #define TYKE_API __declspec(dllimport)
    #else
        #define TYKE_API
    #endif
#else
    #if __GNUC__ >= 4
        #define TYKE_API __attribute__((visibility("default")))
    #else
        #define TYKE_API
    #endif
#endif

#endif
