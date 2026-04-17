/**
 * @file tyke.h
 * @brief Tyke框架主入口头文件
 * @author Nick
 * @date 2026/04/17
 *
 * 这是Tyke框架的主入口头文件，包含了框架的所有核心组件。
 * 用户只需包含此头文件即可使用框架的全部功能。
 *
 * 包含的主要组件：
 * - TykeFramework: 框架主入口类
 * - TykeRequest/TykeResponse: 请求/响应对象
 * - RequestRouter/ResponseRouter: 路由器
 * - RequestController/ResponseController: 控制器基类
 * - RequestFilter/ResponseFilter: 过滤器基类
 */

#ifndef TYKE_TYKE_H
#define TYKE_TYKE_H

#include "core/tyke_framework.h"
#include "core/tyke_request.h"
#include "core/tyke_response.h"
#include "component/singleton.hpp"
#include "core/request_router.h"
#include "controller/request_controller.h"
#include "core/response_router.h"
#include "controller/response_controller.h"
#include "controller/controller_registry.h"
#include "common/log_def.h"
#include "filter/request_filter.h"
#include "filter/response_filter.h"

#endif //TYKE_TYKE_H
