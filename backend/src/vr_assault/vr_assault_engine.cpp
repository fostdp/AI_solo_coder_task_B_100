#include "vr_assault/vr_assault_engine.h"

#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fenyun {

VRAssaultEngine::VRAssaultEngine(std::shared_ptr<ConfigLoader> config,
                                 std::shared_ptr<ImpactSimulator> simulator)
    : config_(std::move(config)), simulator_(std::move(simulator)) {
}

VRAssaultEngine::~VRAssaultEngine() = default;

static std::string generate_session_id(std::atomic<uint64_t>& session_id_counter, std::mt19937& rng) {
    uint64_t counter = session_id_counter.fetch_add(1);
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t random_part = dist(rng);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(counter >> 32)
        << std::setw(8) << static_cast<uint32_t>(counter & 0xFFFFFFFF)
        << "-" << std::setw(8) << random_part
        << "-" << std::setw(4) << (dist(rng) & 0xFFFF)
        << "-" << std::setw(4) << ((dist(rng) & 0x0FFF) | 0x4000)
        << "-" << std::setw(12) << (static_cast<uint64_t>(dist(rng)) << 32 | dist(rng));
    return oss.str();
}

std::string VRAssaultEngine::start_session(const std::string& nickname,
                                            const std::string& vehicle_type,
                                            VRGameMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    UserSession session;
    session.session_id = generate_session_id(session_id_counter_, rng_);
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

    state.current_vibration.magnitude_level = ShockMagnitude::IMPERCEPTIBLE;
    state.current_vibration.amplitude_mm = 0.0;
    state.current_vibration.pitch_deg = 0.0;
    state.current_vibration.roll_deg = 0.0;
    state.current_vibration.yaw_deg = 0.0;
    state.current_vibration.frequency_hz = 0.0;
    state.current_vibration.duration_ms = 0.0;
    state.current_vibration.decay_rate_1_s = 0.0;
    state.current_vibration.force_feedback_n = 0.0;
    state.current_vibration.seat_acceleration_g = 0.0;
    state.current_vibration.visual_screen_shake_px = 0.0;
    state.current_vibration.audio_impact_intensity = 0.0;
    state.current_vibration.start_timestamp_ms = 0;

    state.roll_deg = 0.0;
    state.pitch_deg = 0.0;
    state.vertical_bounce_mm = 0.0;
    state.steering_force_feedback_nm = 0.0;
    state.throttle_force_feedback_n = 0.0;

    VRRoundResult result;
    result.session_id = session.session_id;
    result.start_ms = session.created_ms;
    result.end_ms = 0;
    result.final_health_percent = 100.0;
    result.total_distance_m = 0.0;
    result.total_attacks_sustained = 0;
    result.total_damage_taken = 0.0;
    result.waves_survived = 0;
    result.score = 0.0;
    result.rank = "D";

    sessions_[session.session_id] = session;
    states_[session.session_id] = state;
    round_results_[session.session_id] = result;
    game_modes_[session.session_id] = mode;

    sessions_created_.fetch_add(1);
    return session.session_id;
}

void VRAssaultEngine::end_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return;
    }

    auto state_it = states_.find(session_id);
    auto result_it = round_results_.find(session_id);
    auto mode_it = game_modes_.find(session_id);

    if (state_it != states_.end() && result_it != round_results_.end()) {
        UserVehicleState& state = state_it->second;
        VRRoundResult& result = result_it->second;

        result.end_ms = current_timestamp_ms();
        result.final_health_percent = state.health_percent;
        result.total_distance_m = state.distance_traveled_m;
        result.total_attacks_sustained = state.impacts_received;
        result.waves_survived = state.impacts_received / 5;

        result.score = calculate_score(state, state.distance_traveled_m, state.impacts_received);
        result.rank = calculate_rank(result.score);
    }

    sessions_.erase(session_it);
    if (mode_it != game_modes_.end()) {
        game_modes_.erase(mode_it);
    }
}

bool VRAssaultEngine::has_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(session_id) != sessions_.end();
}

UserSession VRAssaultEngine::get_session_info(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    return UserSession{};
}

