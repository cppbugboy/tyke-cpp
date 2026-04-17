/**
 * @file test_response_filter.h
 * @brief 测试响应过滤器
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何实现自定义响应过滤器。
 */

#ifndef TYKE_TEST_RESPONSE_FILTER_H
#define TYKE_TEST_RESPONSE_FILTER_H
#include "filter/response_filter.h"

/**
 * @brief 测试响应过滤器类
 *
 * 演示响应过滤器的基本实现。
 */
class TestResponseFilter : public tyke::ResponseFilter
{
public:
    TestResponseFilter();
    ~TestResponseFilter() override;

    /**
     * @brief 响应前置处理
     * @param response 响应对象
     * @return true继续处理
     */
    bool Before(const tyke::TykeResponse& response) override;

    /**
     * @brief 响应后置处理
     * @param response 响应对象
     * @return true继续处理
     */
    bool After(const tyke::TykeResponse& response) override;
};


#endif //TYKE_TEST_RESPONSE_FILTER_H
