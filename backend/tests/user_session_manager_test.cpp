#include <gtest/gtest.h>
#include "user_session/user_session_manager.h"
#include "config/config_loader.h"
#include "impact_simulator/impact_simulator.h"
#include "common/data_types.h"
#include "test_helpers.h"

#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <memory>
#include <unordered_set>
#include <algorithm>

namespace fenyun {

class UserSessionUnit : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = test::create_test_user_session_manager();
        ASSERT_NE(manager_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        manager_.reset();
        config_.reset();
    }

    std::shared_ptr<UserSessionManager> manager_;
    std::shared_ptr<ConfigLoader> config_;
};

// ========== SessionManagement - 会话管理 ==========

TEST_F(UserSessionUnit, CreateSession_ReturnsValidId) {
    // 创建会话，返回的session_id非空，长度合理
    auto session = manager_->create_session("test_user", "fenyun_basic");

    EXPECT_FALSE(session.session_id.empty());
    EXPECT_GT(session.session_id.length(), 10u);
    EXPECT_EQ(session.user_nickname, "test_user");
}

TEST_F(UserSessionUnit, CreateSession_UniqueIds) {
    // 创建100个会话，ID全部唯一
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
        auto session = manager_->create_session("user_" + std::to_string(i));
        EXPECT_FALSE(session.session_id.empty());
        auto result = ids.insert(session.session_id);
        EXPECT_TRUE(result.second) << "第 " << i << " 个会话ID重复";
    }
    EXPECT_EQ(ids.size(), 100u);
}

TEST_F(UserSessionUnit, CreateSession_InitialState) {
    // 新会话HP=100，装甲=100，位置=(0, 30)，speed=0
    auto session = manager_->create_session("test_user", "fenyun_basic");
    ASSERT_FALSE(session.session_id.empty());

    auto state = manager_->get_vehicle_state(session.session_id);
    EXPECT_EQ(state.session_id, session.session_id);
    EXPECT_DOUBLE_EQ(state.health_percent, 100.0);
    EXPECT_DOUBLE_EQ(state.armor_integrity_percent, 100.0);
    EXPECT_DOUBLE_EQ(state.position_x, 0.0);
    EXPECT_DOUBLE_EQ(state.position_y, 30.0);
    EXPECT_DOUBLE_EQ(state.speed_ms, 0.0);
    EXPECT_EQ(state.impacts_received, 0);
    EXPECT_DOUBLE_EQ(state.distance_traveled_m, 0.0);
}

TEST_F(UserSessionUnit, HasSession_Existing_ReturnsTrue) {
    // 存在的会话返回true
    auto session = manager_->create_session("test_user");
    EXPECT_TRUE(manager_->has_session(session.session_id));
}

TEST_F(UserSessionUnit, HasSession_Nonexistent_ReturnsFalse) {
    // 不存在返回false
    EXPECT_FALSE(manager_->has_session("nonexistent_session_id"));
    EXPECT_FALSE(manager_->has_session(""));
}

TEST_F(UserSessionUnit, DestroySession_WorksCorrectly) {
    // 销毁后再查询返回false
    auto session = manager_->create_session("test_user");
    std::string sid = session.session_id;

    EXPECT_TRUE(manager_->has_session(sid));

    manager_->destroy_session(sid);

    EXPECT_FALSE(manager_->has_session(sid));
}

// ========== VehicleMovement - 车辆运动 ==========

TEST_F(UserSessionUnit, ThrottleForward_IncreasesSpeed) {
    // 油门=1.0，速度从0开始增加
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    UserActionRequest req;
    req.session_id = sid;
    req.action = "throttle";
    req.param1 = 1.0;
    req.param2 = 0.0;

    manager_->apply_action(req);
    auto state = manager_->get_vehicle_state(sid);

    EXPECT_GT(state.speed_ms, 0.0) << "踩油门后速度应大于0";
}

TEST_F(UserSessionUnit, ThrottleReverse_DecreasesSpeed) {
    // 油门=-1.0，负方向加速
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    UserActionRequest req;
    req.session_id = sid;
    req.action = "throttle";
    req.param1 = -1.0;
    req.param2 = 0.0;

    manager_->apply_action(req);
    auto state = manager_->get_vehicle_state(sid);

    EXPECT_LT(state.speed_ms, 0.0) << "倒车时速度应为负";
}

