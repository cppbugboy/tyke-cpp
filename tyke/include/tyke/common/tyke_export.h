/**
 * @file tyke_export.h
 * @brief 动态库导出/导入宏定义。根据编译配置自动选择TYKE_API的导出或导入属性。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 跨平台DLL符号可见性控制：
 * - Windows: 使用__declspec(dllexport/dllimport)
 *   - 定义TYKE_BUILDING_DLL时导出符号
 *   - 定义TYKE_USING_DLL时导入符号
 *   - 两者都未定义时（静态链接）TYKE_API为空
 * - Linux/macOS: 使用__attribute__((visibility("default")))
 *   - GCC/Clang >= 4.0 时设置默认可见性
 *   - 其他编译器退化为空宏
 */

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    /// @brief Windows平台：在构建DLL时定义TYKE_BUILDING_DLL以导出符号
    #ifdef TYKE_BUILDING_DLL
        #define TYKE_API __declspec(dllexport)
    /// @brief Windows平台：在使用DLL时定义TYKE_USING_DLL以导入符号
    #elif defined(TYKE_USING_DLL)
        #define TYKE_API __declspec(dllimport)
    /// @brief 静态链接时TYKE_API为空（无需导出/导入）
    #else
        #define TYKE_API
    #endif
#else
    /// @brief Linux/macOS平台：使用GCC可见性属性
    #if __GNUC__ >= 4
        #define TYKE_API __attribute__((visibility("default")))
    #else
        #define TYKE_API
    #endif
#endif
