#include <gtest/gtest.h>
#include "vr_assault/vr_assault_engine.h"
#include "config/config_loader.h"
#include "common/data_types.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>

namespace fenyun {

class VRAssaultEngineUnit : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = test::create_test_vr_assault_engine();
        ASSERT_NE(engine_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        engine_.reset();
        config_.reset();
    }

    std::shared_ptr<VRAssaultEngine> engine_;
    std::shared_ptr<ConfigLoader> config_;
};

TEST_F(VRAssaultEngineUnit, SessionManagement_StartSession_ReturnsValidSessionId) {
    std::string session_id = engine_->start_session("test_user", "fenyun_basic", VRGameMode::FREE_DRIVE);

    EXPECT_FALSE(session_id.empty()) << "session_id 不应为空";
    EXPECT_TRUE(engine_->has_session(session_id)) << "新创建的会话应该存在";
}

TEST_F(VRAssaultEngineUnit, SessionManagement_StartSession_InitialStateIsValid) {
    std::string session_id = engine_->start_session("player1", "fenyun_basic", VRGameMode::FREE_DRIVE);

    UserVehicleState state = engine_->get_state(session_id);

    EXPECT_EQ(state.session_id, session_id);
    EXPECT_NEAR(state.health_percent, 100.0, 0.001) << "初始生命值应为 100%";
    EXPECT_NEAR(state.armor_integrity_percent, 100.0, 0.001) << "初始装甲完整度应为 100%";
    EXPECT_EQ(state.impacts_received, 0u) << "初始受击次数应为 0";
    EXPECT_NEAR(state.speed_ms, 0.0, 0.001) << "初始速度应为 0";
}

TEST_F(VRAssaultEngineUnit, SessionManagement_EndSession_SessionRemoved) {
    std::string session_id = engine_->start_session("player1", "fenyun_basic", VRGameMode::FREE_DRIVE);

    ASSERT_TRUE(engine_->has_session(session_id));

    engine_->end_session(session_id);

    EXPECT_FALSE(engine_->has_session(session_id)) << "结束后会话应该不存在";
}

TEST_F(VRAssaultEngineUnit, SessionManagement_HasSession_InvalidSessionReturnsFalse) {
    EXPECT_FALSE(engine_->has_session("nonexistent_session_id"));
    EXPECT_FALSE(engine_->has_session(""));
}

TEST_F(VRAssaultEngineUnit, SessionManagement_GetSessionInfo_ValidSession) {
    std::string session_id = engine_->start_session("test_player", "fenyun_basic", VRGameMode::ASSAULT_CHALLENGE);

    UserSession info = engine_->get_session_info(session_id);

    EXPECT_EQ(info.session_id, session_id);
    EXPECT_EQ(info.user_nickname, "test_player");
    EXPECT_EQ(info.vehicle_type, "fenyun_basic");
    EXPECT_GT(info.created_ms, 0);
    EXPECT_GT(info.last_active_ms, 0);
}

TEST_F(VRAssaultEngineUnit, SessionManagement_ActiveSessions_ReturnsAllSessions) {
    EXPECT_EQ(engine_->active_sessions().size(), 0u) << "初始时没有活动会话";

    std::string id1 = engine_->start_session("player1", "fenyun_basic", VRGameMode::FREE_DRIVE);
    std::string id2 = engine_->start_session("player2", "fenyun_heavy", VRGameMode::SURVIVAL);

    std::vector<UserSession> sessions = engine_->active_sessions();
    EXPECT_EQ(sessions.size(), 2u) << "应该有 2 个活动会话";

    engine_->end_session(id1);
    EXPECT_EQ(engine_->active_sessions().size(), 1u) << "结束一个后应该剩 1 个";

    engine_->end_session(id2);
    EXPECT_EQ(engine_->active_sessions().size(), 0u) << "都结束后应该剩 0 个";
}

