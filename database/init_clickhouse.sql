-- ============================================================
-- 古代轒辒车结构防护仿真与滚石冲击分析系统 - ClickHouse初始化脚本
-- ============================================================

CREATE DATABASE IF NOT EXISTS fenyun_vehicle
    COMMENT '轒辒车仿真分析数据库'
    ENGINE = Ordinary;

USE fenyun_vehicle;

-- ============================================================
-- 传感器数据表：每辆车每1分钟上报一次
-- ============================================================
CREATE TABLE IF NOT EXISTS sensor_data (
    vehicle_id          UInt32          COMMENT '车辆ID',
    timestamp           DateTime64(3)   COMMENT '上报时间戳(毫秒)',
    roof_stress         Float64         COMMENT '顶棚应力(MPa)',
    wheel_deformation   Float64         COMMENT '车轮变形(mm)',
    rock_impact_force   Float64         COMMENT '滚石冲击力(kN)',
    protection_thickness Float64        COMMENT '防护层剩余厚度(mm)',
    protection_material String          COMMENT '防护材料类型(cowhide/wood/iron/composite)',
    ambient_temp        Float64         COMMENT '环境温度(℃)',
    impact_location_x   Float64         COMMENT '冲击点X坐标',
    impact_location_y   Float64         COMMENT '冲击点Y坐标',
    rock_mass           Float64         COMMENT '滚石质量(kg)',
    rock_velocity       Float64         COMMENT '滚石速度(m/s)'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, timestamp)
TTL timestamp + INTERVAL 1 YEAR
COMMENT '传感器实时数据表';

-- ============================================================
-- 结构仿真结果表
-- ============================================================
CREATE TABLE IF NOT EXISTS simulation_results (
    simulation_id       UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3),
    roof_max_deformation Float64        COMMENT '顶棚最大变形(mm)',
    roof_plastic_strain  Float64        COMMENT '顶棚塑性应变',
    roof_von_mises_stress Float64       COMMENT '顶棚Von Mises等效应力(MPa)',
    impact_energy       Float64         COMMENT '冲击能量(J)',
    absorbed_energy     Float64         COMMENT '吸收能量(J)',
    damage_level        UInt8           COMMENT '损伤等级(0-完好,1-轻微,2-中度,3-严重,4-破坏)',
    penetration_depth   Float64         COMMENT '侵彻深度(mm)',
    is_penetrated       UInt8           COMMENT '是否击穿(0-否,1-是)',
    failure_mode        String          COMMENT '破坏模式(bending/shear/punching/combined)',
    deformation_field   Array(Float64)  COMMENT '变形场数据(网格化)',
    stress_field        Array(Float64)  COMMENT '应力场数据(网格化)'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, simulation_id, timestamp)
COMMENT '结构仿真计算结果表';

-- ============================================================
-- 告警记录表
-- ============================================================
CREATE TABLE IF NOT EXISTS alert_records (
    alert_id            UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3),
    alert_type          String          COMMENT '告警类型(deformation_exceed/penetration/material_fatigue)',
    alert_level         UInt8           COMMENT '告警级别(1-提示,2-警告,3-严重,4-紧急)',
    alert_message       String,
    measured_value      Float64         COMMENT '测量值',
    threshold_value     Float64         COMMENT '阈值',
    is_acknowledged     UInt8 DEFAULT 0 COMMENT '是否确认',
    acknowledged_time   Nullable(DateTime64(3)),
    mqtt_topic          String,
    mqtt_message_id     String
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, alert_id, timestamp)
COMMENT '告警记录表';

-- ============================================================
-- 防护方案评估表（AHP层次分析法结果）
-- ============================================================
CREATE TABLE IF NOT EXISTS protection_evaluation (
    eval_id             UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3),
    material_type       String          COMMENT '材料类型',
    material_thickness  Float64         COMMENT '材料厚度(mm)',
    energy_absorption   Float64         COMMENT '吸能能力评分',
    structural_strength Float64         COMMENT '结构强度评分',
    weight_factor       Float64         COMMENT '重量因素评分',
    cost_factor         Float64         COMMENT '成本因素评分',
    durability          Float64         COMMENT '耐久性评分',
    ahp_weight_score    Float64         COMMENT 'AHP综合权重评分',
    rank_position       UInt8           COMMENT '排名',
    is_recommended      UInt8           COMMENT '是否推荐方案(0-否,1-是)',
    sensitivity_data    String          COMMENT '敏感性分析数据(JSON)'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, eval_id, timestamp)