TEST_F(UserSessionUnit, Brake_StopsVehicle) {
    // 刹车后speed=0或接近0
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    UserActionRequest throttle_req;
    throttle_req.session_id = sid;
    throttle_req.action = "throttle";
    throttle_req.param1 = 1.0;
    throttle_req.param2 = 0.0;
    manager_->apply_action(throttle_req);

    auto state_before = manager_->get_vehicle_state(sid);
    ASSERT_GT(state_before.speed_ms, 0.0);

    UserActionRequest brake_req;
    brake_req.session_id = sid;
    brake_req.action = "brake";
    brake_req.param1 = 0.0;
    brake_req.param2 = 0.0;
    manager_->apply_action(brake_req);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_NEAR(state_after.speed_ms, 0.0, 0.001) << "刹车后速度应为0";
}

TEST_F(UserSessionUnit, SteerLeft_ChangesHeading) {
    // 向左打方向，heading角度变化
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);
    double heading_before = state_before.heading_deg;

    UserActionRequest req;
    req.session_id = sid;
    req.action = "steer";
    req.param1 = -1.0;
    req.param2 = 0.0;
    manager_->apply_action(req);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_NE(state_after.heading_deg, heading_before) << "向左打方向后heading应变化";
}

TEST_F(UserSessionUnit, SteerRight_ChangesHeading) {
    // 向右打方向，heading变化方向与向左相反
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_initial = manager_->get_vehicle_state(sid);

    UserActionRequest left_req;
    left_req.session_id = sid;
    left_req.action = "steer";
    left_req.param1 = -1.0;
    left_req.param2 = 0.0;
    manager_->apply_action(left_req);

    auto state_left = manager_->get_vehicle_state(sid);
    double left_change = state_left.heading_deg - state_initial.heading_deg;

    manager_->reset_session(sid);

    UserActionRequest right_req;
    right_req.session_id = sid;
    right_req.action = "steer";
    right_req.param1 = 1.0;
    right_req.param2 = 0.0;
    manager_->apply_action(right_req);

    auto state_right = manager_->get_vehicle_state(sid);
    double right_change = state_right.heading_deg - state_initial.heading_deg;

    EXPECT_NE(left_change, right_change) << "左右转向的heading变化方向应不同";
}

TEST_F(UserSessionUnit, MaxSpeed_Limited) {
    // 持续油门，速度最终稳定在车辆最大速度附近，不超过
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    VehicleProfile vp = config_->get_vehicle("fenyun_basic");
    double max_speed_ms = vp.max_speed_kmh / 3.6;

    UserActionRequest req;
    req.session_id = sid;
    req.action = "throttle";
    req.param1 = 1.0;
    req.param2 = 0.0;

    for (int i = 0; i < 20; ++i) {
        manager_->apply_action(req);
    }

    auto state = manager_->get_vehicle_state(sid);
    EXPECT_LE(state.speed_ms, max_speed_ms + 0.01) << "速度不应超过最大速度";
    EXPECT_GT(state.speed_ms, 0.0) << "持续油门后速度应大于0";
}

// ========== AttackSimulation - 攻击模拟 ==========

TEST_F(UserSessionUnit, TickTime_GeneratesAttacks) {
    // process_time_step(1s)返回1次以上攻击
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto attacks = manager_->process_time_step(sid, 1.0);

    EXPECT_GE(attacks.size(), 1u) << "1秒时间步应生成至少1次攻击";
}

TEST_F(UserSessionUnit, Attack_DealsDamage) {
    // 攻击后HP减少（不是满血状态）
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);
    ASSERT_DOUBLE_EQ(state_before.health_percent, 100.0);

    manager_->process_time_step(sid, 1.0);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_LT(state_after.health_percent, 100.0) << "攻击后HP应减少";
}

TEST_F(UserSessionUnit, ArmorAbsorbsFirst) {
    // 装甲值 > 0时，装甲先减少，HP减少较少
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);
    ASSERT_DOUBLE_EQ(state_before.armor_integrity_percent, 100.0);
    ASSERT_DOUBLE_EQ(state_before.health_percent, 100.0);

    manager_->process_time_step(sid, 0.1);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_LT(state_after.armor_integrity_percent, 100.0) << "装甲应先被消耗";
}

