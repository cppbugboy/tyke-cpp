/**
 * @file router_group.h
 * @brief 路由分组模板基类声明。使用CRTP模式提取请求/响应路由分组的公共逻辑。
 * @author Nick
 * @date 2026/04/19
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tyke
{
    /** @brief 路由分组模板。支持层级分组、过滤器链式挂载和路由注册。
     * @tparam FilterType 过滤器类型（RequestFilter 或 ResponseFilter）。
     * @tparam HandlerFunc 处理函数类型。
     *
     * 每个分组维护独立的前缀和过滤器链。添加路由时，从根到当前节点收集全部过滤器。
     */
    template <typename FilterType, typename HandlerFunc>
    class RouterGroup : public std::enable_shared_from_this<RouterGroup<FilterType, HandlerFunc>>
    {
    public:
        /** @brief 路由条目：绑定处理函数和其所属的过滤器链。 */
        struct RouteEntry
        {
            HandlerFunc handler;
            std::vector<std::shared_ptr<FilterType>> filter_chain;
        };

        /** @brief 构造路由分组。
         * @param prefix 路由路径前缀，用于拼接完整路由。
         * @param global_registry 指向全局路由表的指针，注册时写入。
         * @param parent 父分组（weak_ptr），用于向上收集过滤器。
         */
        RouterGroup(const std::string_view prefix, std::unordered_map<std::string, RouteEntry>* global_registry,
                    const std::shared_ptr<RouterGroup>& parent = nullptr)
            : prefix_(prefix), parent_(parent), global_registry_(global_registry)
        {
        }

        /** @brief 向当前分组添加过滤器。
         * @param filter 过滤器 shared_ptr。
         * @return 自身引用，支持链式调用。
         */
        RouterGroup& AddFilter(std::shared_ptr<FilterType> filter)
        {
            filter_chain_.push_back(std::move(filter));
            return *this;
        }

        /** @brief 创建子分组。
         * @param sub_prefix 子分组路径后缀（自动拼接父前缀）。
         * @return 子分组 shared_ptr。
         * @note 子分组共享全局路由表，向上收集父分组的过滤器。
         */
        std::shared_ptr<RouterGroup> AddSubGroup(const std::string& sub_prefix)
        {
            if (sub_groups_.find(sub_prefix) == sub_groups_.end())
            {
                auto group_shared =
                    std::make_shared<RouterGroup>(prefix_ + sub_prefix, global_registry_, this->shared_from_this());
                sub_groups_[sub_prefix] = group_shared;
            }
            return sub_groups_.at(sub_prefix);
        }

        /** @brief 注册路由处理器到当前分组。
         * @param path 路由路径（相对于分组前缀）。
         * @param handler 处理函数。
         * @note 注册时自动收集从根到当前节点的全部过滤器，构建完整过滤器链。
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

        /** @brief 沿父指针向上递归收集所有过滤器。
         * @param chain 输出收集到的过滤器链（按从根到当前分组的顺序）。
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
        std::string prefix_;
        std::vector<std::shared_ptr<FilterType>> filter_chain_;
        std::weak_ptr<RouterGroup> parent_;
        std::unordered_map<std::string, RouteEntry>* global_registry_;
        std::unordered_map<std::string, std::shared_ptr<RouterGroup>> sub_groups_;
    };
} // namespace tyke