/**
 * @file test_request_filter.cpp
 * @brief 测试请求过滤器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TestRequestFilter类的具体逻辑。
 */

#include "test_request_filter.h"

#include "fmt/base.h"

TestRequestFilter::TestRequestFilter()
= default;

TestRequestFilter::~TestRequestFilter()
= default;

bool TestRequestFilter::Before(const tyke::TykeRequest &request, tyke::TykeResponse &response)
{
    (void)request;
    (void)response;
    fmt::print("before\n");
    return true;
}

bool TestRequestFilter::After(const tyke::TykeRequest &request, tyke::TykeResponse &response)
{
    (void)request;
    (void)response;
    fmt::print("after\n");
    return true;
}
