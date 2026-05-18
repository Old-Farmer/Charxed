#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "json.h"

namespace charxed {

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/

// NOTE: We follow the naming convention of LSP to make life easier.

// TODO: See
// https://github.com/microsoft/vscode-languageserver-node/blob/main/protocol/metaModel.json
// Generated type def and (de)serilization functions according to this file
// using a Python script

// Base Protocol JSON structures
// ----------------------------

// Base Types
struct null {};
using string = std::string;
using boolean = bool;
using integer = int32_t;
using uinteger = uint32_t;
using decimal = double;  // ?

// Abstract Message

struct Message {
    string jsonrpc = "2.0";
};

// Request Message

struct RequestMessage : Message {
    std::variant<integer, string> id;
    string method;
    std::optional<Json> prarams;
};

// Response Message

namespace ErrorCodesNS {
// JSON-RPC defined
constexpr integer ParseError = -32700;
constexpr integer InvalidRequest = -32600;
constexpr integer MethodNotFound = -32601;
constexpr integer InvalidParams = -32602;
constexpr integer InternalError = -32603;

// JSON-RPC reserved range
constexpr integer jsonrpcReservedErrorRangeStart = -32099;

constexpr integer ServerNotInitialized = -32002;
constexpr integer UnknownErrorCode = -32001;

constexpr integer jsonrpcReservedErrorRangeEnd = -32000;

// LSP reserved range
constexpr integer lspReservedErrorRangeStart = -32899;

constexpr integer RequestFailed = -32803;
constexpr integer ServerCancelled = -32802;
constexpr integer ContentModified = -32801;
constexpr integer RequestCancelled = -32800;

constexpr integer lspReservedErrorRangeEnd = -32800;
}  // namespace ErrorCodesNS

struct ResponseError {
    integer code;
    string message;
    std::optional<Json> data;
};

struct ResponseMessage : Message {
    std::variant<integer, string, null> id;
    std::optional<Json> result;
    std::optional<ResponseError> error;
};

// Notification Message

struct NotificationMessage : Message {
    string method;
    std::optional<Json> params;
};

// Cancellation Support
struct CancelParams {
    std::variant<integer, string> id;
};

// Progress Support

using ProgressToken = std::variant<integer, string>;

template <typename T>
struct ProgressParams {
    ProgressToken token;
    T value;
};

}  // namespace charxed
