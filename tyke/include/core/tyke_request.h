/**
 * @file tyke_request.h
 * @brief Tyke请求对象
 * @author Nick
 * @date 2026/04/17
 *
 * TykeRequest封装了请求的所有信息，包括协议头、元数据和内容数据。
 * 支持对象池管理，提供同步和异步发送方式。
 */

#ifndef TYKE_REQUEST_H
#define TYKE_REQUEST_H
#include <future>

#include "common/tyke_result.h"
#include "request_metadata.h"
#include "tyke_response.h"
#include "response_future.h"
#include "common/tyke_def.h"
#include "component/object_pool.hpp"

namespace tyke
{
    /**
     * @brief Tyke请求类
     *
     * 表示一个完整的请求对象，包含请求元数据和内容数据。
     * 支持同步发送、异步发送（回调方式）和异步发送（Future方式）三种模式。
     * 使用对象池管理内存，提高性能。
     *
     * 使用示例：
     * @code
     *   auto request = tyke::MakeRequestPtr();
     *   request->SetModule("user")
     *          ->SetRoute("/user/login")
     *          ->SetContent(tyke::ContentType::kJson, json_data);
     *   auto result = request->Send("server-uuid", response);
     * @endcode
     */
    class TykeRequest
    {
        friend class DataProc;

    public:
        /**
         * @brief 从对象池获取请求对象
         * @return 请求对象指针
         */
        static TykeRequest* Acquire();

        /**
         * @brief 将请求对象释放回对象池
         * @param req 请求对象指针
         */
        static void Release(TykeRequest* req);

        /**
         * @brief 重置请求对象状态
         *
         * 清空所有成员变量，使对象恢复到初始状态。
         */
        void Reset();

        /**
         * @brief 获取协议魔数
         * @return 4字节魔数字符串
         */
        const char* GetMagic() const;

        /**
         * @brief 获取消息类型
         * @return 消息类型枚举值
         */
        MessageType GetMessageType() const;

        /**
         * @brief 设置请求内容
         * @param content_type 内容类型
         * @param content 内容数据
         * @return 当前请求引用，支持链式调用
         */
        TykeRequest& SetContent(const ContentType& content_type, const std::vector<unsigned char>& content);

        /**
         * @brief 获取请求内容
         * @param content_type 输出内容类型字符串
         * @param content 输出内容数据
         */
        void GetContent(std::string& content_type, std::vector<unsigned char>& content) const;

        /**
         * @brief 设置模块名称
         * @param module 模块名称
         * @return 当前请求引用，支持链式调用
         */
        TykeRequest& SetModule(const std::string& module);

        /**
         * @brief 获取模块名称
         * @return 模块名称字符串
         */
        std::string GetModule() const;

        /**
         * @brief 设置路由路径
         * @param route 路由路径，如"/user/login"
         * @return 当前请求引用，支持链式调用
         */
        TykeRequest& SetRoute(const std::string& route);

        /**
         * @brief 获取路由路径
         * @return 路由路径字符串
         */
        std::string GetRoute() const;

        /**
         * @brief 获取消息UUID
         * @return 消息UUID字符串
         */
        std::string GetMsgUuid() const;

        /**
         * @brief 获取异步UUID
         * @return 异步UUID字符串
         */
        std::string GetAsyncUuid() const;

        /**
         * @brief 添加自定义元数据
         * @param key 元数据键名
         * @param value 元数据值
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> AddMetadata(const std::string& key, const JsonValue& value);

        /**
         * @brief 获取自定义元数据
         * @param key 元数据键名
         * @return 存在返回值，不存在返回nullopt
         */
        nonstd::optional<JsonValue> GetMetadata(const std::string& key);

        /**
         * @brief 同步发送请求
         * @param send_uuid 目标服务器UUID
         * @param response 输出响应对象
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> Send(const std::string& send_uuid, TykeResponse& response);

        /**
         * @brief 异步发送请求（无回调）
         * @param send_uuid 目标服务器UUID
         * @param recv_uuid 接收响应的服务器UUID（暂未使用）
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> SendAsync(const std::string& send_uuid, const std::string& recv_uuid);

        /**
         * @brief 异步发送请求（回调方式）
         * @param send_uuid 目标服务器UUID
         * @param func 响应回调函数
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> SendAsyncWithFunc(const std::string& send_uuid,
                                                              std::function<void(TykeResponse &)> func);

        /**
         * @brief 异步发送请求（Future方式）
         * @param send_uuid 目标服务器UUID
         * @param recv_uuid 接收响应的服务器UUID（暂未使用）
         * @return 成功返回ResponseFuture对象，失败返回错误信息
         */
        nonstd::expected<ResponseFuture, std::string> SendAsyncWithFuture(
            const std::string& send_uuid, const std::string& recv_uuid);

    private:
        /**
         * @brief 编码并发送请求
         * @param send_uuid 目标服务器UUID
         * @param msg_type 消息类型
         * @return 成功返回true，失败返回错误信息
         */
        nonstd::expected<bool, std::string> EncodeAndSend(const std::string& send_uuid, MessageType msg_type);

        ProtocolHeader protocol_header_;           ///< 协议头
        RequestMetadata metadata_;                  ///< 请求元数据
        std::vector<unsigned char> content_;        ///< 内容数据
        std::function<void(TykeResponse &)> async_func_;  ///< 异步回调函数

        static ObjectPool<TykeRequest> pool_;       ///< 对象池实例
    };

    /**
     * @brief TykeRequest对象池删除器
     *
     * 用于unique_ptr自动将对象释放回对象池。
     */
    struct TykeRequestDeleter
    {
        void operator()(TykeRequest* p) const
        {
            TykeRequest::Release(p);
        }
    };

    /// TykeRequest的unique_ptr类型别名
    using TykeRequestPtr = std::unique_ptr<TykeRequest, TykeRequestDeleter>;

    /**
     * @brief 从对象池获取请求对象并包装为unique_ptr
     * @return 包装了请求对象的unique_ptr
     */
    inline TykeRequestPtr MakeRequestPtr()
    {
        return TykeRequestPtr(TykeRequest::Acquire());
    }
} // tyke

#endif //TYKE_REQUEST_H