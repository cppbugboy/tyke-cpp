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
    /**
     * @brief 向框架的 RequestRouter 注册所有请求路由处理器。
     *
     * 在静态初始化期间通过 REQUEST_CONTROLLER_REGISTER 宏自动调用。
     * 在 /api/user、/api/data 和 /api/async 下设置路由。
     */
    void RegisterMethod();

    /**
     * @brief 处理用户登录请求。
     * @param request  包含用户名和密码的入站请求。
     * @param response 要填充登录结果和令牌的响应。
     */
    void HandleUserLogin(const tyke::Request& request, tyke::Response& response);

    /**
     * @brief 处理用户注销请求。
     * @param request  入站请求。
     * @param response 要填充注销确认的响应。
     */
    void HandleUserLogout(const tyke::Request& request, tyke::Response& response);

    /**
     * @brief 处理数据查询请求，返回示例数据集。
     * @param request  入站请求。
     * @param response 要填充查询结果的响应。
     */
    void HandleDataQuery(const tyke::Request& request, tyke::Response& response);

    /**
     * @brief 处理带字段验证的数据更新请求。
     * @param request  包含记录 ID 和更新数据的入站请求。
     * @param response 要填充更新确认的响应。
     */
    void HandleDataUpdate(const tyke::Request& request, tyke::Response& response);

    /**
     * @brief 处理异步处理请求。
     *
     * 生成任务 ID 并在响应上设置异步 UUID，
     * 以便框架可以将回复路由回正确的客户端监听器。
     *
     * @param request  入站请求。
     * @param response 要填充任务确认的响应。
     */
    void HandleAsyncProcess(const tyke::Request& request, tyke::Response& response);
}