#include "user_session/user_session_manager.h"

#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fenyun {

UserSessionManager::UserSessionManager(std::shared_ptr<ConfigLoader> config,
                                       std::shared_ptr<ImpactSimulator> simulator)
    : config_(std::move(config)), simulator_(std::move(simulator)) {
}

UserSessionManager::~UserSessionManager() = default;

std::string UserSessionManager::generate_session_id() {
    uint64_t counter = session_id_counter_.fetch_add(1);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t random_part = dist(rng_);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(counter >> 32)
        << std::setw(8) << static_cast<uint32_t>(counter & 0xFFFFFFFF)
        << "-" << std::setw(8) << random_part
        << "-" << std::setw(4) << (dist(rng_) & 0xFFFF)
        << "-" << std::setw(4) << ((dist(rng_) & 0x0FFF) | 0x4000)
        << "-" << std::setw(12) << (static_cast<uint64_t>(dist(rng_)) << 32 | dist(rng_));
    return oss.str();
}

bool UserSessionManager::has_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(session_id) != sessions_.end();
}

UserSession UserSessionManager::get_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return UserSession{};
}

void UserSessionManager::destroy_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
    vehicle_states_.erase(session_id);
    recent_attacks_.erase(session_id);
}

UserVehicleState UserSessionManager::get_vehicle_state(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = vehicle_states_.find(session_id);
    if (it != vehicle_states_.end()) {
        return it->second;
    }
    return UserVehicleState{};
}

UserSession UserSessionManager::create_session(const std::string& nickname,
                                               const std::string& vehicle_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    UserSession session;
    session.session_id = generate_session_id();
    session.created_ms = current_timestamp_ms();
    session.last_active_ms = session.created_ms;
    session.user_nickname = nickname;
    session.vehicle_type = vehicle_type;
    session.current_vehicle_id = static_cast<uint32_t>(sessions_created_.load());

    UserVehicleState state;
    state.session_id = session.session_id;
    state.position_x = 0.0;
    state.position_y = 30.0;
    state.heading_deg = 180.0;
    state.speed_ms = 0.0;
    state.health_percent = 100.0;
    state.armor_integrity_percent = 100.0;
    state.impacts_received = 0;
    state.distance_traveled_m = 0.0;
    state.timestamp_ms = session.created_ms;

    sessions_[session.session_id] = session;
    vehicle_states_[session.session_id] = state;
    recent_attacks_[session.session_id] = std::vector<RockAttackEvent>{};

    sessions_created_.fetch_add(1);
    return session;
}

void UserSessionManager::move_vehicle(UserVehicleState& state, double throttle, double steering, double dt) {
    VehicleProfile vp;
    if (config_) {
        auto it = sessions_.find(state.session_id);
        if (it != sessions_.end()) {
            vp = config_->get_vehicle(it->second.vehicle_type);
        }
    }

    double max_speed_ms = vp.max_speed_kmh > 0 ? vp.max_speed_kmh / 3.6 : 5.0;

    state.speed_ms = std::max(-max_speed_ms * 0.5, std::min(max_speed_ms, state.speed_ms + throttle * max_speed_ms * dt));

    double steer_rate = 30.0;
    state.heading_deg += steering * steer_rate * dt;

    while (state.heading_deg >= 360.0) state.heading_deg -= 360.0;
    while (state.heading_deg < 0.0) state.heading_deg += 360.0;

    double heading_rad = state.heading_deg * PI / 180.0;
    double dx = state.speed_ms * std::cos(heading_rad) * dt;
    double dy = state.speed_ms * std::sin(heading_rad) * dt;
    state.position_x += dx;
    state.position_y += dy;
    state.distance_traveled_m += std::sqrt(dx * dx + dy * dy);
    state.timestamp_ms = current_timestamp_ms();
}

