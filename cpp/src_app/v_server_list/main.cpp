#include <httplib.hpp>
#include <nlohmann_json.hpp>
#include <pp_common/service_runtime.hpp>

// HTTP
httplib::Server svr;

void Init() {
    svr.Get("/hi", [](const httplib::Request &, httplib::Response & res) { res.set_content("Hello World!", "text/plain"); });
}

void MainLoop() {
    svr.listen("0.0.0.0", 8080);
}

void Clean() {
    ///
}

int main(int argc, char ** argv) {
    Init();
    MainLoop();
    Clean();
    return 0;
}
