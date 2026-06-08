namespace cpp2 universal

cpp_include "folly/io/IOBuf.h"

typedef binary (cpp.type = "std::unique_ptr<folly::IOBuf>") IOBufPtr

enum SerializationType {
    PROTOBUF = 1,
    SIMD_JSON = 2,
    THRIFT_BINARY = 3,
    THRIFT_COMPACT = 4,
    THRIFT_JSON = 5,
}

struct UniversalRequest {
    1: SerializationType type;
    2: IOBufPtr payload;
}

struct UniversalResponse {
    1: SerializationType type;
    2: IOBufPtr payload;
}

service UniversalService {
    UniversalResponse universal_test(1: UniversalRequest request);
}