UserVehicleState VRAssaultEngine::get_state(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = states_.find(session_id);
    if (it != states_.end()) {
        return it->second;
    }
    return UserVehicleState{};
}

UserVehicleState VRAssaultEngine::apply_action(const std::string& session_id,
                                                const std::string& action,
                                                double param1,
                                                double param2) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return UserVehicleState{};
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = states_.find(session_id);
    if (state_it == states_.end()) {
        return UserVehicleState{};
    }

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};
    double max_speed_ms = vp.max_speed_kmh > 0 ? vp.max_speed_kmh / 3.6 : 5.0;

    if (action == "throttle") {
        double throttle = std::max(-1.0, std::min(1.0, param1));
        state.speed_ms = std::max(-max_speed_ms * 0.5, std::min(max_speed_ms, state.speed_ms + throttle * max_speed_ms * 0.1));
    } else if (action == "steer") {
        double steer = std::max(-1.0, std::min(1.0, param1));
        state.heading_deg += steer * 10.0;
        while (state.heading_deg >= 360.0) state.heading_deg -= 360.0;
        while (state.heading_deg < 0.0) state.heading_deg += 360.0;
    } else if (action == "brake") {
        state.speed_ms = 0.0;
    } else if (action == "reset_position") {
        state.position_x = 0.0;
        state.position_y = 30.0;
        state.heading_deg = 180.0;
        state.speed_ms = 0.0;
    }

    state.timestamp_ms = current_timestamp_ms();

    if (state_callback_) {
        state_callback_(session_id, state);
    }

    return state;
}

std::vector<RockAttackEvent> VRAssaultEngine::tick(const std::string& session_id,
                                                    double dt_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<RockAttackEvent> events;

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return events;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = states_.find(session_id);
    if (state_it == states_.end()) {
        return events;
    }

    auto mode_it = game_modes_.find(session_id);
    VRGameMode mode = mode_it != game_modes_.end() ? mode_it->second : VRGameMode::FREE_DRIVE;

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};

    move_vehicle(state, 0.0, 0.0, dt_seconds);

    int attack_count = 1;
    double force_multiplier = 1.0;

    switch (mode) {
        case VRGameMode::FREE_DRIVE:
            attack_count = 1;
            force_multiplier = 0.5;
            break;
        case VRGameMode::ASSAULT_CHALLENGE:
            attack_count = 2;
            force_multiplier = 1.0;
            break;
        case VRGameMode::SURVIVAL: {
            int wave = state.impacts_received / 5;
            attack_count = std::min(5, 1 + wave);
            force_multiplier = 1.0 + wave * 0.15;
            break;
        }
        case VRGameMode::TIMED_RACE:
            attack_count = 1;
            force_multiplier = 0.8;
            break;
    }

    for (int i = 0; i < attack_count; ++i) {
        RockAttackEvent attack = generate_attack(session_id, state, force_multiplier);
        apply_attack_to_state(state, attack, vp);
        decay_vibration(state, dt_seconds / attack_count);
        events.push_back(attack);

        if (attack_callback_) {
            attack_callback_(attack);
        }
    }

    auto result_it = round_results_.find(session_id);
    if (result_it != round_results_.end()) {
        VRRoundResult& result = result_it->second;
        result.total_distance_m = state.distance_traveled_m;
        result.total_attacks_sustained = state.impacts_received;
        result.waves_survived = state.impacts_received / 5;
        result.final_health_percent = state.health_percent;
    }

    if (state_callback_) {
        state_callback_(session_id, state);
    }

    return events;
}

std::vector<RockAttackEvent> VRAssaultEngine::trigger_attack(const std::string& session_id,
                                                              double force_multiplier) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<RockAttackEvent> events;

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return events;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = states_.find(session_id);
    if (state_it == states_.end()) {
        return events;
    }

    UserVehicleState& state = state_it->second;
    VehicleProfile vp = config_ ? config_->get_vehicle(session_it->second.vehicle_type) : VehicleProfile{};

    RockAttackEvent attack = generate_attack(session_id, state, force_multiplier);
    apply_attack_to_state(state, attack, vp);
    events.push_back(attack);

    if (attack_callback_) {
        attack_callback_(attack);
    }

    if (state_callback_) {
        state_callback_(session_id, state);
    }

    return events;
}

