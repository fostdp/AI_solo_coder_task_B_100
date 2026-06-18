#include <gtest/gtest.h>
#include "impact_simulator/impact_simulator.h"
#include "config/config_loader.h"
#include "common/data_types.h"
#include "test_helpers.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>

namespace fenyun {

class ImpactSimulatorUnit : public ::testing::Test {
protected:
    void SetUp() override {
        simulator_ = test::create_test_impact_simulator();
        ASSERT_NE(simulator_, nullptr);
        config_ = test::create_test_config_loader();
        ASSERT_NE(config_, nullptr);
    }

    void TearDown() override {
        simulator_->stop();
        simulator_.reset();
        config_.reset();
    }

    std::shared_ptr<ImpactSimulator> simulator_;
    std::shared_ptr<ConfigLoader> config_;
};

TEST_F(ImpactSimulatorUnit, BasicFunctionality_RunSimulation_ReturnsValidResult) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    SimulationResult result = simulator_->run_simulation(data);

    EXPECT_GT(result.simulation_id, 0u) << "simulation_id 应该大于 0";
    EXPECT_EQ(result.vehicle_id, 1u);
    EXPECT_GT(result.timestamp_ms, 0);
    EXPECT_GT(result.impact_energy_j, 0.0);
    EXPECT_GT(result.strain_rate, 0.0);
    EXPECT_GT(result.dynamic_yield_strength_mpa, 0.0);
    EXPECT_GT(result.roof_max_deformation_mm, 0.0);
    EXPECT_GE(result.damage_level, 0);
    EXPECT_FALSE(result.failure_mode.empty());
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_SubmitTask_TaskCompletes) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    auto task = simulator_->submit_plasticity_task(data);

    EXPECT_GT(task->task_id, 0u);
    EXPECT_FALSE(task->completed.load());

    int waited = 0;
    while (!task->completed.load() && waited < 5000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waited += 10;
    }

    EXPECT_TRUE(task->completed.load()) << "任务应该在 5 秒内完成";
    EXPECT_GT(task->result.simulation_id, 0u);
    EXPECT_GT(task->result.impact_energy_j, 0.0);
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_GetTaskResult_ReturnsValidResult) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    auto task = simulator_->submit_plasticity_task(data);
    uint64_t task_id = task->task_id;

    SimulationResult result = simulator_->get_task_result(task_id, 5000);

    EXPECT_GT(result.simulation_id, 0u);
    EXPECT_EQ(result.vehicle_id, 1u);
    EXPECT_GT(result.impact_energy_j, 0.0);
    EXPECT_GT(result.roof_max_deformation_mm, 0.0);
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_IsTaskComplete_WorksCorrectly) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    auto task = simulator_->submit_plasticity_task(data);
    uint64_t task_id = task->task_id;

    EXPECT_FALSE(simulator_->is_task_complete(task_id)) << "刚提交的任务不应该立即完成";

    SimulationResult result = simulator_->get_task_result(task_id, 5000);

    EXPECT_TRUE(simulator_->is_task_complete(task_id)) << "获取结果后任务应该已完成";
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_PendingTasks_DecreasesAfterCompletion) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    size_t initial_pending = simulator_->pending_tasks();

    auto task = simulator_->submit_plasticity_task(data);

    EXPECT_GE(simulator_->pending_tasks(), initial_pending) << "提交后等待任务数不应减少";

    simulator_->get_task_result(task->task_id, 5000);

    EXPECT_LE(simulator_->pending_tasks(), 1u) << "完成后等待任务数应该减少";
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_MultipleTasks_AllComplete) {
    const int NUM_TASKS = 5;
    std::vector<std::shared_ptr<ImpactSimulator::PlasticityTask>> tasks;

    for (int i = 0; i < NUM_TASKS; ++i) {
        SensorData data = test::create_mock_sensor_data(i + 1, "composite", 5.0 + i, 30.0 + i * 5);
        tasks.push_back(simulator_->submit_plasticity_task(data));
    }

    EXPECT_GE(simulator_->pending_tasks(), 1u) << "提交多个任务后应该有等待任务";

    for (auto& task : tasks) {
        SimulationResult result = simulator_->get_task_result(task->task_id, 10000);
        EXPECT_GT(result.simulation_id, 0u);
        EXPECT_TRUE(task->completed.load());
    }

    EXPECT_EQ(simulator_->pending_tasks(), 0u) << "所有任务完成后等待数应为 0";
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_CleanupCompletedTasks_RemovesCompleted) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    auto task = simulator_->submit_plasticity_task(data);
    simulator_->get_task_result(task->task_id, 5000);

    EXPECT_TRUE(simulator_->is_task_complete(task->task_id));

    simulator_->cleanup_completed_tasks();

    EXPECT_FALSE(simulator_->is_task_complete(task->task_id)) << "清理后已完成任务应该被移除";
}

TEST_F(ImpactSimulatorUnit, PlasticityCalc_InvalidTaskId_ReturnsEmptyResult) {
    SimulationResult result = simulator_->get_task_result(999999999, 100);

    EXPECT_EQ(result.simulation_id, 0u) << "无效任务 ID 应该返回空结果";
    EXPECT_FALSE(simulator_->is_task_complete(999999999)) << "无效任务 ID 不应该是完成状态";
}

