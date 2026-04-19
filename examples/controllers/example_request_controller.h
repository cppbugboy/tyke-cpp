/**
 * @file example_request_controller.h
 * @brief 示例请求控制器声明。演示请求路由注册、请求处理和响应构建。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>

#include "tyke/tyke.h"

class ExampleRequestController : public tyke::RequestController, public tyke::Singleton<ExampleRequestController>
{
    friend class tyke::Singleton<ExampleRequestController>;

public:
    void RegisterMethod() override;

private:
    ExampleRequestController() = default;
    ~ExampleRequestController() override = default;

    void HandleUserLogin(const tyke::TykeRequest& request, tyke::TykeResponse& response);
    void HandleUserLogout(const tyke::TykeRequest& request, tyke::TykeResponse& response);
    void HandleDataQuery(const tyke::TykeRequest& request, tyke::TykeResponse& response);
    void HandleDataUpdate(const tyke::TykeRequest& request, tyke::TykeResponse& response);
    void HandleAsyncProcess(const tyke::TykeRequest& request, tyke::TykeResponse& response);

    void LogRequest(const tyke::TykeRequest& request, const std::string& handler_name);
    void LogResponse(const tyke::TykeResponse& response, const std::string& handler_name);
};