COMMENT '防护方案AHP评估结果表';

-- ============================================================
-- 车辆状态汇总（使用AggregatingMergeTree）
-- ============================================================
CREATE TABLE IF NOT EXISTS vehicle_status_summary (
    vehicle_id          UInt32,
    event_date          Date,
    report_count        AggregateFunction(count, UInt64),
    avg_roof_stress     AggregateFunction(avg, Float64),
    max_roof_stress     AggregateFunction(max, Float64),
    avg_wheel_def       AggregateFunction(avg, Float64),
    max_wheel_def       AggregateFunction(max, Float64),
    avg_impact_force    AggregateFunction(avg, Float64),
    max_impact_force    AggregateFunction(max, Float64),
    alert_count         AggregateFunction(count, UInt64)
)
ENGINE = AggregatingMergeTree()
PARTITION BY event_date
ORDER BY (vehicle_id, event_date)
COMMENT '车辆状态日汇总表';

-- ============================================================
-- 创建Materialized View用于实时汇总
-- ============================================================
CREATE MATERIALIZED VIEW IF NOT EXISTS vehicle_status_mv
TO vehicle_status_summary
AS
SELECT
    vehicle_id,
    toDate(timestamp) AS event_date,
    countState() AS report_count,
    avgState(roof_stress) AS avg_roof_stress,
    maxState(roof_stress) AS max_roof_stress,
    avgState(wheel_deformation) AS avg_wheel_def,
    maxState(wheel_deformation) AS max_wheel_def,
    avgState(rock_impact_force) AS avg_impact_force,
    maxState(rock_impact_force) AS max_impact_force,
    countState() AS alert_count
FROM sensor_data
GROUP BY vehicle_id, toDate(timestamp);

-- ============================================================
-- 插入材料参数参考数据
-- ============================================================
CREATE TABLE IF NOT EXISTS material_properties (
    material_id         UInt8,
    material_name       String,
    density             Float64         COMMENT '密度(kg/m³)',
    youngs_modulus      Float64         COMMENT '弹性模量(GPa)',
    poisson_ratio       Float64         COMMENT '泊松比',
    yield_strength      Float64         COMMENT '屈服强度(MPa)',
    ultimate_strength   Float64         COMMENT '极限强度(MPa)',
    toughness           Float64         COMMENT '韧性(MJ/m³)',
    specific_energy_absorption Float64  COMMENT '比吸能(kJ/kg)',
    cost_per_unit       Float64         COMMENT '单位成本',
    historical_period   String          COMMENT '历史时期可用性'
)
ENGINE = ReplacingMergeTree()
ORDER BY material_id
COMMENT '材料属性参数表';

INSERT INTO material_properties VALUES
(1, 'cowhide',   860.0,  0.15,  0.40,  25.0,   60.0,   15.0,  80.0,   3.0, 'Spring and Autumn'),
(2, 'wood',      650.0, 10.0,   0.35,  60.0,  120.0,   10.0,  50.0,   1.0, 'Spring and Autumn'),
(3, 'iron',     7850.0, 206.0,  0.29, 235.0,  400.0,   80.0, 200.0,  10.0, 'Warring States'),
(4, 'composite', 900.0,  15.0,  0.33, 120.0,  250.0,   45.0, 300.0,   5.0, 'Spring and Autumn');

-- ============================================================
-- 创建测试车辆数据
-- ============================================================
CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id          UInt32,
    vehicle_name        String,
    build_date          Date,
    protection_config   String          COMMENT '防护配置(JSON)',
    dimensions          String          COMMENT '尺寸参数(JSON)',
    status              UInt8           COMMENT '状态(0-停用,1-在用,2-维护中)'
)
ENGINE = ReplacingMergeTree()
ORDER BY vehicle_id
COMMENT '轒辒车辆台账';

INSERT INTO vehicles VALUES
(1, '先锋一号', '2025-03-15', '{"layers":[{"material":"wood","thickness":80},{"material":"cowhide","thickness":20}]}', '{"length":6.5,"width":2.8,"height":3.2}', 1),
(2, '雷霆二号', '2025-04-20', '{"layers":[{"material":"wood","thickness":100},{"material":"iron","thickness":5}]}', '{"length":7.0,"width":3.0,"height":3.5}', 1),
(3, '攻坚三号', '2025-05-10', '{"layers":[{"material":"wood","thickness":60},{"material":"cowhide","thickness":30},{"material":"wood","thickness":40}]}', '{"length":6.8,"width":2.9,"height":3.3}', 1);
