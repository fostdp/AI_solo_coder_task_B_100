#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace fenyun {

class MetricsCollector {
public:
    static MetricsCollector& instance();

    bool start(uint16_t port = 9091, const std::string& bind_addr = "0.0.0.0");
    void stop();

    void inc_sensor_received(uint32_t n = 1);
    void inc_sensor_valid(uint32_t n = 1);
    void inc_sensor_invalid(uint32_t n = 1);
    void inc_sensor_dropped(uint32_t n = 1);

    void observe_simulation_latency_ms(double ms);
    void inc_simulation_total(uint32_t n = 1);
    void inc_simulation_errors(uint32_t n = 1);

    void inc_evaluations_total(uint32_t n = 1);
    void observe_ahp_consistency_ratio(double cr);

    void inc_alerts_total(const std::string& level, const std::string& type, uint32_t n = 1);
    void inc_mqtt_published(uint32_t n = 1);
    void inc_mqtt_errors(uint32_t n = 1);

    void inc_clickhouse_write_ok(uint64_t rows = 1);
    void inc_clickhouse_write_fail(uint32_t n = 1);

    void observe_http_request_latency_ms(const std::string& method,
                                          const std::string& path,
                                          double ms);
    void inc_http_request_total(const std::string& method,
                                const std::string& path,
                                int status_code);

    void set_queue_approx_size(const std::string& queue_name, size_t size);

private:
    MetricsCollector();
    ~MetricsCollector();
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool started_ = false;
};

#define METRICS MetricsCollector::instance()

}
