/**
 * @file response_filter.h
 * @brief 响应过滤器接口
 * @author Nick
 * @date 2026/04/17
 *
 * ResponseFilter是响应过滤器的抽象基类，定义了响应处理前后的拦截接口。
 * 过滤器可以用于日志记录、响应修改、统计等场景。
 *
 * 使用示例：
 * @code
 *   class LogFilter : public ResponseFilter {
 *   public:
 *       bool Before(const TykeResponse& response) override {
 *           // 记录响应日志
 *           return true;
 *       }
 *       bool After(const TykeResponse& response) override {
 *           return true;
 *       }
 *   };
 * @endcode
 */

#ifndef TYKE_RESPONSEFILTER_H
#define TYKE_RESPONSEFILTER_H

#include "core/tyke_response.h"

namespace tyke
{
    /**
     * @brief 响应过滤器抽象基类
     *
     * 定义响应处理前后的拦截接口。
     * 过滤器按注册顺序执行Before方法，按逆序执行After方法。
     */
    class ResponseFilter
    {
    public:
        ResponseFilter() = default;
        virtual ~ResponseFilter() = default;

        /**
         * @brief 响应前置处理
         * @param response 响应对象
         * @return true 继续处理，false 中断处理链
         *
         * 在响应处理器执行前调用。
         */
        virtual bool Before(const TykeResponse& response) = 0;

        /**
         * @brief 响应后置处理
         * @param response 响应对象
         * @return true 继续处理，false 中断处理链
         *
         * 在响应处理器执行后调用。
         */
        virtual bool After(const TykeResponse& response) = 0;
    };
}
#endif //TYKE_RESPONSEFILTER_H