UserVehicleState UserSessionManager::apply_action(const UserActionRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(request.session_id);
    if (session_it == sessions_.end()) {
        return UserVehicleState{};
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = vehicle_states_.find(request.session_id);
    if (state_it == vehicle_states_.end()) {
        return UserVehicleState{};
    }

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};
    double max_speed_ms = vp.max_speed_kmh > 0 ? vp.max_speed_kmh / 3.6 : 5.0;

    if (request.action == "throttle") {
        double throttle = std::max(-1.0, std::min(1.0, request.param1));
        state.speed_ms = std::max(-max_speed_ms * 0.5, std::min(max_speed_ms, state.speed_ms + throttle * max_speed_ms * 0.1));
    } else if (request.action == "steer") {
        double steer = std::max(-1.0, std::min(1.0, request.param1));
        state.heading_deg += steer * 10.0;
        while (state.heading_deg >= 360.0) state.heading_deg -= 360.0;
        while (state.heading_deg < 0.0) state.heading_deg += 360.0;
    } else if (request.action == "brake") {
        state.speed_ms = 0.0;
    } else if (request.action == "reset_position") {
        state.position_x = 0.0;
        state.position_y = 30.0;
        state.heading_deg = 180.0;
        state.speed_ms = 0.0;
    }

    state.timestamp_ms = current_timestamp_ms();
    return state;
}

double UserSessionManager::compute_damage(const SimulationResult& sim, const VehicleProfile& vp) const {
    std::uniform_real_distribution<double> dist;
    double base_damage = 0.0;

    switch (sim.damage_level) {
        case 0:
            dist = std::uniform_real_distribution<double>(0.0, 2.0);
            break;
        case 1:
            dist = std::uniform_real_distribution<double>(2.0, 5.0);
            break;
        case 2:
            dist = std::uniform_real_distribution<double>(5.0, 12.0);
            break;
        case 3:
            dist = std::uniform_real_distribution<double>(12.0, 25.0);
            break;
        case 4:
        default:
            dist = std::uniform_real_distribution<double>(25.0, 50.0);
            break;
    }

    base_damage = dist(rng_);

    double armor_factor = 1.0;
    if (vp.roof_thickness_mm > 0) {
        armor_factor = std::max(0.3, 100.0 / (100.0 + vp.roof_thickness_mm * 0.5));
    }

    return base_damage * armor_factor;
}

RockAttackEvent UserSessionManager::generate_random_attack(const std::string& session_id,
                                                           const UserVehicleState& state,
                                                           double force_multiplier) {
    RockAttackEvent event;
    event.event_id = attack_id_counter_.fetch_add(1);
    event.session_id = session_id;
    event.timestamp_ms = current_timestamp_ms();

    std::uniform_real_distribution<double> mass_dist(10.0, 100.0);
    std::uniform_real_distribution<double> vel_dist(8.0, 20.0);
    event.rock_mass_kg = mass_dist(rng_) * force_multiplier;
    event.rock_velocity_ms = vel_dist(rng_) * std::sqrt(force_multiplier);

    VehicleProfile vp;
    if (config_) {
        auto sess_it = sessions_.find(session_id);
        if (sess_it != sessions_.end()) {
            vp = config_->get_vehicle(sess_it->second.vehicle_type);
        }
    }

    double roof_length = vp.length_m > 0 ? vp.length_m : 6.5;
    double roof_width = vp.width_m > 0 ? vp.width_m : 2.8;
    std::uniform_real_distribution<double> x_dist(-roof_length / 2.0, roof_length / 2.0);
    std::uniform_real_distribution<double> y_dist(-roof_width / 2.0, roof_width / 2.0);

    double local_x = x_dist(rng_);
    double local_y = y_dist(rng_);

    double heading_rad = state.heading_deg * PI / 180.0;
    double cos_h = std::cos(heading_rad);
    double sin_h = std::sin(heading_rad);
    event.impact_x = state.position_x + local_x * cos_h - local_y * sin_h;
    event.impact_y = state.position_y + local_x * sin_h + local_y * cos_h;

    SensorData sensor_data{};
    sensor_data.vehicle_id = 0;
    sensor_data.timestamp_ms = event.timestamp_ms;
    sensor_data.impact_location_x = local_x;
    sensor_data.impact_location_y = local_y;
    sensor_data.rock_mass = event.rock_mass_kg;
    sensor_data.rock_velocity = event.rock_velocity_ms;
    sensor_data.protection_thickness = vp.roof_thickness_mm > 0 ? vp.roof_thickness_mm : DEFAULT_ROOF_THICKNESS;
    sensor_data.protection_material = vp.primary_material.empty() ? "wood" : vp.primary_material;
    sensor_data.ambient_temp = 293.15;
    sensor_data.rock_impact_force = event.rock_mass_kg * GRAVITY * event.rock_velocity_ms;

    SimulationResult sim_result;
    if (simulator_) {
        sim_result = simulator_->run_simulation(sensor_data);
    }

    event.damage_dealt = compute_damage(sim_result, vp);

    return event;
}

