/**
 * @file request_router.h
 * @brief 请求路由器
 * @author Nick
 * @date 2026/04/17
 *
 * RequestRouter管理请求路由表，提供路由注册和查找功能。
 * 采用单例模式，支持分组路由和过滤器链。
 */

#ifndef TYKE_ROUTER_H
#define TYKE_ROUTER_H
#include <unordered_map>

#include "request_router_group.h"
#include "tyke_request.h"
#include "component/singleton.hpp"

namespace tyke
{
/// 请求路由器单例访问宏
#define REQUEST_ROUTER_INSTANCE RequestRouter::GetInstance()

    /**
     * @brief 请求路由器类
     *
     * 管理请求路由表，支持分组路由和过滤器链。
     * 使用RouterGroup构建路由树，最终拍平为路由表进行快速查找。
     *
     * 使用示例：
     * @code
     *   auto root = REQUEST_ROUTER_INSTANCE->GetRoot();
     *   auto group = root->AddSubGroup("/api");
     *   group->AddFilter(std::make_shared<AuthFilter>())
     *         ->AddRouteHandler("/login", LoginHandler);
     * @endcode
     */
    class RequestRouter : public Singleton<RequestRouter>
    {
        friend class Singleton<RequestRouter>;

    public:
        /**
         * @brief 获取根路由组
         * @return 根路由组的共享指针
         */
        std::shared_ptr<RequestRouterGroup> GetRoot();

        /**
         * @brief 根据路径获取路由条目
         * @param path 路由路径
         * @return 找到返回路由条目指针，未找到返回nullptr
         */
        RequestRouteEntry* GetRouteEntry(const std::string& path);

    private:
        RequestRouter();
        ~RequestRouter() override = default;

        std::unordered_map<std::string, RequestRouteEntry> route_table_;  ///< 路由表：路径 -> 路由条目
        std::shared_ptr<RequestRouterGroup> root_group_;                   ///< 根路由组
    };
} // tyke

#endif //TYKE_ROUTER_H