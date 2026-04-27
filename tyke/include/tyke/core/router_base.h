/**
 * @file router_base.h
 * @brief 路由器模板基类 (C++17)
 * @author Nick
 * @date 2026/04/19
 *
 * 使用CRTP模式提取请求/响应路由器的公共逻辑，包括路由表管理和路由查找功能。
 *
 * C++17特性:
 * - 使用std::string_view优化GetRouteEntry参数
 * - 使用if初始化语句简化find操作
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "tyke/common/log_def.h"

namespace tyke
{
    template <typename RouterGroupType>
    class RouterBase
    {
    public:
        using RouteEntry = typename RouterGroupType::RouteEntry;

        RouterBase()
        {
            root_group_ = std::make_shared<RouterGroupType>("", &route_table_);
            LOG_DEBUG("RouterBase initialized");
        }

        ~RouterBase() = default;

        std::shared_ptr<RouterGroupType> GetRoot()
        {
            return root_group_;
        }

        RouteEntry* GetRouteEntry(std::string_view path)
        {
            if (auto it = route_table_.find(std::string(path)); it != route_table_.end())
            {
                LOG_DEBUG("Route entry found: path={}", path);
                return &(it->second);
            }
            LOG_WARN("Route entry not found: path={}", path);
            return nullptr;
        }

    private:
        std::unordered_map<std::string, RouteEntry> route_table_;
        std::shared_ptr<RouterGroupType> root_group_;
    };
} // namespace tyke