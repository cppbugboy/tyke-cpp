/**
 * @file test_request_controller.h
 * @brief 测试请求控制器
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何实现自定义请求控制器。
 */

#ifndef TYKE_TEST_REQUEST_CONTROLLER_H
#define TYKE_TEST_REQUEST_CONTROLLER_H

#include "tyke/tyke.h"

/**
 * @brief 测试请求控制器类
 *
 * 演示请求控制器的基本实现，注册请求路由处理器。
 */
class TestRequestController : public tyke::RequestController, public tyke::Singleton<TestRequestController>
{
    friend class tyke::Singleton<TestRequestController>;

public:
    /**
     * @brief 注册路由方法
     *
     * 注册请求路由处理器。
     */
    void RegisterMethod() override;

    /**
     * @brief Hello路由处理器
     * @param request 请求对象
     * @param response 响应对象
     */
    static void Hello(const tyke::TykeRequest &request, tyke::TykeResponse &response);

private:
    TestRequestController()           = default;
    ~TestRequestController() override = default;
};


#endif //TYKE_TEST_REQUEST_CONTROLLER_H
