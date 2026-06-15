// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "json2pb/json_to_pb_sonic.h"

#ifdef BRPC_WITH_SONIC_JSON

#include <cinttypes>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include <gflags/gflags.h>
#include <google/protobuf/descriptor.h>
#include "butil/base64.h"
#include "butil/strings/string_piece.h"
#include "butil/strings/string_number_conversions.h"
#include "butil/strings/string_util.h"
#include "json2pb/encode_decode.h"
#include "json2pb/protobuf_map.h"
#include "sonic/dom/generic_document.h"
#include "sonic/dom/genericnode.h"
#include "sonic/error.h"

namespace json2pb {

namespace {

template <typename JsonValue, typename = void>
struct SonicHasStringNumber : std::false_type {};

template <typename JsonValue>
struct SonicHasStringNumber<
    JsonValue,
    decltype(static_cast<void>(std::declval<const JsonValue&>().IsStringNumber()))>
    : std::true_type {};

template <typename JsonValue>
inline bool IsStringNumberCompat(const JsonValue& value) {
    if constexpr (SonicHasStringNumber<JsonValue>::value) {
        return value.IsStringNumber();
    }
    return false;
}

inline int Json2PbMaxRecursionDepth() {
    std::string value;
    if (GFLAGS_NAMESPACE::GetCommandLineOption("json2pb_max_recursion_depth",
                                               &value)) {
        int depth = 100;
        if (butil::StringToInt(value, &depth)) {
            return depth;
        }
    }
    return 100;
}

inline const google::protobuf::Message* ToMessagePtr(
    const google::protobuf::Message* pb) {
    return pb;
}

inline const google::protobuf::Message* ToMessagePtr(std::nullptr_t) {
    return nullptr;
}

#define J2PERROR_WITH_PB(pb, perr, fmt, ...)                                   \
    if (perr) {                                                                \
        if (!perr->empty()) {                                                  \
            perr->append(", ", 2);                                             \
        }                                                                      \
        butil::string_appendf(perr, fmt, ##__VA_ARGS__);                       \
        const google::protobuf::Message* _pb_for_error = ToMessagePtr(pb);     \
        if (_pb_for_error != nullptr) {                                        \
            butil::string_appendf(perr, " [%s]",                               \
                                  _pb_for_error->GetDescriptor()->name().c_str()); \
        }                                                                      \
    } else { }

template <typename JsonValue>
void string_append_value_sonic(const JsonValue& value, std::string* output) {
    if (value.IsNull()) {
        output->append("null");
    } else if (value.IsBool()) {
        output->append(value.GetBool() ? "true" : "false");
    } else if (value.IsDouble()) {
        butil::string_appendf(output, "%f", value.GetDouble());
    } else if (value.IsUint64()) {
        butil::string_appendf(output, "%" PRIu64, value.GetUint64());
    } else if (value.IsInt64()) {
        butil::string_appendf(output, "%" PRId64, value.GetInt64());
    } else if (value.IsString()) {
        output->push_back('"');
        sonic_json::StringView sv = value.GetStringView();
        output->append(sv.data(), sv.size());
        output->push_back('"');
    } else if (value.IsArray()) {
        output->append("array");
    } else if (value.IsObject()) {
        output->append("object");
    }
}

template <typename JsonValue>
bool value_invalid_sonic(const google::protobuf::FieldDescriptor* field,
                         const char* type,
                         const JsonValue& value,
                         std::string* err) {
    bool optional = field->is_optional();
    if (err) {
        if (!err->empty()) {
            err->append(", ");
        }
        err->append("Invalid value `");
        string_append_value_sonic(value, err);
        butil::string_appendf(err,
                              "' for %sfield `%s' which SHOULD be %s",
                              optional ? "optional " : "",
                              field->full_name().c_str(),
                              type);
    }
    if (!optional) {
        return false;
    }
    return true;
}

template <typename T, typename JsonValue>
bool convert_string_to_double_float_type_sonic(
    void (google::protobuf::Reflection::*func)(
        google::protobuf::Message* message,
        const google::protobuf::FieldDescriptor* field, T value) const,
    google::protobuf::Message* message,
    const google::protobuf::FieldDescriptor* field,
    const google::protobuf::Reflection* reflection,
    const JsonValue& item,
    std::string* err) {
    sonic_json::StringView sv = item.GetStringView();
    std::string limit_type(sv.data(), sv.size());
    if (std::numeric_limits<T>::has_quiet_NaN &&
        strcasecmp(limit_type.c_str(), "NaN") == 0) {
        (reflection->*func)(message, field, std::numeric_limits<T>::quiet_NaN());
        return true;
    }
    if (std::numeric_limits<T>::has_infinity &&
        strcasecmp(limit_type.c_str(), "Infinity") == 0) {
        (reflection->*func)(message, field, std::numeric_limits<T>::infinity());
        return true;
    }
    if (std::numeric_limits<T>::has_infinity &&
        strcasecmp(limit_type.c_str(), "-Infinity") == 0) {
        (reflection->*func)(message, field, -std::numeric_limits<T>::infinity());
        return true;
    }
    return value_invalid_sonic(field, typeid(T).name(), item, err);
}

template <typename JsonValue>
bool convert_float_type_sonic(const JsonValue& item, bool repeated,
                              google::protobuf::Message* message,
                              const google::protobuf::FieldDescriptor* field,
                              const google::protobuf::Reflection* reflection,
                              std::string* err) {
    if (item.IsNumber() && !IsStringNumberCompat(item)) {
        if (repeated) {
            reflection->AddFloat(message, field, item.GetDouble());
        } else {
            reflection->SetFloat(message, field, item.GetDouble());
        }
    } else if (item.IsString()) {
        if (!convert_string_to_double_float_type_sonic<float>(
                (repeated ? &google::protobuf::Reflection::AddFloat
                          : &google::protobuf::Reflection::SetFloat),
                message, field, reflection, item, err)) {
            return false;
        }
    } else {
        return value_invalid_sonic(field, "float", item, err);
    }
    return true;
}

template <typename JsonValue>
bool convert_double_type_sonic(const JsonValue& item, bool repeated,
                               google::protobuf::Message* message,
                               const google::protobuf::FieldDescriptor* field,
                               const google::protobuf::Reflection* reflection,
                               std::string* err) {
    if (item.IsNumber() && !IsStringNumberCompat(item)) {
        if (repeated) {
            reflection->AddDouble(message, field, item.GetDouble());
        } else {
            reflection->SetDouble(message, field, item.GetDouble());
        }
    } else if (item.IsString()) {
        if (!convert_string_to_double_float_type_sonic<double>(
                (repeated ? &google::protobuf::Reflection::AddDouble
                          : &google::protobuf::Reflection::SetDouble),
                message, field, reflection, item, err)) {
            return false;
        }
    } else {
        return value_invalid_sonic(field, "double", item, err);
    }
    return true;
}

template <typename JsonValue>
bool convert_enum_type_sonic(const JsonValue& item, bool repeated,
                             google::protobuf::Message* message,
                             const google::protobuf::FieldDescriptor* field,
                             const google::protobuf::Reflection* reflection,
                             std::string* err) {
    const google::protobuf::EnumValueDescriptor* enum_value_descriptor = NULL;
    if (item.IsInt64()) {
        int64_t enum_num = item.GetInt64();
        if (enum_num >= std::numeric_limits<int>::min() &&
            enum_num <= std::numeric_limits<int>::max()) {
            enum_value_descriptor =
                field->enum_type()->FindValueByNumber(static_cast<int>(enum_num));
        }
    } else if (item.IsString()) {
        sonic_json::StringView sv = item.GetStringView();
        enum_value_descriptor = field->enum_type()->FindValueByName(
            std::string(sv.data(), sv.size()));
    }
    if (!enum_value_descriptor) {
        return value_invalid_sonic(field, "enum", item, err);
    }
    if (repeated) {
        reflection->AddEnum(message, field, enum_value_descriptor);
    } else {
        reflection->SetEnum(message, field, enum_value_descriptor);
    }
    return true;
}

template <typename JsonValue>
bool convert_int64_type_sonic(const JsonValue& item, bool repeated,
                              google::protobuf::Message* message,
                              const google::protobuf::FieldDescriptor* field,
                              const google::protobuf::Reflection* reflection,
                              std::string* err) {
    int64_t num;
    if (item.IsInt64()) {
        if (repeated) {
            reflection->AddInt64(message, field, item.GetInt64());
        } else {
            reflection->SetInt64(message, field, item.GetInt64());
        }
    } else if (item.IsString() &&
               butil::StringToInt64(butil::StringPiece(
                                        item.GetStringView().data(),
                                        item.GetStringView().size()),
                                    &num)) {
        if (repeated) {
            reflection->AddInt64(message, field, num);
        } else {
            reflection->SetInt64(message, field, num);
        }
    } else {
        return value_invalid_sonic(field, "INT64", item, err);
    }
    return true;
}

template <typename JsonValue>
bool convert_uint64_type_sonic(const JsonValue& item, bool repeated,
                               google::protobuf::Message* message,
                               const google::protobuf::FieldDescriptor* field,
                               const google::protobuf::Reflection* reflection,
                               std::string* err) {
    uint64_t num;
    if (item.IsUint64()) {
        if (repeated) {
            reflection->AddUInt64(message, field, item.GetUint64());
        } else {
            reflection->SetUInt64(message, field, item.GetUint64());
        }
    } else if (item.IsString() &&
               butil::StringToUint64(butil::StringPiece(
                                         item.GetStringView().data(),
                                         item.GetStringView().size()),
                                     &num)) {
        if (repeated) {
            reflection->AddUInt64(message, field, num);
        } else {
            reflection->SetUInt64(message, field, num);
        }
    } else {
        return value_invalid_sonic(field, "UINT64", item, err);
    }
    return true;
}

template <typename JsonValue>
bool JsonValueToProtoMessageSonic(const JsonValue& json_value,
                                  google::protobuf::Message* message,
                                  const Json2PbOptions& options,
                                  std::string* err,
                                  int depth);

template <typename JsonValue>
bool JsonValueToProtoFieldSonic(const JsonValue& value,
                                const google::protobuf::FieldDescriptor* field,
                                google::protobuf::Message* message,
                                const Json2PbOptions& options,
                                std::string* err,
                                int depth) {
    if (value.IsNull()) {
        if (field->is_required()) {
            J2PERROR_WITH_PB(nullptr, err, "Missing required field: %s",
                             field->full_name().c_str());
            return false;
        }
        return true;
    }

    if (field->is_repeated() && !value.IsArray()) {
        J2PERROR_WITH_PB(nullptr, err, "Invalid value for repeated field: %s",
                         field->full_name().c_str());
        return false;
    }

    const google::protobuf::Reflection* reflection = message->GetReflection();
    switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!it->IsInt64() ||
                    it->GetInt64() < std::numeric_limits<int32_t>::min() ||
                    it->GetInt64() > std::numeric_limits<int32_t>::max()) {
                    if (!value_invalid_sonic(field, "INT32", *it, err)) {
                        return false;
                    }
                    continue;
                }
                reflection->AddInt32(message, field, static_cast<int32_t>(it->GetInt64()));
            }
        } else {
            if (!value.IsInt64() ||
                value.GetInt64() < std::numeric_limits<int32_t>::min() ||
                value.GetInt64() > std::numeric_limits<int32_t>::max()) {
                return value_invalid_sonic(field, "INT32", value, err);
            }
            reflection->SetInt32(message, field, static_cast<int32_t>(value.GetInt64()));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!it->IsUint64() ||
                    it->GetUint64() > std::numeric_limits<uint32_t>::max()) {
                    if (!value_invalid_sonic(field, "UINT32", *it, err)) {
                        return false;
                    }
                    continue;
                }
                reflection->AddUInt32(message, field,
                                      static_cast<uint32_t>(it->GetUint64()));
            }
        } else {
            if (!value.IsUint64() ||
                value.GetUint64() > std::numeric_limits<uint32_t>::max()) {
                return value_invalid_sonic(field, "UINT32", value, err);
            }
            reflection->SetUInt32(message, field,
                                  static_cast<uint32_t>(value.GetUint64()));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!it->IsBool()) {
                    if (!value_invalid_sonic(field, "BOOL", *it, err)) {
                        return false;
                    }
                    continue;
                }
                reflection->AddBool(message, field, it->GetBool());
            }
        } else {
            if (!value.IsBool()) {
                return value_invalid_sonic(field, "BOOL", value, err);
            }
            reflection->SetBool(message, field, value.GetBool());
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!convert_int64_type_sonic(*it, true, message, field, reflection, err)) {
                    return false;
                }
            }
        } else if (!convert_int64_type_sonic(value, false, message, field, reflection, err)) {
            return false;
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!convert_uint64_type_sonic(*it, true, message, field, reflection, err)) {
                    return false;
                }
            }
        } else if (!convert_uint64_type_sonic(value, false, message, field, reflection, err)) {
            return false;
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!convert_float_type_sonic(*it, true, message, field, reflection, err)) {
                    return false;
                }
            }
        } else if (!convert_float_type_sonic(value, false, message, field, reflection, err)) {
            return false;
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!convert_double_type_sonic(*it, true, message, field, reflection, err)) {
                    return false;
                }
            }
        } else if (!convert_double_type_sonic(value, false, message, field, reflection, err)) {
            return false;
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!it->IsString()) {
                    if (!value_invalid_sonic(field, "string", *it, err)) {
                        return false;
                    }
                    continue;
                }
                sonic_json::StringView sv = it->GetStringView();
                std::string str(sv.data(), sv.size());
                if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES &&
                    options.base64_to_bytes) {
                    std::string decoded;
                    if (!butil::Base64Decode(str, &decoded)) {
                        J2PERROR_WITH_PB(message, err, "Fail to decode base64 string=%s",
                                         str.c_str());
                        return false;
                    }
                    str = decoded;
                }
                reflection->AddString(message, field, str);
            }
        } else {
            if (!value.IsString()) {
                return value_invalid_sonic(field, "string", value, err);
            }
            sonic_json::StringView sv = value.GetStringView();
            std::string str(sv.data(), sv.size());
            if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES &&
                options.base64_to_bytes) {
                std::string decoded;
                if (!butil::Base64Decode(str, &decoded)) {
                    J2PERROR_WITH_PB(message, err, "Fail to decode base64 string=%s",
                                     str.c_str());
                    return false;
                }
                str = decoded;
            }
            reflection->SetString(message, field, str);
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!convert_enum_type_sonic(*it, true, message, field, reflection, err)) {
                    return false;
                }
            }
        } else if (!convert_enum_type_sonic(value, false, message, field, reflection, err)) {
            return false;
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        if (field->is_repeated()) {
            for (auto it = value.Begin(); it != value.End(); ++it) {
                if (!it->IsObject()) {
                    if (!value_invalid_sonic(field, "message", *it, err)) {
                        return false;
                    }
                    continue;
                }
                if (!JsonValueToProtoMessageSonic(
                        *it, reflection->AddMessage(message, field), options, err, depth + 1)) {
                    return false;
                }
            }
        } else if (!JsonValueToProtoMessageSonic(
                       value, reflection->MutableMessage(message, field), options, err,
                       depth + 1)) {
            return false;
        }
        break;
    }
    return true;
}

