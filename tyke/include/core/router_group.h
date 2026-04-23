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
    template <typename FilterType, typename HandlerFunc>
    class RouterGroup : public std::enable_shared_from_this<RouterGroup<FilterType, HandlerFunc>>
    {
    public:
        struct RouteEntry
        {
            HandlerFunc handler;
            std::vector<std::shared_ptr<FilterType>> filter_chain;
        };


        RouterGroup(std::string_view prefix,
                    std::unordered_map<std::string, RouteEntry>* global_registry,
                    const std::shared_ptr<RouterGroup>& parent = nullptr)
            : prefix_(prefix), parent_(parent), global_registry_(global_registry)
        {
        }


        RouterGroup& AddFilter(std::shared_ptr<FilterType> filter)
        {
            filter_chain_.push_back(std::move(filter));
            return *this;
        }


        std::shared_ptr<RouterGroup> Group(const std::string& sub_prefix)
        {
            if (sub_groups_.find(sub_prefix) == sub_groups_.end())
            {
                auto group_shared = std::make_shared<RouterGroup>(prefix_ + sub_prefix, global_registry_,
                                                                  this->shared_from_this());
                sub_groups_[sub_prefix] = group_shared;
            }
            return sub_groups_.at(sub_prefix);
        }


        void Route(const std::string& path, HandlerFunc handler)
        {
            const std::string full_path = prefix_ + path;
            std::vector<std::shared_ptr<FilterType>> full_chain;
            CollectFilters(full_chain);
            if (global_registry_)
            {
                (*global_registry_)[full_path] = {std::move(handler), std::move(full_chain)};
            }
        }

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
}