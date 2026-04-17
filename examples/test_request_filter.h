/**
 * @file test_request_filter.h
 * @brief 测试请求过滤器
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何实现自定义请求过滤器。
 */

#ifndef TYKE_TEST_REQUEST_FILTER_H
#define TYKE_TEST_REQUEST_FILTER_H
#include "tyke/tyke.h"

/// 测试请求过滤器单例访问宏
#define TEST_REQUEST_FILTER TestRequestFilter::GetInstance()

/**
 * @brief 测试请求过滤器类
 *
 * 演示请求过滤器的基本实现，在请求处理前后打印日志。
 */
class TestRequestFilter : public tyke::RequestFilter, public tyke::Singleton<TestRequestFilter>
{
    friend class tyke::Singleton<TestRequestFilter>;

public:
    /**
     * @brief 请求前置处理
     * @param request 请求对象
     * @param response 响应对象
     * @return true继续处理
     */
    bool Before(const tyke::TykeRequest& request, tyke::TykeResponse& response) override;

    /**
     * @brief 请求后置处理
     * @param request 请求对象
     * @param response 响应对象
     * @return true继续处理
     */
    bool After(const tyke::TykeRequest& request, tyke::TykeResponse& response) override;

private:
    TestRequestFilter();
    ~TestRequestFilter() override;
};


#endif //TYKE_TEST_REQUEST_FILTER_H
