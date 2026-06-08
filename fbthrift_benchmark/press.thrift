namespace cpp2 press_test

cpp_include "folly/io/IOBuf.h"

typedef binary (cpp.type = "std::unique_ptr<folly::IOBuf>") IOBufPtr

struct Request {
    1: bool echo_attachment;
    2: IOBufPtr payload;
}

struct PerfTestResponse {
    1: double cpu_usage;
    2: double mem_usage;
    3: IOBufPtr payload;
}

service PerfTestService {
    PerfTestResponse native_test(1: Request req);
}