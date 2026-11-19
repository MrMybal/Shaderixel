#pragma once

#include "core/Json.h"

namespace sx::ai {

inline Json Schema(std::initializer_list<std::pair<const char*, Json>> props,
                   std::initializer_list<const char*> required = {}) {
    Json schema = Json::Object();
    schema["type"] = "object";
    schema["properties"] = Json::Object();
    for (const auto& [name, type] : props) schema["properties"][name] = type;
    schema["required"] = Json::Array();
    for (const auto* name : required) schema["required"].Push(name);
    return schema;
}

inline Json T(const char* type, const char* description = nullptr) {
    Json field = Json::Object();
    field["type"] = type;
    if (description) field["description"] = description;
    return field;
}

inline Json Range(const char* type, double low, double high) {
    Json field = T(type);
    field["minimum"] = low;
    field["maximum"] = high;
    return field;
}

inline Json Enum(std::initializer_list<const char*> values) {
    Json field = T("string");
    for (const char* value : values) field["enum"].Push(value);
    return field;
}

}  // namespace sx::ai
