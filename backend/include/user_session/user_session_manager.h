#pragma once

#include "common/data_types.h"
#include "impact_simulator/impact_simulator.h"
#include "config/config_loader.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <random>
#include <chrono>

namespace fenyun {

class UserSessionManager {
public:
    explicit UserSessionManager(std::shared_ptr<ConfigLoader> config,
                                 std::shared_ptr<ImpactSimulator> simulator);
    ~UserSessionManager();

    UserSession create_session(const std::string& nickname = "",
                               const std::string& vehicle_type = "fenyun_basic");

    bool has_session(const std::string& session_id) const;

    UserSession get_session(const std::string& session_id) const;

    void destroy_session(const std::string& session_id);

    UserVehicleState get_vehicle_state(const std::string& session_id) const;

    UserVehicleState apply_action(const UserActionRequest& request);

    std::vector<RockAttackEvent> process_time_step(const std::string& session_id,
                                                  double dt_seconds = 1.0);

    std::vector<RockAttackEvent> trigger_rock_attack(const std::string& session_id,
                                                      double force_multiplier = 1.0);

    void reset_session(const std::string& session_id);

    std::vector<UserSession> active_sessions() const;

    uint64_t total_sessions_created() const { return sessions_created_.load(); }

    void cleanup_expired_sessions(int64_t idle_timeout_ms = 300000);

private:
    std::string generate_session_id();

    RockAttackEvent generate_random_attack(const std::string& session_id,
                                          const UserVehicleState& state,
                                          double force_multiplier);

    double compute_damage(const SimulationResult& sim, const VehicleProfile& vp) const;

    void update_state_from_attack(UserVehicleState& state,
                                  const RockAttackEvent& attack,
                                  const VehicleProfile& vp);

    void move_vehicle(UserVehicleState& state, double throttle, double steering, double dt);

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<ImpactSimulator> simulator_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserSession> sessions_;
    std::unordered_map<std::string, UserVehicleState> vehicle_states_;
    std::unordered_map<std::string, std::vector<RockAttackEvent>> recent_attacks_;

    std::atomic<uint64_t> sessions_created_ {0};
    std::atomic<uint64_t> attack_id_counter_ {0};
    std::atomic<uint64_t> session_id_counter_ {0};
    mutable std::mt19937 rng_ {std::random_device{}()};
};

}