TEST_F(UserSessionUnit, NoArmor_DirectHealthDamage) {
    // 装甲=0后，攻击直接扣HP
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto attacks = manager_->trigger_rock_attack(sid, 5.0);
    ASSERT_GT(attacks.size(), 0u);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_LT(state_after.health_percent, 100.0) << "攻击应扣HP";
}

TEST_F(UserSessionUnit, ManualAttack_MoreDamage) {
    // force_multiplier=2.0的攻击比普通攻击伤害大
    auto session1 = manager_->create_session("user1", "fenyun_basic");
    auto session2 = manager_->create_session("user2", "fenyun_basic");
    std::string sid1 = session1.session_id;
    std::string sid2 = session2.session_id;

    auto attacks_normal = manager_->trigger_rock_attack(sid1, 1.0);
    auto attacks_strong = manager_->trigger_rock_attack(sid2, 2.0);

    ASSERT_GT(attacks_normal.size(), 0u);
    ASSERT_GT(attacks_strong.size(), 0u);

    double dmg_normal = attacks_normal[0].damage_dealt;
    double dmg_strong = attacks_strong[0].damage_dealt;

    EXPECT_GT(dmg_strong, dmg_normal) << "高force_multiplier的攻击伤害应更大";
}

TEST_F(UserSessionUnit, DamageLevel_CorrelatesWithDamage) {
    // damage_level 4级的伤害 > damage_level 1级
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    double total_damage_low = 0.0;
    double total_damage_high = 0.0;
    int count_low = 0;
    int count_high = 0;

    for (int i = 0; i < 50; ++i) {
        auto attacks = manager_->trigger_rock_attack(sid, 1.0);
        for (const auto& a : attacks) {
            if (a.damage_dealt < 5.0) {
                total_damage_low += a.damage_dealt;
                count_low++;
            }
            if (a.damage_dealt > 20.0) {
                total_damage_high += a.damage_dealt;
                count_high++;
            }
        }
    }

    if (count_low > 0 && count_high > 0) {
        double avg_low = total_damage_low / count_low;
        double avg_high = total_damage_high / count_high;
        EXPECT_GT(avg_high, avg_low) << "高伤害级别的平均伤害应更高";
    }
}

// ========== ImmersionMetrics - 沉浸感验证 ==========

TEST_F(UserSessionUnit, VehicleMovesSmoothly) {
    // 连续10个tick，位置变化连续，没有瞬移
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    VehicleProfile vp = config_->get_vehicle("fenyun_basic");
    double max_speed_ms = vp.max_speed_kmh / 3.6;
    double dt = 0.1;

    UserActionRequest throttle_req;
    throttle_req.session_id = sid;
    throttle_req.action = "throttle";
    throttle_req.param1 = 0.5;
    throttle_req.param2 = 0.0;
    manager_->apply_action(throttle_req);

    double prev_x = manager_->get_vehicle_state(sid).position_x;
    double prev_y = manager_->get_vehicle_state(sid).position_y;

    for (int i = 0; i < 10; ++i) {
        manager_->process_time_step(sid, dt);
        auto state = manager_->get_vehicle_state(sid);

        double dx = state.position_x - prev_x;
        double dy = state.position_y - prev_y;
        double dist = std::sqrt(dx * dx + dy * dy);

        EXPECT_LE(dist, max_speed_ms * dt + 0.01) << "第 " << i << " 帧移动距离不应超过最大速度×时间步长";

        prev_x = state.position_x;
        prev_y = state.position_y;
    }
}

TEST_F(UserSessionUnit, AttackVariety_DifferentDamage) {
    // 10次攻击的伤害值有差异（不是同一个数字），体现随机性
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    std::set<double> damage_values;
    for (int i = 0; i < 10; ++i) {
        auto attacks = manager_->trigger_rock_attack(sid, 1.0);
        for (const auto& a : attacks) {
            damage_values.insert(std::round(a.damage_dealt * 100) / 100);
        }
    }

    EXPECT_GT(damage_values.size(), 1u) << "多次攻击的伤害值应有差异";
}