void VRAssaultEngine::reset_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto session_it = sessions_.find(session_id);
    if (session_it == sessions_.end()) {
        return;
    }
    session_it->second.last_active_ms = current_timestamp_ms();

    auto state_it = states_.find(session_id);
    if (state_it != states_.end()) {
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

        state.current_vibration.magnitude_level = ShockMagnitude::IMPERCEPTIBLE;
        state.current_vibration.amplitude_mm = 0.0;
        state.current_vibration.pitch_deg = 0.0;
        state.current_vibration.roll_deg = 0.0;
        state.current_vibration.yaw_deg = 0.0;
        state.current_vibration.frequency_hz = 0.0;
        state.current_vibration.duration_ms = 0.0;
        state.current_vibration.decay_rate_1_s = 0.0;
        state.current_vibration.force_feedback_n = 0.0;
        state.current_vibration.seat_acceleration_g = 0.0;
        state.current_vibration.visual_screen_shake_px = 0.0;
        state.current_vibration.audio_impact_intensity = 0.0;
        state.current_vibration.start_timestamp_ms = 0;

        state.roll_deg = 0.0;
        state.pitch_deg = 0.0;
        state.vertical_bounce_mm = 0.0;
        state.steering_force_feedback_nm = 0.0;
        state.throttle_force_feedback_n = 0.0;
    }

    auto result_it = round_results_.find(session_id);
    if (result_it != round_results_.end()) {
        VRRoundResult& result = result_it->second;
        result.start_ms = current_timestamp_ms();
        result.end_ms = 0;
        result.final_health_percent = 100.0;
        result.total_distance_m = 0.0;
        result.total_attacks_sustained = 0;
        result.total_damage_taken = 0.0;
        result.waves_survived = 0;
        result.score = 0.0;
        result.rank = "D";
    }
}

std::vector<UserSession> VRAssaultEngine::active_sessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserSession> result;
    result.reserve(sessions_.size());
    for (const auto& kv : sessions_) {
        result.push_back(kv.second);
    }
    return result;
}

VRRoundResult VRAssaultEngine::get_round_result(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = round_results_.find(session_id);
    if (it != round_results_.end()) {
        return it->second;
    }
    return VRRoundResult{};
}

