/**
 * @file response_router.cpp
 * @brief 响应路由器实现
 * @author Nick
 * @date 2026/04/17
 *
 * 实现ResponseRouter类的具体逻辑，管理响应路由表。
 */

#include "core/response_router.h"

#include "common/log_def.h"

namespace tyke
{
    /**
     * @brief 构造函数
     *
     * 初始化根路由组。
     */
    ResponseRouter::ResponseRouter()
    {
        root_group_ = std::make_shared<ResponseRouterGroup>("", &route_table_);
        LOG_DEBUG("ResponseRouter initialized");
    }

    /**
     * @brief 获取根路由组
     * @return 根路由组的共享指针
     */
    std::shared_ptr<ResponseRouterGroup> ResponseRouter::GetRoot()
    {
        return root_group_;
    }

    /**
     * @brief 根据路径获取路由条目
     * @param path 路由路径
     * @return 找到返回路由条目指针，未找到返回nullptr
     */
    ResponseRouteEntry* ResponseRouter::GetRouteEntry(const std::string& path)
    {
        auto it = route_table_.find(path);
        if (it != route_table_.end())
        {
            LOG_DEBUG("Response route entry found: path={}", path);
            return &(it->second);
        }
        LOG_WARN("Response route entry not found: path={}", path);
        return nullptr;
    }
}
