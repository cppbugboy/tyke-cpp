/**
 * @file example_request_controller.h
 * @brief 示例请求控制器声明。演示请求路由注册、请求处理和响应构建。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <string>

#include "tyke/tyke.h"

namespace controller::request::examples
{
    void RegisterMethod();
    void HandleUserLogin(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr);
    void HandleUserLogout(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr);
    void HandleDataQuery(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr);
    void HandleDataUpdate(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr);
    void HandleAsyncProcess(const tyke::TykeRequest& request, tyke::TykeResponse& response, const tyke::ContextPtr& context_ptr);
}

// class ExampleRequestController : public tyke::RequestController, public tyke::Singleton<ExampleRequestController>
// {
//     friend class tyke::Singleton<ExampleRequestController>;
//
// public:
//     void RegisterMethod() override;
//
// private:
//     ExampleRequestController() = default;
//     ~ExampleRequestController() override = default;
//
//     void HandleUserLogin(const tyke::TykeRequest& request, tyke::TykeResponse& response, std::shared_ptr<tyke::Context> context);
//     void HandleUserLogout(const tyke::TykeRequest& request, tyke::TykeResponse& response, std::shared_ptr<tyke::Context> context);
//     void HandleDataQuery(const tyke::TykeRequest& request, tyke::TykeResponse& response, std::shared_ptr<tyke::Context> context);
//     void HandleDataUpdate(const tyke::TykeRequest& request, tyke::TykeResponse& response, std::shared_ptr<tyke::Context> context);
//     void HandleAsyncProcess(const tyke::TykeRequest& request, tyke::TykeResponse& response, std::shared_ptr<tyke::Context> context);
// };