template <typename JsonValue>
bool JsonMapToProtoMapSonic(const JsonValue& value,
                            const google::protobuf::FieldDescriptor* map_desc,
                            google::protobuf::Message* message,
                            const Json2PbOptions& options,
                            std::string* err,
                            int depth) {
    if (!value.IsObject()) {
        J2PERROR_WITH_PB(nullptr, err, "Non-object value for map field: %s",
                         map_desc->full_name().c_str());
        return false;
    }

    const google::protobuf::Reflection* reflection = message->GetReflection();
    const google::protobuf::FieldDescriptor* key_desc =
        map_desc->message_type()->FindFieldByName(KEY_NAME);
    const google::protobuf::FieldDescriptor* value_desc =
        map_desc->message_type()->FindFieldByName(VALUE_NAME);

    for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
        google::protobuf::Message* entry = reflection->AddMessage(message, map_desc);
        const google::protobuf::Reflection* entry_reflection = entry->GetReflection();
        sonic_json::StringView key_sv = it->name.GetStringView();
        entry_reflection->SetString(entry, key_desc, std::string(key_sv.data(), key_sv.size()));
        if (!JsonValueToProtoFieldSonic(it->value, value_desc, entry, options, err,
                                        depth + 1)) {
            return false;
        }
    }
    return true;
}

