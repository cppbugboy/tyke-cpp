/**
 * @file response_router.h
 * @brief 响应路由器
 * @author Nick
 * @date 2026/04/17
 *
 * ResponseRouter管理响应路由表，提供路由注册和查找功能。
 * 采用单例模式，支持分组路由和过滤器链。
 */

#ifndef TYKE_RESPONSE_ROUTER_H
#define TYKE_RESPONSE_ROUTER_H
#include "response_router_group.h"
#include "component/singleton.hpp"

namespace tyke
{
/// 响应路由器单例访问宏
#define RESPONSE_ROUTER_INSTANCE ResponseRouter::GetInstance()

    /**
     * @brief 响应路由器类
     *
     * 管理响应路由表，支持分组路由和过滤器链。
     * 使用RouterGroup构建路由树，最终拍平为路由表进行快速查找。
     *
     * 使用示例：
     * @code
     *   auto root = RESPONSE_ROUTER_INSTANCE->GetRoot();
     *   auto group = root->AddSubGroup("/api");
     *   group->AddFilter(std::make_shared<LogFilter>())
     *         ->AddRouteHandler("/callback", CallbackHandler);
     * @endcode
     */
    class ResponseRouter : public Singleton<ResponseRouter>
    {
        friend class Singleton<ResponseRouter>;

    public:
        /**
         * @brief 获取根路由组
         * @return 根路由组的共享指针
         */
        std::shared_ptr<ResponseRouterGroup> GetRoot();

        /**
         * @brief 根据路径获取路由条目
         * @param path 路由路径
         * @return 找到返回路由条目指针，未找到返回nullptr
         */
        ResponseRouteEntry* GetRouteEntry(const std::string& path);

    private:
        ResponseRouter();
        ~ResponseRouter() override = default;

        std::unordered_map<std::string, ResponseRouteEntry> route_table_;  ///< 路由表：路径 -> 路由条目
        std::shared_ptr<ResponseRouterGroup> root_group_;                   ///< 根路由组
    };
}
#endif //TYKE_RESPONSE_ROUTER_H