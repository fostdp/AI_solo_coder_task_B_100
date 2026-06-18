#pragma once

#include "common/data_types.h"
#include "fenyun_application.h"

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>

namespace fenyun {

class FenyunHttpServer {
public:
    explicit FenyunHttpServer(std::shared_ptr<FenyunApplication> app);
    ~FenyunHttpServer();

    bool start(uint16_t port = 8080, const std::string& host = "0.0.0.0");
    void stop();
    bool is_running() const { return running_.load(); }

    uint16_t port() const { return port_; }

private:
    void accept_loop();
    void handle_client(int client_fd);

    std::string process_request(const std::string& method,
                                const std::string& path,
                                const std::string& query,
                                const std::string& body);

    std::string handle_get(const std::string& path, const std::string& query);
    std::string handle_post(const std::string& path, const std::string& body);

    static std::string make_json_response(int status_code, const std::string& json_body);
    static std::string make_response(int status_code,
                                     const std::string& content_type,
                                     const std::string& body);
    static std::string url_decode(const std::string& s);
    static void parse_query_string(const std::string& query,
                                   std::map<std::string, std::string>& params);

    std::shared_ptr<FenyunApplication> app_;

    int server_fd_ = -1;
    uint16_t port_ = 8080;
    std::string host_;
    std::atomic<bool> running_ {false};
    std::thread accept_thread_;
};

}
