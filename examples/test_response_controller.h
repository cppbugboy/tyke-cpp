/**
 * @file test_response_controller.h
 * @brief 测试响应控制器
 * @author Nick
 * @date 2026/04/17
 *
 * 演示如何实现自定义响应控制器。
 */

#ifndef TYKE_TEST_RESPONSE_CONTROLLER_H
#define TYKE_TEST_RESPONSE_CONTROLLER_H

#include "tyke/tyke.h"

/**
 * @brief 测试响应控制器类
 *
 * 演示响应控制器的基本实现，注册响应路由处理器。
 */
class TestResponseController : public tyke::ResponseController, public tyke::Singleton<TestResponseController>
{
    friend class tyke::Singleton<TestResponseController>;

public:
    /**
     * @brief 注册路由方法
     *
     * 注册响应路由处理器。
     */
    void RegisterMethod() override;

private:
    TestResponseController() = default;
    ~TestResponseController() override = default;
};


#endif //TYKE_TEST_RESPONSE_CONTROLLER_H
