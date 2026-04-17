/**
 * @file request_filter.h
 * @brief 请求过滤器接口
 * @author Nick
 * @date 2026/04/17
 *
 * RequestFilter是请求过滤器的抽象基类，定义了请求处理前后的拦截接口。
 * 过滤器可以用于认证、授权、日志记录、参数校验等场景。
 *
 * 使用示例：
 * @code
 *   class AuthFilter : public RequestFilter {
 *   public:
 *       bool Before(const TykeRequest& request, TykeResponse& response) override {
 *           // 检查认证信息
 *           return true; // 返回true继续处理，返回false中断处理
 *       }
 *       bool After(const TykeRequest& request, TykeResponse& response) override {
 *           return true;
 *       }
 *   };
 * @endcode
 */

#ifndef TYKE_REQUESTFILTER_H
#define TYKE_REQUESTFILTER_H

#include "core/tyke_request.h"

namespace tyke
{
    /**
     * @brief 请求过滤器抽象基类
     *
     * 定义请求处理前后的拦截接口。
     * 过滤器按注册顺序执行Before方法，按逆序执行After方法。
     */
    class RequestFilter
    {
    public:
        RequestFilter() = default;
        virtual ~RequestFilter() = default;

        /**
         * @brief 请求前置处理
         * @param request 请求对象（只读）
         * @param response 响应对象（可修改）
         * @return true 继续处理，false 中断处理链
         *
         * 在请求处理器执行前调用，可用于认证、授权、参数校验等。
         * 返回false将中断处理链，直接返回响应。
         */
        virtual bool Before(const TykeRequest& request, TykeResponse& response) = 0;

        /**
         * @brief 请求后置处理
         * @param request 请求对象（只读）
         * @param response 响应对象（可修改）
         * @return true 继续处理，false 中断处理链
         *
         * 在请求处理器执行后调用，可用于日志记录、响应修改等。
         */
        virtual bool After(const TykeRequest& request, TykeResponse& response) = 0;
    };
}
#endif //TYKE_REQUESTFILTER_H