void UserSessionManager::update_state_from_attack(UserVehicleState& state,
                                                   const RockAttackEvent& attack,
                                                   const VehicleProfile& vp) {
    state.impacts_received += 1;

    double health_damage = attack.damage_dealt;
    double armor_damage = attack.damage_dealt * 0.8;

    if (state.armor_integrity_percent > 0) {
        double armor_absorbed = std::min(state.armor_integrity_percent, armor_damage);
        state.armor_integrity_percent -= armor_absorbed;
        health_damage = std::max(0.0, health_damage - armor_absorbed * 0.5);
    }

    state.health_percent = std::max(0.0, state.health_percent - health_damage);
    state.timestamp_ms = current_timestamp_ms();
}

std::vector<RockAttackEvent> UserSessionManager::process_time_step(const std::string& session_id,
                                                                    double dt_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<RockAttackEvent> events;

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return events;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = vehicle_states_.find(session_id);
    if (state_it == vehicle_states_.end()) {
        return events;
    }

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};

    move_vehicle(state, 0.0, 0.0, dt_seconds);

    std::uniform_int_distribution<int> attack_count_dist(1, 3);
    int attack_count = attack_count_dist(rng_);

    for (int i = 0; i < attack_count; ++i) {
        RockAttackEvent attack = generate_random_attack(session_id, state, 1.0);
        update_state_from_attack(state, attack, vp);
        events.push_back(attack);
    }

    recent_attacks_[session_id] = events;

    return events;
}

std::vector<RockAttackEvent> UserSessionManager::trigger_rock_attack(const std::string& session_id,
                                                                      double force_multiplier) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<RockAttackEvent> events;

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return events;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = vehicle_states_.find(session_id);
    if (state_it == vehicle_states_.end()) {
        return events;
    }

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};

    RockAttackEvent attack = generate_random_attack(session_id, state, force_multiplier);
    update_state_from_attack(state, attack, vp);
    events.push_back(attack);

    recent_attacks_[session_id] = events;

    return events;
}

void UserSessionManager::reset_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = vehicle_states_.find(session_id);
    if (state_it != vehicle_states_.end()) {
        UserVehicleState& state = state_it->second;
        state.position_x = 0.0;
        state.position_y = 30.0;
        state.heading_deg = 180.0;
        state.speed_ms = 0.0;
        state.health_percent = 100.0;
        state.armor_integrity_percent = 100.0;
        state.impacts_received = 0;
        state.distance_traveled_m = 0.0;
        state.timestamp_ms = current_timestamp_ms();
    }

    auto attacks_it = recent_attacks_.find(session_id);
    if (attacks_it != recent_attacks_.end()) {
        attacks_it->second.clear();
    }
}

std::vector<UserSession> UserSessionManager::active_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserSession> result;
    result.reserve(sessions_.size());
    for (const auto& kv : sessions_) {
        result.push_back(kv.second);
    }
    return result;
}

void UserSessionManager::cleanup_expired_sessions(int64_t idle_timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t now = current_timestamp_ms();

    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (now - it->second.last_active_ms > idle_timeout_ms) {
            const std::string& sid = it->first;
            vehicle_states_.erase(sid);
            recent_attacks_.erase(sid);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

}
