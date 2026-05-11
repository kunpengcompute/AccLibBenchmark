namespace cpp2 press_test

struct Request {
    1: bool echo_attachment;
    2: binary payload;
}

struct PerfTestResponse {
    1: string cpu_usage;
    2: binary payload;

}

service PerfTestService {
    PerfTestResponse test(1: Request req);
}