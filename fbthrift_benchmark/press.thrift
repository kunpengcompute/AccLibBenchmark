namespace cpp2 press_test

struct Request {
    1: bool echo_attachment;
    2: binary payload;
}

struct PerfTestResponse {
    1: double cpu_usage;
    2: double mem_usage;  
    3: binary payload;
}

service PerfTestService {
    PerfTestResponse test(1: Request req);
}