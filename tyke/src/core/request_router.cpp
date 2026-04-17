/**
 * @file request_router.cpp
 * @brief 请求路由器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现RequestRouter类的具体逻辑，管理请求路由表。
 */

#include "core/request_router.h"

#include "common/log_def.h"

namespace tyke
{
    /**
     * @brief 构造函数
     *
     * 初始化根路由组。
     */
    RequestRouter::RequestRouter()
    {
        root_group_ = std::make_shared<RequestRouterGroup>("", &route_table_);
        LOG_DEBUG("RequestRouter initialized");
    }

    /**
     * @brief 获取根路由组
     * @return 根路由组的共享指针
     */
    std::shared_ptr<RequestRouterGroup> RequestRouter::GetRoot()
    {
        return root_group_;
    }

    /**
     * @brief 根据路径获取路由条目
     * @param path 路由路径
     * @return 找到返回路由条目指针，未找到返回nullptr
     */
    RequestRouteEntry* RequestRouter::GetRouteEntry(const std::string& path)
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
} // tyke