TEST_F(VRAssaultEngineUnit, VehicleControl_ApplyThrottle_IncreasesSpeed) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    UserVehicleState initial = engine_->get_state(session_id);
    ASSERT_NEAR(initial.speed_ms, 0.0, 0.001);

    UserVehicleState after = engine_->apply_action(session_id, "throttle", 1.0, 0.0);

    EXPECT_GT(after.speed_ms, 0.0) << "踩油门后速度应该大于 0";
}

TEST_F(VRAssaultEngineUnit, VehicleControl_ApplySteer_ChangesHeading) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    UserVehicleState initial = engine_->get_state(session_id);
    double initial_heading = initial.heading_deg;

    UserVehicleState after = engine_->apply_action(session_id, "steer", 1.0, 0.0);

    EXPECT_GT(after.heading_deg, initial_heading) << "向右转向应该增加航向角";
}

TEST_F(VRAssaultEngineUnit, VehicleControl_ApplyBrake_StopsVehicle) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    engine_->apply_action(session_id, "throttle", 1.0, 0.0);
    UserVehicleState after_throttle = engine_->get_state(session_id);
    ASSERT_GT(after_throttle.speed_ms, 0.0);

    UserVehicleState after_brake = engine_->apply_action(session_id, "brake", 0.0, 0.0);
    EXPECT_NEAR(after_brake.speed_ms, 0.0, 0.001) << "刹车后速度应该为 0";
}

TEST_F(VRAssaultEngineUnit, VehicleControl_ResetPosition_ResetsToInitial) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    engine_->apply_action(session_id, "throttle", 1.0, 0.0);
    engine_->apply_action(session_id, "steer", 0.5, 0.0);

    UserVehicleState after = engine_->apply_action(session_id, "reset_position", 0.0, 0.0);

    EXPECT_NEAR(after.position_x, 0.0, 0.001);
    EXPECT_NEAR(after.position_y, 30.0, 0.001);
    EXPECT_NEAR(after.heading_deg, 180.0, 0.001);
    EXPECT_NEAR(after.speed_ms, 0.0, 0.001);
}

TEST_F(VRAssaultEngineUnit, VehicleControl_InvalidAction_DoesNotCrash) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    EXPECT_NO_FATAL_FAILURE({
        engine_->apply_action(session_id, "invalid_action", 0.0, 0.0);
    });
}

TEST_F(VRAssaultEngineUnit, Combat_TriggerAttack_GeneratesAttackEvent) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::ASSAULT_CHALLENGE);

    std::vector<RockAttackEvent> attacks = engine_->trigger_attack(session_id, 1.0);

    EXPECT_GT(attacks.size(), 0u) << "触发攻击应该生成至少一个攻击事件";

    for (const auto& attack : attacks) {
        EXPECT_GT(attack.attack_id, 0u);
        EXPECT_GT(attack.rock_mass_kg, 0.0);
        EXPECT_GT(attack.impact_velocity_ms, 0.0);
    }
}

TEST_F(VRAssaultEngineUnit, Combat_TriggerAttack_ReducesHealth) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::SURVIVAL);

    UserVehicleState before = engine_->get_state(session_id);
    ASSERT_NEAR(before.health_percent, 100.0, 0.001);

    engine_->trigger_attack(session_id, 2.0);
    UserVehicleState after = engine_->get_state(session_id);

    EXPECT_LT(after.health_percent, 100.0) << "受到攻击后生命值应该下降";
    EXPECT_GT(after.impacts_received, 0u) << "受击次数应该增加";
}

TEST_F(VRAssaultEngineUnit, Combat_Tick_GeneratesAttacksOverTime) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::ASSAULT_CHALLENGE);

    uint32_t initial_impacts = engine_->get_state(session_id).impacts_received;

    std::vector<RockAttackEvent> attacks = engine_->tick(session_id, 1.0);

    UserVehicleState after = engine_->get_state(session_id);
    EXPECT_GE(after.impacts_received, initial_impacts) << "tick 后受击次数不应减少";
}

