/**
 * @file tyke.h
 * @brief Tyke框架统一入口头文件。用户只需引入此文件即可使用全部公开API。
 * @author Nick
 * @date 2026/04/19
 *
 * @details
 * 本文件按模块聚合所有公开头文件：
 * - core: 请求/响应处理、路由、拦截器、框架核心
 * - common: 日志宏、类型定义
 * - component: 单例模式基类
 * - ipc: IPC客户端连接池工厂
 */

#pragma once

// --- Core: 请求/响应处理、路由与拦截器 ---
#include "core/request_controller.h"
#include "core/response_controller.h"
#include "core/framework.h"
#include "core/request.h"
#include "core/request_router.h"
#include "core/response.h"
#include "core/response_router.h"
#include "core/request_filter.h"
#include "core/response_filter.h"

// --- Common: 日志宏 ---
#include "common/log_def.h"

// --- Component: 单例模式 ---
#include "component/singleton.h"

// --- IPC: 连接池工厂（提供IPC客户端连接的池化管理） ---
#include "ipc/connection_pool_factory.h"
