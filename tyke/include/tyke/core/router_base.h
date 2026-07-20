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
    /** @brief 路由器模板基类 (CRTP模式)。
     * @tparam RouterGroupType 路由分组类型（RequestRouterGroup 或 ResponseRouterGroup）。
     *
     * 管理路由表（全局 flat map）和根分组树。路由匹配为 O(1) 哈希查找。
     */
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

        /** @brief 获取根路由分组，用于构建分组树。 */
        std::shared_ptr<RouterGroupType> GetRoot()
        {
            return root_group_;
        }

        /** @brief 按完整路径查找路由条目。
         * @param path 路由完整路径。
         * @return 指向 RouteEntry 的指针，未找到返回 nullptr。
         * @note 使用 C++17 if-init-statement 优化查找。
         */
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