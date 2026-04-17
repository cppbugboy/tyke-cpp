/**
 * @file router_group.h
 * @brief 路由组模板类
 * @author Nick
 * @date 2026/04/17
 *
 * RouterGroup是通用的路由组模板类，支持分组路由和过滤器链。
 * 通过模板参数支持请求路由和响应路由两种场景。
 */

#ifndef TYKE_ROUTER_GROUP_H
#define TYKE_ROUTER_GROUP_H
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tyke
{
    /**
     * @brief 路由组模板类
     * @tparam FilterType 过滤器类型
     * @tparam HandlerFunc 处理器函数类型
     *
     * 支持分组路由和过滤器链的通用路由组。
     * 子组继承父组的过滤器，形成过滤器链。
     *
     * 使用示例：
     * @code
     *   auto root = std::make_shared<RouterGroup<RequestFilter, RequestHandlerFunc>>("", &route_table);
     *   auto api_group = root->AddSubGroup("/api");
     *   api_group->AddFilter(auth_filter)
     *            ->AddRouteHandler("/login", login_handler);
     * @endcode
     */
    template<typename FilterType, typename HandlerFunc>
    class RouterGroup : public std::enable_shared_from_this<RouterGroup<FilterType, HandlerFunc>>
    {
    public:
        /**
         * @brief 路由条目结构
         *
         * 包含处理器函数和过滤器链。
         */
        struct RouteEntry
        {
            HandlerFunc handler;                                    ///< 处理器函数
            std::vector<std::shared_ptr<FilterType>> filter_chain;  ///< 过滤器链
        };

        /**
         * @brief 构造函数
         * @param prefix 路由前缀
         * @param global_registry 全局路由表指针
         * @param parent 父路由组（可选）
         */
        RouterGroup(const std::string& prefix,
                    std::unordered_map<std::string, RouteEntry>* global_registry,
                    const std::shared_ptr<RouterGroup>& parent = nullptr)
            : prefix_(prefix), parent_(parent), global_registry_(global_registry)
        {
        }

        /**
         * @brief 添加过滤器
         * @param filter 过滤器共享指针
         * @return 当前路由组引用，支持链式调用
         */
        RouterGroup& AddFilter(std::shared_ptr<FilterType> filter)
        {
            filter_chain_.push_back(std::move(filter));
            return *this;
        }

        /**
         * @brief 添加子路由组
         * @param sub_prefix 子路由前缀
         * @return 子路由组的共享指针
         */
        std::shared_ptr<RouterGroup> AddSubGroup(const std::string& sub_prefix)
        {
            return std::make_shared<RouterGroup>(prefix_ + sub_prefix, global_registry_, this->shared_from_this());
        }

        /**
         * @brief 添加路由处理器
         * @param path 路由路径
         * @param handler 处理器函数
         *
         * 将完整路径和处理器注册到全局路由表，同时收集所有过滤器形成过滤器链。
         */
        void AddRouteHandler(const std::string& path, HandlerFunc handler)
        {
            const std::string full_path = prefix_ + path;
            std::vector<std::shared_ptr<FilterType>> full_chain;
            CollectFilters(full_chain);
            if (global_registry_)
            {
                (*global_registry_)[full_path] = {std::move(handler), std::move(full_chain)};
            }
        }

        /**
         * @brief 收集过滤器链
         * @param chain 输出的过滤器链
         *
         * 递归收集从根组到当前组的所有过滤器，形成完整的过滤器链。
         */
        void CollectFilters(std::vector<std::shared_ptr<FilterType>>& chain) const
        {
            if (auto p = parent_.lock())
            {
                p->CollectFilters(chain);
            }
            for (const auto& f : filter_chain_)
            {
                chain.push_back(f);
            }
        }

    private:
        std::string prefix_;                                        ///< 路由前缀
        std::vector<std::shared_ptr<FilterType>> filter_chain_;     ///< 当前组的过滤器链
        std::weak_ptr<RouterGroup> parent_;                         ///< 父路由组弱引用
        std::unordered_map<std::string, RouteEntry>* global_registry_;  ///< 全局路由表指针
    };
} // namespace tyke

#endif //TYKE_ROUTER_GROUP_H
