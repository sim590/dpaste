#include "ipc.h"

namespace dpaste {
namespace ipc {

void ParsedMessage::msgpack_unpack(msgpack::object msg) {
    auto e = dht::findMapValue(msg, "e");
    auto r = dht::findMapValue(msg, "r");

    std::string q;
    if (auto rq = dht::findMapValue(msg, "q")) {
        if (rq->type != msgpack::type::STR)
            throw msgpack::type_error();
        q = rq->as<std::string>();
    }

    if (e)
        type = MessageType::Error;
    else if (r)
        type = MessageType::Reply;
    else if (q == "paste")
        type = MessageType::Paste;
    else if (q == "get")
        type = MessageType::Get;

    auto d = dht::findMapValue(msg, "d");
    auto& content = d ? *d : (r ? *r : *e);
    if (auto c = dht::findMapValue(content, "code"))
        code = c->as<std::string>();
    if (auto p = dht::findMapValue(content, "paste"))
        paste_value = p->as<dht::Blob>();
}

} /* ipc  */
} /* dpaste */