TEST_F(ImpactSimulatorUnit, JohnsonCook_GetParams_ReturnsValidParams) {
    JohnsonCookParams jc = simulator_->get_jc_params("composite");

    EXPECT_GT(jc.A, 0.0) << "A 参数应该大于 0";
    EXPECT_GT(jc.B, 0.0) << "B 参数应该大于 0";
    EXPECT_GT(jc.n, 0.0) << "n 参数应该大于 0";
    EXPECT_GT(jc.C, 0.0) << "C 参数应该大于 0";
    EXPECT_GT(jc.m, 0.0) << "m 参数应该大于 0";
    EXPECT_GT(jc.eps_dot_0, 0.0) << "eps_dot_0 参数应该大于 0";
    EXPECT_GT(jc.T_melt, 0.0) << "T_melt 参数应该大于 0";
    EXPECT_GT(jc.T_ref, 0.0) << "T_ref 参数应该大于 0";
}

TEST_F(ImpactSimulatorUnit, DamageLevel_IncreasesWithImpactEnergy) {
    SensorData low_data = test::create_mock_sensor_data(1, "composite", 1.0, 10.0);
    SensorData high_data = test::create_mock_sensor_data(2, "composite", 10.0, 50.0);

    SimulationResult low_result = simulator_->run_simulation(low_data);
    SimulationResult high_result = simulator_->run_simulation(high_data);

    EXPECT_GE(high_result.damage_level, low_result.damage_level)
        << "高能量冲击的损伤等级应该不低于低能量冲击";
}

TEST_F(ImpactSimulatorUnit, DeformationField_HasCorrectSize) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    SimulationResult result = simulator_->run_simulation(data);

    EXPECT_GT(result.deformation_field.size(), 0u) << "变形场不应为空";
    EXPECT_GT(result.stress_field.size(), 0u) << "应力场不应为空";
    EXPECT_EQ(result.deformation_field.size(), result.stress_field.size())
        << "变形场和应力场大小应该相同";

    for (double val : result.deformation_field) {
        EXPECT_GE(val, 0.0) << "变形值不应为负";
    }

    for (double val : result.stress_field) {
        EXPECT_GE(val, 0.0) << "应力值不应为负";
    }
}

TEST_F(ImpactSimulatorUnit, SimulationStats_IncrementsCorrectly) {
    uint64_t initial = simulator_->simulations_run();

    SensorData data1 = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);
    simulator_->run_simulation(data1);
    EXPECT_EQ(simulator_->simulations_run(), initial + 1);

    SensorData data2 = test::create_mock_sensor_data(2, "composite", 3.0, 25.0);
    simulator_->run_simulation(data2);
    EXPECT_EQ(simulator_->simulations_run(), initial + 2);
}

TEST_F(ImpactSimulatorUnit, Temperature_AffectsFlowStress) {
    SensorData cold_data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);
    cold_data.ambient_temp = -40.0;

    SensorData hot_data = test::create_mock_sensor_data(2, "composite", 5.0, 30.0);
    hot_data.ambient_temp = 80.0;

    SimulationResult cold_result = simulator_->run_simulation(cold_data);
    SimulationResult hot_result = simulator_->run_simulation(hot_data);

    EXPECT_GT(cold_result.dynamic_yield_strength_mpa, hot_result.dynamic_yield_strength_mpa)
        << "低温下的动态屈服强度应该高于高温下";
}

TEST_F(ImpactSimulatorUnit, StrainRate_IncreasesWithVelocity) {
    SensorData slow_data = test::create_mock_sensor_data(1, "composite", 5.0, 10.0);
    SensorData fast_data = test::create_mock_sensor_data(2, "composite", 5.0, 100.0);

    SimulationResult slow_result = simulator_->run_simulation(slow_data);
    SimulationResult fast_result = simulator_->run_simulation(fast_data);

    EXPECT_GT(fast_result.strain_rate, slow_result.strain_rate)
        << "高速冲击的应变率应该更高";
}

TEST_F(ImpactSimulatorUnit, ThreadPool_RunsInSeparateThread) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 5.0, 30.0);

    auto task = simulator_->submit_plasticity_task(data);

    std::thread::id main_thread_id = std::this_thread::get_id();

    EXPECT_FALSE(task->completed.load()) << "任务不应该在提交时立即完成";

    simulator_->get_task_result(task->task_id, 5000);

    EXPECT_TRUE(task->completed.load()) << "任务应该已完成";
}

TEST_F(ImpactSimulatorUnit, EdgeCases_ZeroRockMass_DoesNotCrash) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 0.0, 30.0);

    EXPECT_NO_FATAL_FAILURE({
        SimulationResult result = simulator_->run_simulation(data);
        EXPECT_GT(result.simulation_id, 0u);
    });
}

TEST_F(ImpactSimulatorUnit, EdgeCases_VeryHighVelocity_DoesNotCrash) {
    SensorData data = test::create_mock_sensor_data(1, "composite", 10.0, 1000.0);

    EXPECT_NO_FATAL_FAILURE({
        SimulationResult result = simulator_->run_simulation(data);
        EXPECT_GT(result.simulation_id, 0u);
    });
}

TEST_F(ImpactSimulatorUnit, EdgeCases_InvalidMaterial_DoesNotCrash) {
    SensorData data = test::create_mock_sensor_data(1, "nonexistent_material", 5.0, 30.0);

    EXPECT_NO_FATAL_FAILURE({
        SimulationResult result = simulator_->run_simulation(data);
        EXPECT_GT(result.simulation_id, 0u);
    });
}

TEST_F(ImpactSimulatorUnit, StartStop_WorksCorrectly) {
    EXPECT_FALSE(simulator_->is_running());

    simulator_->start();
    EXPECT_TRUE(simulator_->is_running());

    simulator_->stop();
    EXPECT_FALSE(simulator_->is_running());
}

}
