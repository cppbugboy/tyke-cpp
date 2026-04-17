/**
 * @file response_future.h
 * @brief 响应Future对象
 * @author Nick
 * @date 2026/04/17
 *
 * ResponseFuture封装了异步请求的Future对象，用于获取异步响应结果。
 */

#ifndef TYKE_RESPONSE_FUTURE_H
#define TYKE_RESPONSE_FUTURE_H
#include <future>
#include <string>

#include "tyke_response.h"

namespace tyke
{
    /**
     * @brief 响应Future类
     *
     * 封装std::future<TykeResponse>，提供异步响应获取功能。
     * 用于SendAsyncWithFuture方式的异步请求。
     *
     * 使用示例：
     * @code
     *   auto future_result = request->SendAsyncWithFuture("server-uuid", "");
     *   if (future_result) {
     *       TykeResponse response = future_result.value().GetResponse();
     *       // 处理响应
     *   }
     * @endcode
     */
    class ResponseFuture
    {
    public:
        /**
         * @brief 构造函数
         * @param msg_uuid 消息UUID
         * @param future Future对象
         */
        ResponseFuture(const std::string& msg_uuid, std::future<TykeResponse> future);

        /**
         * @brief 获取响应结果
         * @return 响应对象
         *
         * 阻塞等待响应到达，获取后自动清理RequestStub中的条目。
         */
        TykeResponse GetResponse();

    private:
        std::string msg_uuid_;                  ///< 消息UUID
        std::future<TykeResponse> future_;      ///< Future对象
    };
} // tyke

#endif //TYKE_RESPONSE_FUTURE_H