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

#ifndef BRPC_JSON2PB_PB_TO_JSON_SONIC_H
#define BRPC_JSON2PB_PB_TO_JSON_SONIC_H

#include <string>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>
#include "json2pb/pb_to_json.h"

namespace json2pb {

bool ProtoMessageToJsonWithSonic(const google::protobuf::Message& message,
                                 std::string* json,
                                 const Pb2JsonOptions& options,
                                 std::string* error);

bool ProtoMessageToJsonWithSonic(
    const google::protobuf::Message& message,
    google::protobuf::io::ZeroCopyOutputStream* stream,
    const Pb2JsonOptions& options,
    std::string* error);

}  // namespace json2pb

#endif  // BRPC_JSON2PB_PB_TO_JSON_SONIC_H
