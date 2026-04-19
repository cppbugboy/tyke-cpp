/**
 * @file example_response_controller.h
 * @brief 示例响应控制器声明。演示异步响应的路由注册和处理。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>

#include "tyke/tyke.h"

class ExampleResponseController : public tyke::ResponseController, public tyke::Singleton<ExampleResponseController>
{
    friend class tyke::Singleton<ExampleResponseController>;

public:
    void RegisterMethod() override;

private:
    ExampleResponseController() = default;
    ~ExampleResponseController() override = default;

    void HandleAsyncCallback(const tyke::TykeResponse& response);
    void HandleAsyncNotification(const tyke::TykeResponse& response);

    void LogResponse(const tyke::TykeResponse& response, const std::string& handler_name);
};