void VRAssaultEngine::cleanup_expired(int64_t idle_timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t now = current_timestamp_ms();

    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (now - it->second.last_active_ms > idle_timeout_ms) {
            const std::string& sid = it->first;
            states_.erase(sid);
            round_results_.erase(sid);
            game_modes_.erase(sid);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

ShockVibration VRAssaultEngine::compute_shock(double damage_level,
                                               double rock_mass_kg,
                                               double rock_velocity_ms,
                                               double impact_x,
                                               double vehicle_width_m,
                                               int64_t now_ms) {
    ShockVibration sv{};

    int level = static_cast<int>(std::max(0.0, std::min(4.0, damage_level)));

    switch (level) {
        case 0:
            sv.magnitude_level = ShockMagnitude::IMPERCEPTIBLE;
            sv.amplitude_mm = 0.1;
            sv.pitch_deg = 0.05;
            sv.roll_deg = 0.05;
            sv.frequency_hz = 20;
            sv.duration_ms = 80;
            sv.decay_rate_1_s = 2.0;
            sv.force_feedback_n = 0.5;
            sv.seat_acceleration_g = 0.02;
            sv.visual_screen_shake_px = 0;
            break;
        case 1:
            sv.magnitude_level = ShockMagnitude::MINOR;
            sv.amplitude_mm = 0.8;
            sv.pitch_deg = 0.3;
            sv.roll_deg = 0.4;
            sv.frequency_hz = 18;
            sv.duration_ms = 200;
            sv.decay_rate_1_s = 2.5;
            sv.force_feedback_n = 2.0;
            sv.seat_acceleration_g = 0.08;
            sv.visual_screen_shake_px = 1;
            break;
        case 2:
            sv.magnitude_level = ShockMagnitude::MODERATE;
            sv.amplitude_mm = 3.5;
            sv.pitch_deg = 1.2;
            sv.roll_deg = 1.8;
            sv.frequency_hz = 14;
            sv.duration_ms = 500;
            sv.decay_rate_1_s = 1.8;
            sv.force_feedback_n = 8.0;
            sv.seat_acceleration_g = 0.25;
            sv.visual_screen_shake_px = 3;
            break;
        case 3:
            sv.magnitude_level = ShockMagnitude::STRONG;
            sv.amplitude_mm = 12.0;
            sv.pitch_deg = 3.5;
            sv.roll_deg = 5.0;
            sv.frequency_hz = 10;
            sv.duration_ms = 1400;
            sv.decay_rate_1_s = 1.2;
            sv.force_feedback_n = 35.0;
            sv.seat_acceleration_g = 0.70;
            sv.visual_screen_shake_px = 8;
            break;
        case 4:
        default:
            sv.magnitude_level = ShockMagnitude::SEVERE;
            sv.amplitude_mm = 30.0;
            sv.pitch_deg = 8.0;
            sv.roll_deg = 12.0;
            sv.frequency_hz = 7;
            sv.duration_ms = 3000;
            sv.decay_rate_1_s = 0.8;
            sv.force_feedback_n = 120.0;
            sv.seat_acceleration_g = 1.80;
            sv.visual_screen_shake_px = 20;
            break;
    }

    double half_width = vehicle_width_m / 2.0;
    double x_normalized = (impact_x + half_width) / vehicle_width_m;

    if (x_normalized < 0.33) {
        sv.roll_deg = std::abs(sv.roll_deg);
    } else if (x_normalized > 0.67) {
        sv.roll_deg = -std::abs(sv.roll_deg);
    }

    std::uniform_real_distribution<double> pitch_dist(0.8, 1.2);
    sv.pitch_deg *= pitch_dist(rng_);

    sv.yaw_deg = sv.roll_deg * 0.3;

    sv.audio_impact_intensity = static_cast<double>(level) / 4.0;

    double velocity_factor = std::max(0.5, rock_velocity_ms / 14.0);
    sv.force_feedback_n *= velocity_factor;

    sv.start_timestamp_ms = now_ms;

    return sv;
}

RockAttackEvent VRAssaultEngine::generate_attack(const std::string& session_id,
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

    event.shock_effect = compute_shock(
        static_cast<double>(sim_result.damage_level),
        event.rock_mass_kg,
        event.rock_velocity_ms,
        local_y,
        roof_width,
        event.timestamp_ms
    );

    return event;
}

double VRAssaultEngine::compute_damage(const SimulationResult& sim,
                                        const VehicleProfile& vp) const {
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

void VRAssaultEngine::apply_attack_to_state(UserVehicleState& state,
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

    const ShockVibration& shock = attack.shock_effect;
    ShockVibration& cv = state.current_vibration;
    double cur_amp = cv.amplitude_mm;
    double new_amp = shock.amplitude_mm;
    double total_weight = cur_amp + new_amp;

    if (total_weight > 0) {
        double w_cur = cur_amp / total_weight;
        double w_new = new_amp / total_weight;

        cv.frequency_hz = std::max(cv.frequency_hz, shock.frequency_hz);
        cv.amplitude_mm = cur_amp + new_amp;
        cv.pitch_deg = cv.pitch_deg * w_cur + shock.pitch_deg * w_new;
        cv.roll_deg = cv.roll_deg * w_cur + shock.roll_deg * w_new;
        cv.yaw_deg = cv.yaw_deg * w_cur + shock.yaw_deg * w_new;
        cv.decay_rate_1_s = std::min(cv.decay_rate_1_s > 0 ? cv.decay_rate_1_s : 100.0, shock.decay_rate_1_s);
        cv.duration_ms = std::max(cv.duration_ms, shock.duration_ms);
        cv.seat_acceleration_g = cv.seat_acceleration_g * w_cur + shock.seat_acceleration_g * w_new;
        cv.visual_screen_shake_px = cv.visual_screen_shake_px * w_cur + shock.visual_screen_shake_px * w_new;
        cv.audio_impact_intensity = std::max(cv.audio_impact_intensity, shock.audio_impact_intensity);
        cv.force_feedback_n = cv.force_feedback_n * w_cur + shock.force_feedback_n * w_new;

        if (static_cast<uint8_t>(shock.magnitude_level) > static_cast<uint8_t>(cv.magnitude_level)) {
            cv.magnitude_level = shock.magnitude_level;
        }
    } else {
        cv = shock;
    }
    cv.start_timestamp_ms = attack.timestamp_ms;

    state.roll_deg = cv.roll_deg;
    state.pitch_deg = cv.pitch_deg;
    state.vertical_bounce_mm = cv.amplitude_mm;

    double side_factor = 1.0 + std::abs(shock.roll_deg) / 12.0;
    state.steering_force_feedback_nm = shock.force_feedback_n * side_factor;
    state.throttle_force_feedback_n = shock.force_feedback_n * 0.5;

    state.timestamp_ms = current_timestamp_ms();

    auto result_it = round_results_.find(state.session_id);
    if (result_it != round_results_.end()) {
        result_it->second.total_damage_taken += attack.damage_dealt;
    }
}

void VRAssaultEngine::move_vehicle(UserVehicleState& state,
                                    double throttle,
                                    double steering,
                                    double dt) {
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

void VRAssaultEngine::decay_vibration(UserVehicleState& state, double dt) {
    ShockVibration& cv = state.current_vibration;
    double original_amplitude = cv.amplitude_mm;

    if (cv.decay_rate_1_s > 0 && original_amplitude > 0) {
        double decay_factor = std::exp(-cv.decay_rate_1_s * dt);
        cv.amplitude_mm *= decay_factor;
        cv.pitch_deg *= decay_factor;
        cv.roll_deg *= decay_factor;
        cv.yaw_deg *= decay_factor;
        cv.seat_acceleration_g *= decay_factor;
        cv.visual_screen_shake_px *= decay_factor;
        cv.force_feedback_n *= decay_factor;

        state.roll_deg = cv.roll_deg;
        state.pitch_deg = cv.pitch_deg;
        state.vertical_bounce_mm = cv.amplitude_mm;
        state.steering_force_feedback_nm *= decay_factor;
        state.throttle_force_feedback_n *= decay_factor;

        int64_t elapsed_ms = state.timestamp_ms - cv.start_timestamp_ms;
        bool duration_passed = cv.duration_ms > 0 && elapsed_ms > cv.duration_ms;
        bool amplitude_negligible = original_amplitude > 0 && cv.amplitude_mm < original_amplitude * 0.01;

        if (duration_passed && amplitude_negligible) {
            cv.magnitude_level = ShockMagnitude::IMPERCEPTIBLE;
            cv.amplitude_mm = 0.0;
            cv.pitch_deg = 0.0;
            cv.roll_deg = 0.0;
            cv.yaw_deg = 0.0;
            cv.frequency_hz = 0.0;
            cv.duration_ms = 0.0;
            cv.decay_rate_1_s = 0.0;
            cv.force_feedback_n = 0.0;
            cv.seat_acceleration_g = 0.0;
            cv.visual_screen_shake_px = 0.0;
            cv.audio_impact_intensity = 0.0;
            cv.start_timestamp_ms = 0;

            state.roll_deg = 0.0;
            state.pitch_deg = 0.0;
            state.vertical_bounce_mm = 0.0;
            state.steering_force_feedback_nm = 0.0;
            state.throttle_force_feedback_n = 0.0;
        }
    }
}

double VRAssaultEngine::calculate_score(const UserVehicleState& state,
                                         double distance,
                                         int attacks) const {
    double damage_taken = 100.0 - state.health_percent;
    int waves = attacks / 5;
    return distance * 0.3 + (100.0 - damage_taken) * 2.0 + waves * 50.0;
}

std::string VRAssaultEngine::calculate_rank(double score) const {
    if (score > 2000.0) {
        return "S";
    } else if (score > 1500.0) {
        return "A";
    } else if (score > 1000.0) {
        return "B";
    } else if (score > 500.0) {
        return "C";
    } else {
        return "D";
    }
}

}