TEST_F(VRAssaultEngineUnit, Stats_TotalSessions_IncrementsCorrectly) {
    uint64_t initial = engine_->total_sessions();

    std::string id1 = engine_->start_session("p1", "fenyun_basic", VRGameMode::FREE_DRIVE);
    EXPECT_EQ(engine_->total_sessions(), initial + 1);

    std::string id2 = engine_->start_session("p2", "fenyun_heavy", VRGameMode::SURVIVAL);
    EXPECT_EQ(engine_->total_sessions(), initial + 2);

    engine_->end_session(id1);
    EXPECT_EQ(engine_->total_sessions(), initial + 2) << "结束会话不减少总创建数";
}

TEST_F(VRAssaultEngineUnit, GameModes_AllModesCanStart) {
    std::vector<VRGameMode> modes = {
        VRGameMode::FREE_DRIVE,
        VRGameMode::ASSAULT_CHALLENGE,
        VRGameMode::SURVIVAL,
        VRGameMode::TIMED_RACE
    };

    for (auto mode : modes) {
        std::string session_id = engine_->start_session("test_player", "fenyun_basic", mode);
        EXPECT_FALSE(session_id.empty()) << "模式 " << static_cast<int>(mode) << " 应该能创建会话";
        EXPECT_TRUE(engine_->has_session(session_id));
        engine_->end_session(session_id);
    }
}

TEST_F(VRAssaultEngineUnit, RoundResult_EndSession_HasValidResult) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::ASSAULT_CHALLENGE);

    engine_->trigger_attack(session_id, 1.0);
    engine_->apply_action(session_id, "throttle", 1.0, 0.0);

    VRRoundResult result_before = engine_->get_round_result(session_id);
    EXPECT_EQ(result_before.session_id, session_id);

    engine_->end_session(session_id);

    EXPECT_FALSE(result_before.rank.empty());
    EXPECT_GE(result_before.final_health_percent, 0.0);
}

TEST_F(VRAssaultEngineUnit, EdgeCases_EndNonExistentSession_DoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({
        engine_->end_session("nonexistent_session");
    });
}

TEST_F(VRAssaultEngineUnit, EdgeCases_GetStateNonExistentSession_ReturnsDefault) {
    UserVehicleState state = engine_->get_state("nonexistent_session");
    EXPECT_EQ(state.session_id, "");
}

TEST_F(VRAssaultEngineUnit, EdgeCases_ResetSession_ResetsState) {
    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    engine_->trigger_attack(session_id, 2.0);
    engine_->apply_action(session_id, "throttle", 1.0, 0.0);

    UserVehicleState before_reset = engine_->get_state(session_id);
    ASSERT_LT(before_reset.health_percent, 100.0);

    engine_->reset_session(session_id);
    UserVehicleState after_reset = engine_->get_state(session_id);

    EXPECT_NEAR(after_reset.health_percent, 100.0, 0.001) << "重置后生命值应该恢复到 100%";
    EXPECT_EQ(after_reset.impacts_received, 0u) << "重置后受击次数应该为 0";
    EXPECT_NEAR(after_reset.speed_ms, 0.0, 0.001) << "重置后速度应该为 0";
}

TEST_F(VRAssaultEngineUnit, Callbacks_StateCallback_TriggeredOnAction) {
    int callback_count = 0;
    std::string last_session_id;

    engine_->set_state_callback([&](const std::string& session_id, const UserVehicleState&) {
        callback_count++;
        last_session_id = session_id;
    });

    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);

    engine_->apply_action(session_id, "throttle", 1.0, 0.0);

    EXPECT_GE(callback_count, 1) << "执行动作后应该触发状态回调";
    EXPECT_EQ(last_session_id, session_id);
}

TEST_F(VRAssaultEngineUnit, Cleanup_CleanupExpired_DoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE({
        engine_->cleanup_expired(1000);
    });

    std::string session_id = engine_->start_session("test", "fenyun_basic", VRGameMode::FREE_DRIVE);
    EXPECT_TRUE(engine_->has_session(session_id));

    EXPECT_NO_FATAL_FAILURE({
        engine_->cleanup_expired(300000);
    });
    EXPECT_TRUE(engine_->has_session(session_id)) << "未过期的会话不应该被清理";
}

}
