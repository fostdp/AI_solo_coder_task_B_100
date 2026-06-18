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
#include <functional>

namespace fenyun {

using SessionCallback = std::function<void(const std::string& session_id, const UserVehicleState& state)>;
using AttackCallback = std::function<void(const RockAttackEvent& event)>;

enum class VRGameMode : uint8_t {
    FREE_DRIVE = 0,
    ASSAULT_CHALLENGE = 1,
    SURVIVAL = 2,
    TIMED_RACE = 3
};

struct VRRoundResult {
    std::string session_id;
    int64_t start_ms;
    int64_t end_ms;
    double final_health_percent;
    double total_distance_m;
    int total_attacks_sustained;
    double total_damage_taken;
    int waves_survived;
    double score;
    std::string rank;
};

class VRAssaultEngine {
public:
    explicit VRAssaultEngine(std::shared_ptr<ConfigLoader> config,
                             std::shared_ptr<ImpactSimulator> simulator);
    ~VRAssaultEngine();

    std::string start_session(const std::string& nickname = "",
                              const std::string& vehicle_type = "fenyun_basic",
                              VRGameMode mode = VRGameMode::FREE_DRIVE);

    void end_session(const std::string& session_id);

    bool has_session(const std::string& session_id) const;
    UserSession get_session_info(const std::string& session_id) const;
    UserVehicleState get_state(const std::string& session_id) const;

    UserVehicleState apply_action(const std::string& session_id,
                                   const std::string& action,
                                   double param1 = 0.0,
                                   double param2 = 0.0);

    std::vector<RockAttackEvent> tick(const std::string& session_id,
                                      double dt_seconds = 1.0);

    std::vector<RockAttackEvent> trigger_attack(const std::string& session_id,
                                                 double force_multiplier = 1.0);

    void reset_session(const std::string& session_id);
    std::vector<UserSession> active_sessions() const;

    VRRoundResult get_round_result(const std::string& session_id) const;

    void set_state_callback(SessionCallback cb) { state_callback_ = cb; }
    void set_attack_callback(AttackCallback cb) { attack_callback_ = cb; }

    uint64_t total_sessions() const { return sessions_created_.load(); }

    void cleanup_expired(int64_t idle_timeout_ms = 300000);

private:
    ShockVibration compute_shock(double damage_level,
                                  double rock_mass_kg,
                                  double rock_velocity_ms,
                                  double impact_x,
                                  double vehicle_width_m,
                                  int64_t now_ms);

    RockAttackEvent generate_attack(const std::string& session_id,
                                     const UserVehicleState& state,
                                     double force_multiplier);

    double compute_damage(const SimulationResult& sim,
                          const VehicleProfile& vp) const;

    void apply_attack_to_state(UserVehicleState& state,
                               const RockAttackEvent& attack,
                               const VehicleProfile& vp);

    void move_vehicle(UserVehicleState& state,
                      double throttle,
                      double steering,
                      double dt);

    void decay_vibration(UserVehicleState& state, double dt);

    double calculate_score(const UserVehicleState& state,
                            double distance,
                            int attacks) const;

    std::string calculate_rank(double score) const;

    std::shared_ptr<ConfigLoader> config_;
    std::shared_ptr<ImpactSimulator> simulator_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserSession> sessions_;
    std::unordered_map<std::string, UserVehicleState> states_;
    std::unordered_map<std::string, VRRoundResult> round_results_;
    std::unordered_map<std::string, VRGameMode> game_modes_;

    std::atomic<uint64_t> sessions_created_ {0};
    std::atomic<uint64_t> attack_id_counter_ {0};
    std::atomic<uint64_t> session_id_counter_ {0};
    mutable std::mt19937 rng_ {std::random_device{}()};

    SessionCallback state_callback_;
    AttackCallback attack_callback_;
};

}
