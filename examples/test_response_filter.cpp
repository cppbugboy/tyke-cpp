/**
 * @file test_response_filter.cpp
 * @brief 测试响应过滤器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TestResponseFilter类的具体逻辑。
 */

#include "test_response_filter.h"

TestResponseFilter::TestResponseFilter()
= default;

TestResponseFilter::~TestResponseFilter()
= default;

bool TestResponseFilter::Before(const tyke::TykeResponse& response)
{
    (void)response;
    return true;
}

bool TestResponseFilter::After(const tyke::TykeResponse& response)
{
    (void)response;
    return true;
}
