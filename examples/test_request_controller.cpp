/**
 * @file test_request_controller.cpp
 * @brief 测试请求控制器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现TestRequestController类的具体逻辑。
 */

#include "test_request_controller.h"
#include "test_request_filter.h"

#include "tyke/tyke.h"
#include "fmt/base.h"

// 自动注册控制器
TYKE_CONTROLLER_REGISTER(TestRequestController);

void TestRequestController::RegisterMethod()
{
    fmt::print("TestRequestController::RegisterMethod()\n");

    const auto root_group = tyke::REQUEST_ROUTER_INSTANCE->GetRoot();

    // 注册 /test/hello 路由，添加过滤器
    auto test_group = root_group->AddSubGroup("/test");
    test_group->AddFilter(std::shared_ptr<tyke::RequestFilter>(TEST_REQUEST_FILTER, [](tyke::RequestFilter*){})).AddRouteHandler("/hello", Hello);

    // 注册 /test2/test2-2 路由组
    auto test2_group = root_group->AddSubGroup("/test2");
    test2_group->AddSubGroup("/test2-2");
}

void TestRequestController::Hello(const tyke::TykeRequest &request, tyke::TykeResponse &response)
{
    (void)request;
    fmt::print("TestRequestController::Test()\n");
    std::string content = "hello world";
    response.SetContent(tyke::ContentType::kText, std::vector<unsigned char>(content.begin(), content.end()));
}