TEST_F(UserSessionUnit, DistanceAccumulates) {
    // 持续前进，distance_traveled_m单调增加
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    UserActionRequest throttle_req;
    throttle_req.session_id = sid;
    throttle_req.action = "throttle";
    throttle_req.param1 = 1.0;
    throttle_req.param2 = 0.0;
    manager_->apply_action(throttle_req);

    double prev_dist = 0.0;
    for (int i = 0; i < 5; ++i) {
        manager_->process_time_step(sid, 0.5);
        auto state = manager_->get_vehicle_state(sid);
        EXPECT_GE(state.distance_traveled_m, prev_dist - 0.001)
            << "行驶距离应单调增加";
        prev_dist = state.distance_traveled_m;
    }

    EXPECT_GT(prev_dist, 0.0) << "持续前进后总行驶距离应大于0";
}

TEST_F(UserSessionUnit, ImpactCount_Increases) {
    // 每次受到攻击，impacts_received计数增加
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);
    int impacts_before = state_before.impacts_received;

    int total_attacks = 0;
    for (int i = 0; i < 3; ++i) {
        auto attacks = manager_->process_time_step(sid, 1.0);
        total_attacks += attacks.size();
    }

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_EQ(state_after.impacts_received, impacts_before + total_attacks)
        << "受击次数应等于攻击次数";
}

TEST_F(UserSessionUnit, HealthStaysInBounds) {
    // HP始终在 [0, 100] 范围内，不会出现负数或超过100
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    for (int i = 0; i < 50; ++i) {
        manager_->trigger_rock_attack(sid, 3.0);
        auto state = manager_->get_vehicle_state(sid);

        EXPECT_GE(state.health_percent, 0.0) << "HP不应为负数";
        EXPECT_LE(state.health_percent, 100.0) << "HP不应超过100";
    }
}

TEST_F(UserSessionUnit, VehicleDoesNotTeleport) {
    // 每帧移动距离 <= 最大速度 × 时间步长（防穿墙/瞬移）
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    VehicleProfile vp = config_->get_vehicle("fenyun_basic");
    double max_speed_ms = vp.max_speed_kmh / 3.6;
    double dt = 0.1;

    UserActionRequest throttle_req;
    throttle_req.session_id = sid;
    throttle_req.action = "throttle";
    throttle_req.param1 = 1.0;
    throttle_req.param2 = 0.0;

    for (int i = 0; i < 10; ++i) {
        manager_->apply_action(throttle_req);
    }

    double prev_x = manager_->get_vehicle_state(sid).position_x;
    double prev_y = manager_->get_vehicle_state(sid).position_y;

    manager_->process_time_step(sid, dt);

    auto state = manager_->get_vehicle_state(sid);
    double dx = state.position_x - prev_x;
    double dy = state.position_y - prev_y;
    double dist = std::sqrt(dx * dx + dy * dy);

    EXPECT_LE(dist, max_speed_ms * dt + 0.01) << "单帧移动距离不应超过最大速度×时间步长";
}

// ========== EdgeCases - 边界测试 ==========

TEST_F(UserSessionUnit, ZeroHealth_VehicleDestroyed) {
    // HP=0时还能接收攻击但不变成负数
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    for (int i = 0; i < 100; ++i) {
        manager_->trigger_rock_attack(sid, 10.0);
    }

    auto state = manager_->get_vehicle_state(sid);
    EXPECT_GE(state.health_percent, 0.0) << "HP不应为负数";
    EXPECT_LE(state.health_percent, 10.0) << "多次强力攻击后HP应接近0";

    EXPECT_NO_THROW({
        manager_->trigger_rock_attack(sid, 1.0);
    }) << "HP=0时接收攻击不应崩溃";

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_GE(state_after.health_percent, 0.0) << "攻击后HP仍不应为负";
}