template <typename JsonValue>
bool JsonValueToProtoMessageSonic(const JsonValue& json_value,
                                  google::protobuf::Message* message,
                                  const Json2PbOptions& options,
                                  std::string* err,
                                  int depth) {
    if (depth > Json2PbMaxRecursionDepth()) {
        J2PERROR_WITH_PB(message, err, "Exceeded maximum recursion depth");
        return false;
    }
    const google::protobuf::Descriptor* descriptor = message->GetDescriptor();
    if (!json_value.IsObject() &&
        !(json_value.IsArray() && options.array_to_single_repeated && depth == 0)) {
        J2PERROR_WITH_PB(message, err, "The input is not a json object");
        return false;
    }

    const google::protobuf::Reflection* reflection = message->GetReflection();
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    fields.reserve(64);
    for (int i = 0; i < descriptor->extension_range_count(); ++i) {
        const google::protobuf::Descriptor::ExtensionRange* ext_range =
            descriptor->extension_range(i);
#if GOOGLE_PROTOBUF_VERSION < 4025000
        for (int tag_number = ext_range->start; tag_number < ext_range->end; ++tag_number)
#else
        for (int tag_number = ext_range->start_number();
             tag_number < ext_range->end_number(); ++tag_number)
#endif
        {
            const google::protobuf::FieldDescriptor* field =
                reflection->FindKnownExtensionByNumber(tag_number);
            if (field) {
                fields.push_back(field);
            }
        }
    }
    for (int i = 0; i < descriptor->field_count(); ++i) {
        fields.push_back(descriptor->field(i));
    }

    if (json_value.IsArray()) {
        if (fields.size() == 1 && fields.front()->is_repeated()) {
            return JsonValueToProtoFieldSonic(json_value, fields.front(), message, options, err,
                                              depth);
        }
        J2PERROR_WITH_PB(message, err, "the input json can't be array here");
        return false;
    }

    std::string field_name_str_temp;
    for (size_t i = 0; i < fields.size(); ++i) {
        const google::protobuf::FieldDescriptor* field = fields[i];
        const std::string& orig_name = field->name();
        bool res = decode_name(orig_name, field_name_str_temp);
        const std::string& field_name_str = (res ? field_name_str_temp : orig_name);
        auto member = json_value.FindMember(
            sonic_json::StringView(field_name_str.data(), field_name_str.size()));
        if (member == json_value.MemberEnd()) {
            if (field->is_required()) {
                J2PERROR_WITH_PB(nullptr, err, "Missing required field: %s",
                                 field->full_name().c_str());
                return false;
            }
            continue;
        }
        const auto* value_ptr = &(member->value);
        if (IsProtobufMap(field) && value_ptr->IsObject()) {
            if (!JsonMapToProtoMapSonic(*value_ptr, field, message, options, err, depth)) {
                return false;
            }
        } else if (!JsonValueToProtoFieldSonic(*value_ptr, field, message, options, err,
                                               depth)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool JsonToProtoMessageWithSonic(const std::string& json_string,
                                 google::protobuf::Message* message,
                                 const Json2PbOptions& options,
                                 std::string* error) {
    if (error) {
        error->clear();
    }
    sonic_json::Document d;
    d.Parse(json_string.data(), json_string.size());
    if (d.HasParseError()) {
        J2PERROR_WITH_PB(message, error, "Invalid json: %s",
                         sonic_json::ErrorMsg(d.GetParseError()));
        return false;
    }
    return JsonValueToProtoMessageSonic(d, message, options, error, 0);
}

}  // namespace json2pb

#endif  // BRPC_WITH_SONIC_JSON

