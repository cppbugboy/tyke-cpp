#include "core/message_dispatcher.h"

namespace tyke
{
    std::unordered_map<MessageType, MessageHandler>& MessageDispatcher::GetHandlers()
    {
        static std::unordered_map<MessageType, MessageHandler> handlers;
        return handlers;
    }

    void MessageDispatcher::RegisterHandler(MessageType type, MessageHandler handler)
    {
        GetHandlers()[type] = std::move(handler);
    }

    void MessageDispatcher::Dispatch(ClientId client_id, const std::vector<unsigned char>& data, uint32_t& used)
    {
        ProtocolHeader header;
        std::memcpy(&header, data.data(), sizeof(ProtocolHeader));

        auto& handlers = GetHandlers();
        auto it = handlers.find(header.msg_type);
        if (it != handlers.end())
        {
            it->second(client_id, data, used);
        }
    }

    bool MessageDispatcher::HasHandler(MessageType type)
    {
        return GetHandlers().find(type) != GetHandlers().end();
    }
}
