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

#include "json2pb/pb_to_json_sonic.h"

#ifdef BRPC_WITH_SONIC_JSON

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <google/protobuf/descriptor.h>
#include <gflags/gflags.h>
#include "butil/base64.h"
#include "json2pb/encode_decode.h"
#include "json2pb/protobuf_map.h"
#include "sonic/dom/generic_document.h"
#include "sonic/error.h"
#include "sonic/writebuffer.h"

namespace json2pb {

DECLARE_int32(json2pb_max_recursion_depth);

namespace {

using SonicDocument = sonic_json::Document;
using SonicNode = sonic_json::Node;

inline bool WriteStringToZeroCopyOutputStream(
    const std::string& data, google::protobuf::io::ZeroCopyOutputStream* stream) {
    const char* src = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        void* buf = nullptr;
        int buf_size = 0;
        if (!stream->Next(&buf, &buf_size)) {
            return false;
        }
        const size_t to_copy =
            std::min(remaining, static_cast<size_t>(buf_size));
        memcpy(buf, src, to_copy);
        if (to_copy < static_cast<size_t>(buf_size)) {
            stream->BackUp(static_cast<int>(buf_size - to_copy));
        }
        src += to_copy;
        remaining -= to_copy;
    }
    return true;
}

inline void NormalizeUnicodeEscapesUppercase(std::string* json) {
    for (size_t i = 0; i + 5 < json->size(); ++i) {
        if ((*json)[i] == '\\' && (*json)[i + 1] == 'u') {
            for (size_t j = i + 2; j < i + 6; ++j) {
                char& c = (*json)[j];
                if (c >= 'a' && c <= 'f') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
            i += 5;
        }
    }
}

bool ExceedMaxDepthSonic(const google::protobuf::Message& message,
                         int current_depth) {
    if (current_depth > FLAGS_json2pb_max_recursion_depth) {
        return true;
    }

    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    for (int i = 0; i < descriptor->field_count(); ++i) {
        fields.push_back(descriptor->field(i));
    }
    std::vector<const google::protobuf::FieldDescriptor*> ext_fields;
    descriptor->file()->pool()->FindAllExtensions(descriptor, &ext_fields);
    fields.insert(fields.end(), ext_fields.begin(), ext_fields.end());

    for (const auto* field : fields) {
        if (field->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            continue;
        }
        if (field->is_repeated()) {
            const int count = reflection->FieldSize(message, field);
            for (int i = 0; i < count; ++i) {
                if (ExceedMaxDepthSonic(
                        reflection->GetRepeatedMessage(message, field, i),
                        current_depth + 1)) {
                    return true;
                }
            }
        } else if (reflection->HasField(message, field)) {
            if (ExceedMaxDepthSonic(reflection->GetMessage(message, field),
                                    current_depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

inline void CollectFields(
    const google::protobuf::Message& message,
    const Pb2JsonOptions& options,
    std::vector<const google::protobuf::FieldDescriptor*>* fields,
    std::vector<const google::protobuf::FieldDescriptor*>* map_fields) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();
    (void)reflection;

    const int ext_range_count = descriptor->extension_range_count();
    const int field_count = descriptor->field_count();
    fields->clear();
    map_fields->clear();
    fields->reserve(64);

    for (int i = 0; i < ext_range_count; ++i) {
        const google::protobuf::Descriptor::ExtensionRange* ext_range =
            descriptor->extension_range(i);
#if GOOGLE_PROTOBUF_VERSION < 4025000
        for (int tag_number = ext_range->start; tag_number < ext_range->end;
             ++tag_number)
#else
        for (int tag_number = ext_range->start_number();
             tag_number < ext_range->end_number(); ++tag_number)
#endif
        {
            const google::protobuf::FieldDescriptor* field =
                reflection->FindKnownExtensionByNumber(tag_number);
            if (field) {
                fields->push_back(field);
            }
        }
    }

    for (int i = 0; i < field_count; ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);
        if (options.enable_protobuf_map && json2pb::IsProtobufMap(field)) {
            map_fields->push_back(field);
        } else {
            fields->push_back(field);
        }
    }
}

inline std::string JsonFieldName(const google::protobuf::FieldDescriptor* field) {
    std::string field_name_str;
    const std::string& orig_name = field->name();
    const bool decoded = decode_name(orig_name, field_name_str);
    return decoded ? field_name_str : orig_name;
}

inline bool ShouldEmitField(const google::protobuf::Message& message,
                            const google::protobuf::FieldDescriptor* field,
                            const Pb2JsonOptions& options) {
    const google::protobuf::Reflection* reflection = message.GetReflection();
    if (!field->is_repeated() && !reflection->HasField(message, field)) {
        if (field->is_required()) {
            return true;
        }
        return options.always_print_primitive_fields;
    }
    if (field->is_repeated() && reflection->FieldSize(message, field) == 0) {
        return options.jsonify_empty_array;
    }
    return true;
}

bool ProtoFieldToSonicNode(const google::protobuf::Message& message,
                           const google::protobuf::FieldDescriptor* field,
                           SonicNode* node,
                           SonicDocument* document,
                           const Pb2JsonOptions& options,
                           std::string* error,
                           int depth);

bool ProtoMessageToSonicNode(const google::protobuf::Message& message,
                             SonicNode* node,
                             SonicDocument* document,
                             const Pb2JsonOptions& options,
                             std::string* error,
                             int depth) {
    if (depth > FLAGS_json2pb_max_recursion_depth) {
        if (error) {
            *error = "Exceeded maximum recursion depth";
        }
        return false;
    }

    const google::protobuf::Reflection* reflection = message.GetReflection();
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    std::vector<const google::protobuf::FieldDescriptor*> map_fields;
    CollectFields(message, options, &fields, &map_fields);

    if (depth == 0 && options.single_repeated_to_array) {
        if (map_fields.empty() && fields.size() == 1 && fields.front()->is_repeated()) {
            return ProtoFieldToSonicNode(message, fields.front(), node, document,
                                         options, error, depth);
        }
    }

    node->SetObject();
    node->MemberReserve(fields.size() + map_fields.size(), document->GetAllocator());

    for (const auto* field : fields) {
        if (!ShouldEmitField(message, field, options)) {
            continue;
        }
        if (!field->is_repeated() && !reflection->HasField(message, field) &&
            field->is_required()) {
            if (error) {
                *error = "Missing required field: " + field->full_name();
            }
            return false;
        }
        SonicNode child;
        if (!ProtoFieldToSonicNode(message, field, &child, document, options, error,
                                   depth)) {
            return false;
        }
        const std::string name = JsonFieldName(field);
        node->AddMember(sonic_json::StringView(name.data(), name.size()),
                        std::move(child), document->GetAllocator());
    }

    for (const auto* map_desc : map_fields) {
        SonicNode map_node;
        map_node.SetObject();
        const google::protobuf::FieldDescriptor* key_desc =
            map_desc->message_type()->field(json2pb::KEY_INDEX);
        const google::protobuf::FieldDescriptor* value_desc =
            map_desc->message_type()->field(json2pb::VALUE_INDEX);

        std::string entry_name;
        for (int j = 0; j < reflection->FieldSize(message, map_desc); ++j) {
            const google::protobuf::Message& entry =
                reflection->GetRepeatedMessage(message, map_desc, j);
            const google::protobuf::Reflection* entry_reflection =
                entry.GetReflection();
            entry_name = entry_reflection->GetStringReference(entry, key_desc,
                                                              &entry_name);

            SonicNode child;
            if (!ProtoFieldToSonicNode(entry, value_desc, &child, document, options,
                                       error, depth)) {
                return false;
            }
            map_node.AddMember(
                sonic_json::StringView(entry_name.data(), entry_name.size()),
                std::move(child), document->GetAllocator());
        }

        const std::string name = JsonFieldName(map_desc);
        node->AddMember(sonic_json::StringView(name.data(), name.size()),
                        std::move(map_node), document->GetAllocator());
    }
    return true;
}

template <typename Getter>
void FillRepeatedScalarNode(const google::protobuf::Message& message,
                            const google::protobuf::FieldDescriptor* field,
                            SonicNode* node,
                            SonicDocument* document,
                            Getter getter) {
    const google::protobuf::Reflection* reflection = message.GetReflection();
    const int field_size = reflection->FieldSize(message, field);
    node->SetArray();
    node->Reserve(field_size, document->GetAllocator());
    for (int index = 0; index < field_size; ++index) {
        SonicNode item = getter(index);
        node->PushBack(std::move(item), document->GetAllocator());
    }
}

bool ProtoFieldToSonicNode(const google::protobuf::Message& message,
                           const google::protobuf::FieldDescriptor* field,
                           SonicNode* node,
                           SonicDocument* document,
                           const Pb2JsonOptions& options,
                           std::string* error,
                           int depth) {
    const google::protobuf::Reflection* reflection = message.GetReflection();
    switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetBool(reflection->GetRepeatedBool(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetBool(reflection->GetBool(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetInt64(reflection->GetRepeatedInt32(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetInt64(reflection->GetInt32(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetUint64(reflection->GetRepeatedUInt32(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetUint64(reflection->GetUInt32(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetInt64(reflection->GetRepeatedInt64(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetInt64(reflection->GetInt64(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetUint64(reflection->GetRepeatedUInt64(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetUint64(reflection->GetUInt64(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetDouble(reflection->GetRepeatedFloat(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetDouble(reflection->GetFloat(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        if (field->is_repeated()) {
            FillRepeatedScalarNode(message, field, node, document,
                                   [&](int index) {
                                       SonicNode item;
                                       item.SetDouble(reflection->GetRepeatedDouble(
                                           message, field, index));
                                       return item;
                                   });
        } else {
            node->SetDouble(reflection->GetDouble(message, field));
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        auto encode_string = [&](const std::string& input, SonicNode* out) {
            std::string value = input;
            if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES &&
                options.bytes_to_base64) {
                std::string encoded;
                butil::Base64Encode(value, &encoded);
                value.swap(encoded);
            }
            out->SetString(value.data(), value.size(), document->GetAllocator());
        };

        if (field->is_repeated()) {
            const int field_size = reflection->FieldSize(message, field);
            node->SetArray();
            node->Reserve(field_size, document->GetAllocator());
            std::string value;
            for (int index = 0; index < field_size; ++index) {
                SonicNode item;
                value = reflection->GetRepeatedStringReference(message, field, index,
                                                               &value);
                encode_string(value, &item);
                node->PushBack(std::move(item), document->GetAllocator());
            }
        } else {
            std::string value;
            value = reflection->GetStringReference(message, field, &value);
            encode_string(value, node);
        }
        break;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        if (field->is_repeated()) {
            const int field_size = reflection->FieldSize(message, field);
            node->SetArray();
            node->Reserve(field_size, document->GetAllocator());
            for (int index = 0; index < field_size; ++index) {
                SonicNode item;
                const auto* enum_value =
                    reflection->GetRepeatedEnum(message, field, index);
                if (options.enum_option == OUTPUT_ENUM_BY_NAME) {
                    const std::string& name = enum_value->name();
                    item.SetString(name.data(), name.size(),
                                   document->GetAllocator());
                } else {
                    item.SetInt64(enum_value->number());
                }
                node->PushBack(std::move(item), document->GetAllocator());
            }
        } else {
            const auto* enum_value = reflection->GetEnum(message, field);
            if (options.enum_option == OUTPUT_ENUM_BY_NAME) {
                const std::string& name = enum_value->name();
                node->SetString(name.data(), name.size(), document->GetAllocator());
            } else {
                node->SetInt64(enum_value->number());
            }
        }
        break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        if (field->is_repeated()) {
            const int field_size = reflection->FieldSize(message, field);
            node->SetArray();
            node->Reserve(field_size, document->GetAllocator());
            for (int index = 0; index < field_size; ++index) {
                SonicNode item;
                if (!ProtoMessageToSonicNode(
                        reflection->GetRepeatedMessage(message, field, index), &item,
                        document, options, error, depth + 1)) {
                    return false;
                }
                node->PushBack(std::move(item), document->GetAllocator());
            }
        } else {
            if (!ProtoMessageToSonicNode(reflection->GetMessage(message, field),
                                         node, document, options, error, depth + 1)) {
                return false;
            }
        }
        break;
    }
    return true;
}

}  // namespace

bool ProtoMessageToJsonWithSonic(const google::protobuf::Message& message,
                                 std::string* json,
                                 const Pb2JsonOptions& options,
                                 std::string* error) {
    if (ExceedMaxDepthSonic(message, 0)) {
        if (error) {
            *error = "Exceeded maximum recursion depth";
        }
        return false;
    }

    SonicDocument document;
    if (!ProtoMessageToSonicNode(message, &document, &document, options, error, 0)) {
        return false;
    }

    sonic_json::WriteBuffer buffer;
    const sonic_json::SonicError err = document.Serialize(buffer);
    if (err != sonic_json::kErrorNone) {
        if (error) {
            *error = sonic_json::ErrorMsg(err);
        }
        return false;
    }
    json->append(buffer.ToString(), buffer.Size());
    NormalizeUnicodeEscapesUppercase(json);
    return true;
}

bool ProtoMessageToJsonWithSonic(
    const google::protobuf::Message& message,
    google::protobuf::io::ZeroCopyOutputStream* stream,
    const Pb2JsonOptions& options,
    std::string* error) {
    std::string json;
    if (!ProtoMessageToJsonWithSonic(message, &json, options, error)) {
        return false;
    }
    return WriteStringToZeroCopyOutputStream(json, stream);
}

}  // namespace json2pb

#endif  // BRPC_WITH_SONIC_JSON
