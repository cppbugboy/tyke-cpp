/**
 * @file router_base.h
 * @brief 路由器模板基类
 * @author Nick
 * @date 2026/04/19
 *
 * 使用CRTP模式提取请求/响应路由器的公共逻辑，包括路由表管理和路由查找功能。
 */

#ifndef TYKE_ROUTER_BASE_H
#define TYKE_ROUTER_BASE_H

#include <memory>
#include <string>
#include <unordered_map>

#include "common/log_def.h"
#include "component/singleton.h"

namespace tyke
{
    template<typename RouterGroupType, typename Derived>
    class RouterBase : public Singleton<Derived>
    {
        friend class Singleton<Derived>;

    public:
        using RouteEntry = typename RouterGroupType::RouteEntry;

        std::shared_ptr<RouterGroupType> GetRoot()
        {
            return root_group_;
        }

        RouteEntry* GetRouteEntry(const std::string& path)
        {
            auto it = route_table_.find(path);
            if (it != route_table_.end())
            {
                LOG_DEBUG("Route entry found: path={}", path);
                return &(it->second);
            }
            LOG_WARN("Route entry not found: path={}", path);
            return nullptr;
        }

    protected:
        RouterBase()
        {
            root_group_ = std::make_shared<RouterGroupType>("", &route_table_);
            LOG_DEBUG("RouterBase initialized");
        }

        ~RouterBase() = default;

        std::unordered_map<std::string, RouteEntry> route_table_;
        std::shared_ptr<RouterGroupType> root_group_;
    };
}

#endif