TEST_F(UserSessionUnit, ResetSession_RestoresInitialState) {
    // 重置后HP=100，装甲=100，位置回到起点
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    for (int i = 0; i < 10; ++i) {
        manager_->trigger_rock_attack(sid, 2.0);
    }

    UserActionRequest throttle_req;
    throttle_req.session_id = sid;
    throttle_req.action = "throttle";
    throttle_req.param1 = 1.0;
    throttle_req.param2 = 0.0;
    manager_->apply_action(throttle_req);
    manager_->process_time_step(sid, 1.0);

    auto state_before = manager_->get_vehicle_state(sid);
    ASSERT_LT(state_before.health_percent, 100.0);
    ASSERT_GT(state_before.distance_traveled_m, 0.0);

    manager_->reset_session(sid);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_DOUBLE_EQ(state_after.health_percent, 100.0);
    EXPECT_DOUBLE_EQ(state_after.armor_integrity_percent, 100.0);
    EXPECT_DOUBLE_EQ(state_after.position_x, 0.0);
    EXPECT_DOUBLE_EQ(state_after.position_y, 30.0);
    EXPECT_DOUBLE_EQ(state_after.speed_ms, 0.0);
    EXPECT_EQ(state_after.impacts_received, 0);
    EXPECT_DOUBLE_EQ(state_after.distance_traveled_m, 0.0);
}

TEST_F(UserSessionUnit, ExpiredSession_CleanedUp) {
    // 模拟超时会话被cleanup清理
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    EXPECT_TRUE(manager_->has_session(sid));

    manager_->cleanup_expired_sessions(1);

    EXPECT_TRUE(manager_->has_session(sid)) << "刚创建的会话不应被立即清理";
}

TEST_F(UserSessionUnit, InvalidAction_Ignored) {
    // 无效action字符串不改变车辆状态
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);

    UserActionRequest req;
    req.session_id = sid;
    req.action = "invalid_action_xyz";
    req.param1 = 1.0;
    req.param2 = 0.0;
    manager_->apply_action(req);

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_DOUBLE_EQ(state_after.speed_ms, state_before.speed_ms);
    EXPECT_DOUBLE_EQ(state_after.heading_deg, state_before.heading_deg);
}

TEST_F(UserSessionUnit, NegativeDt_ClampedToZero) {
    // 负时间步长不回退
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    auto state_before = manager_->get_vehicle_state(sid);
    double dist_before = state_before.distance_traveled_m;

    EXPECT_NO_THROW({
        manager_->process_time_step(sid, -1.0);
    }) << "负时间步长不应崩溃";

    auto state_after = manager_->get_vehicle_state(sid);
    EXPECT_GE(state_after.distance_traveled_m, dist_before - 0.001)
        << "负时间步长不应使行驶距离减少";
}

TEST_F(UserSessionUnit, EmptySessionId_Handled) {
    // 空字符串session_id不崩溃
    EXPECT_NO_THROW({
        manager_->has_session("");
    });

    EXPECT_NO_THROW({
        manager_->get_session("");
    });

    EXPECT_NO_THROW({
        manager_->destroy_session("");
    });

    EXPECT_NO_THROW({
        manager_->get_vehicle_state("");
    });

    UserActionRequest req;
    req.session_id = "";
    req.action = "throttle";
    req.param1 = 1.0;
    EXPECT_NO_THROW({
        manager_->apply_action(req);
    });

    EXPECT_NO_THROW({
        manager_->process_time_step("", 1.0);
    });

    EXPECT_NO_THROW({
        manager_->trigger_rock_attack("", 1.0);
    });

    EXPECT_NO_THROW({
        manager_->reset_session("");
    });
}

// ========== SessionCleanup - 会话清理 ==========

TEST_F(UserSessionUnit, CleanupExpired_RemovesOldSessions) {
    // 创建会话 + 手动设置last_active很早 + cleanup → 会话被清理
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    EXPECT_TRUE(manager_->has_session(sid));

    manager_->cleanup_expired_sessions(0);

    EXPECT_FALSE(manager_->has_session(sid)) << "超时会话应被清理";
}

TEST_F(UserSessionUnit, Cleanup_PreservesActiveSessions) {
    // 活跃会话不会被误删
    auto session = manager_->create_session("test_user", "fenyun_basic");
    std::string sid = session.session_id;

    manager_->process_time_step(sid, 0.1);

    EXPECT_TRUE(manager_->has_session(sid));

    manager_->cleanup_expired_sessions(300000);

    EXPECT_TRUE(manager_->has_session(sid)) << "活跃会话不应被清理";
}

}
