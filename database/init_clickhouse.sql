-- ============================================================
-- 古代轒辒车结构防护仿真与滚石冲击分析系统 - ClickHouse初始化脚本
-- v1.2 - 工程化版本: 多级TTL + 降采样物化视图 + 数据保留策略
-- ============================================================

CREATE DATABASE IF NOT EXISTS fenyun_vehicle
    COMMENT '轒辒车仿真分析数据库'
    ENGINE = Atomic;

USE fenyun_vehicle;

-- ============================================================
-- 0. SETTINGS
-- ============================================================
SET allow_experimental_analyzer = 1;
SET merge_tree_enable_use_asynchronous_mutations = 1;

-- ============================================================
-- 1. 传感器原始数据表 (RAW, 保留7天)
-- ============================================================
CREATE TABLE IF NOT EXISTS sensor_data (
    vehicle_id          UInt32          COMMENT '车辆ID',
    timestamp           DateTime64(3, 'UTC')  COMMENT '上报时间戳(毫秒,UTC)',
    roof_stress         Float64         COMMENT '顶棚应力(MPa)',
    wheel_deformation   Float64         COMMENT '车轮变形(mm)',
    rock_impact_force   Float64         COMMENT '滚石冲击力(kN)',
    protection_thickness Float64        COMMENT '防护层剩余厚度(mm)',
    protection_material LowCardinality(String)  COMMENT '防护材料类型',
    ambient_temp        Float64         COMMENT '环境温度(℃)',
    impact_location_x   Float64         COMMENT '冲击点X坐标',
    impact_location_y   Float64         COMMENT '冲击点Y坐标',
    rock_mass           Float64         COMMENT '滚石质量(kg)',
    rock_velocity       Float64         COMMENT '滚石速度(m/s)',
    sensor_source       LowCardinality(String) DEFAULT 'simulator'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMMDD(timestamp)
ORDER BY (vehicle_id, timestamp)
PRIMARY KEY (vehicle_id, timestamp)
TTL timestamp + INTERVAL 7 DAY      -- 原始高频数据保留7天
DELETE WHERE 1 = 1
COMMENT '传感器原始高频数据表(60s上报, 保留7天)'
SETTINGS
    index_granularity = 8192,
    min_bytes_for_wide_part = '10M',
    max_parts_in_total = 10000,
    ttl_only_drop_parts = 1,
    merge_with_ttl_timeout = 3600;

-- ============================================================
-- 2. 传感器5分钟粒度降采样表 (保留30天)
-- ============================================================
CREATE TABLE IF NOT EXISTS sensor_data_5min (
    vehicle_id          UInt32,
    time_bucket         DateTime64(3, 'UTC'),
    samples             UInt32          COMMENT '5min内样本数',
    avg_roof_stress     Float64,
    max_roof_stress     Float64,
    p95_roof_stress     Float64,
    avg_wheel_def       Float64,
    max_wheel_def       Float64,
    avg_impact_force    Float64,
    max_impact_force    Float64,
    p99_impact_force    Float64,
    avg_prot_thickness  Float64,
    min_prot_thickness  Float64,
    avg_rock_mass       Float64,
    max_rock_mass       Float64,
    avg_rock_velocity   Float64,
    total_impact_energy Float64         COMMENT '总冲击能量=Σ0.5·m·v²'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(time_bucket)
ORDER BY (vehicle_id, time_bucket)
PRIMARY KEY (vehicle_id, time_bucket)
TTL time_bucket + INTERVAL 30 DAY
COMMENT '传感器5min降采样表(保留30天)'
SETTINGS
    index_granularity = 4096,
    ttl_only_drop_parts = 1;

-- 5min降采样物化视图 (从sensor_data到sensor_data_5min)
CREATE MATERIALIZED VIEW IF NOT EXISTS sensor_data_5min_mv
TO sensor_data_5min
AS
SELECT
    vehicle_id,
    toStartOfFiveMinute(timestamp) AS time_bucket,
    count() AS samples,
    avg(roof_stress)                AS avg_roof_stress,
    max(roof_stress)                AS max_roof_stress,
    quantile(0.95)(roof_stress)     AS p95_roof_stress,
    avg(wheel_deformation)          AS avg_wheel_def,
    max(wheel_deformation)          AS max_wheel_def,
    avg(rock_impact_force)          AS avg_impact_force,
    max(rock_impact_force)          AS max_impact_force,
    quantile(0.99)(rock_impact_force) AS p99_impact_force,
    avg(protection_thickness)       AS avg_prot_thickness,
    min(protection_thickness)       AS min_prot_thickness,
    avg(rock_mass)                  AS avg_rock_mass,
    max(rock_mass)                  AS max_rock_mass,
    avg(rock_velocity)              AS avg_rock_velocity,
    sum(0.5 * rock_mass * rock_velocity * rock_velocity) AS total_impact_energy
FROM sensor_data
GROUP BY vehicle_id, time_bucket;

-- ============================================================
-- 3. 传感器1小时粒度降采样表 (保留1年)
-- ============================================================
CREATE TABLE IF NOT EXISTS sensor_data_1h (
    vehicle_id          UInt32,
    time_bucket         DateTime64(3, 'UTC'),
    samples             UInt32,
    avg_roof_stress     Float64,
    max_roof_stress     Float64,
    p95_roof_stress     Float64,
    avg_wheel_def       Float64,
    max_wheel_def       Float64,
    avg_impact_force    Float64,
    max_impact_force    Float64,
    p99_impact_force    Float64,
    avg_prot_thickness  Float64,
    min_prot_thickness  Float64,
    impact_event_count  UInt32        COMMENT '冲击事件(>20kN)次数',
    peak_energy_event   Float64       COMMENT '最大单事件能量(J)'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(time_bucket)
ORDER BY (vehicle_id, time_bucket)
PRIMARY KEY (vehicle_id, time_bucket)
TTL time_bucket + INTERVAL 1 YEAR
COMMENT '传感器1h降采样表(保留1年)'
SETTINGS
    index_granularity = 4096,
    ttl_only_drop_parts = 1;

-- 1h降采样 (从5min表聚合)
CREATE MATERIALIZED VIEW IF NOT EXISTS sensor_data_1h_mv
TO sensor_data_1h
AS
SELECT
    vehicle_id,
    toStartOfHour(time_bucket) AS time_bucket,
    sum(samples) AS samples,
    avg(avg_roof_stress)              AS avg_roof_stress,
    max(max_roof_stress)              AS max_roof_stress,
    max(p95_roof_stress)              AS p95_roof_stress,
    avg(avg_wheel_def)                AS avg_wheel_def,
    max(max_wheel_def)                AS max_wheel_def,
    avg(avg_impact_force)             AS avg_impact_force,
    max(max_impact_force)             AS max_impact_force,
    max(p99_impact_force)             AS p99_impact_force,
    avg(avg_prot_thickness)           AS avg_prot_thickness,
    min(min_prot_thickness)           AS min_prot_thickness,
    countIf(max_impact_force > 20)    AS impact_event_count,
    max(total_impact_energy)          AS peak_energy_event
FROM sensor_data_5min
GROUP BY vehicle_id, time_bucket;

-- ============================================================
-- 4. 结构仿真结果表 (保留90天)
-- ============================================================
CREATE TABLE IF NOT EXISTS simulation_results (
    simulation_id       UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3, 'UTC'),
    roof_max_deformation Float64        COMMENT '顶棚最大变形(mm)',
    roof_plastic_strain  Float64        COMMENT '顶棚塑性应变',
    roof_von_mises_stress Float64       COMMENT '顶棚Von Mises等效应力(MPa)',
    impact_energy       Float64         COMMENT '冲击能量(J)',
    absorbed_energy     Float64         COMMENT '吸收能量(J)',
    damage_level        UInt8           COMMENT '损伤等级0-4',
    penetration_depth   Float64         COMMENT '侵彻深度(mm)',
    is_penetrated       UInt8           COMMENT '是否击穿',
    failure_mode        LowCardinality(String) COMMENT '破坏模式',
    strain_rate         Float64         COMMENT '等效平均应变率(1/s)',
    jc_yield_stress_mpa Float64        COMMENT 'JC动态屈服强度(MPa)',
    deformation_field   Array(Float32)  COMMENT '10x10变形场(mm)',
    stress_field        Array(Float32)  COMMENT '10x10应力场(MPa)'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, simulation_id, timestamp)
PRIMARY KEY (vehicle_id, simulation_id)
TTL timestamp + INTERVAL 90 DAY
COMMENT 'JC结构仿真结果表(保留90天, 高频云图数据较短保留)'
SETTINGS
    index_granularity = 2048,
    ttl_only_drop_parts = 1,
    min_bytes_for_wide_part = '50M';

-- ============================================================
-- 5. 仿真结果1h聚合表 (保留1年)
-- ============================================================
CREATE TABLE IF NOT EXISTS simulation_results_1h (
    vehicle_id          UInt32,
    time_bucket         DateTime64(3, 'UTC'),
    sim_count           UInt32,
    avg_deformation     Float64,
    max_deformation     Float64,
    p95_deformation     Float64,
    avg_stress          Float64,
    max_stress          Float64,
    avg_absorbed_ratio  Float64         COMMENT '平均吸能比=吸收/冲击',
    damage_distribution Array(UInt32)   COMMENT '[L0,L1,L2,L3,L4]各级损伤数量',
    penetrations        UInt32          COMMENT '击穿次数',
    fatigue_warning_cnt UInt32          COMMENT '应力>0.8σ_y次数'
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(time_bucket)
ORDER BY (vehicle_id, time_bucket)
PRIMARY KEY (vehicle_id, time_bucket)
TTL time_bucket + INTERVAL 1 YEAR
COMMENT '仿真结果1h聚合(保留1年)';

-- ============================================================
-- 6. 告警记录表 (保留2年)
-- ============================================================
CREATE TABLE IF NOT EXISTS alert_records (
    alert_id            UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3, 'UTC'),
    alert_type          LowCardinality(String)  COMMENT 'deformation/penetration/stress/fatigue',
    alert_level         UInt8           COMMENT '1-提示 2-警告 3-严重 4-紧急',
    alert_message       String,
    measured_value      Float64,
    threshold_value     Float64,
    is_acknowledged     UInt8 DEFAULT 0,
    acknowledged_time   Nullable(DateTime64(3, 'UTC')),
    acknowledged_by     LowCardinality(String) DEFAULT '',
    mqtt_topic          String,
    mqtt_message_id     String
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, alert_level, alert_id, timestamp)
PRIMARY KEY (vehicle_id, alert_level, alert_id)
TTL timestamp + INTERVAL 2 YEAR
COMMENT '告警记录表(保留2年用于审计)'
SETTINGS
    index_granularity = 4096,
    ttl_only_drop_parts = 1;

-- ============================================================
-- 7. 告警日汇总 (永久保留)
-- ============================================================
CREATE TABLE IF NOT EXISTS alert_daily_summary (
    vehicle_id          UInt32,
    event_date          Date,
    level1_cnt          UInt32,
    level2_cnt          UInt32,
    level3_cnt          UInt32,
    level4_cnt          UInt32,
    deformation_cnt     UInt32,
    penetration_cnt     UInt32,
    stress_cnt          UInt32,
    fatigue_cnt         UInt32,
    ack_rate_pct        Float32         COMMENT '告警确认率%'
)
ENGINE = SummingMergeTree()
PARTITION BY toYYYYMM(event_date)
ORDER BY (vehicle_id, event_date)
PRIMARY KEY (vehicle_id, event_date)
COMMENT '告警日汇总(永久保留, SummingMergeTree自动求和)';

-- ============================================================
-- 8. 防护方案AHP评估表 (永久保留)
-- ============================================================
CREATE TABLE IF NOT EXISTS protection_evaluation (
    eval_id             UInt64,
    vehicle_id          UInt32,
    timestamp           DateTime64(3, 'UTC'),
    trigger_context     LowCardinality(String) DEFAULT 'manual'  COMMENT 'manual/scheduled/alert',
    material_type       LowCardinality(String),
    material_thickness  Float64         COMMENT 'mm',
    energy_absorption   Float32,
    structural_strength Float32,
    weight_factor       Float32,
    cost_factor         Float32,
    durability          Float32,
    ahp_weight_score    Float32         COMMENT '0~1综合评分',
    rank_position       UInt8,
    is_recommended      UInt8,
    cr_value            Float32         COMMENT '本次评估一致性比率',
    expert_count        UInt8           COMMENT '参与评估专家数',
    consensus_index     Float32         COMMENT '群决策共识指数(0~1)',
    sensitivity_data    String          COMMENT '敏感性分析(JSON)'
)
ENGINE = ReplacingMergeTree(timestamp)
PARTITION BY toYYYYMM(timestamp)
ORDER BY (vehicle_id, eval_id)
PRIMARY KEY (vehicle_id, eval_id)
COMMENT 'AHP评估结果(永久保留, ReplacingMergeTree去重)';

-- ============================================================
-- 9. 车辆状态日汇总 (AggregatingMergeTree, 永久保留)
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
    alert_count         AggregateFunction(count, UInt64),
    damage_level_sum    AggregateFunction(sum, UInt64),
    energy_joules_sum   AggregateFunction(sum, Float64)
)
ENGINE = AggregatingMergeTree()
PARTITION BY event_date
ORDER BY (vehicle_id, event_date)
PRIMARY KEY (vehicle_id, event_date)
COMMENT '车辆状态日汇总(AggregatingMergeTree, 永久保留)';

CREATE MATERIALIZED VIEW IF NOT EXISTS vehicle_status_mv
TO vehicle_status_summary
AS
SELECT
    vehicle_id,
    toDate(timestamp) AS event_date,
    countState()                                     AS report_count,
    avgState(roof_stress)                            AS avg_roof_stress,
    maxState(roof_stress)                            AS max_roof_stress,
    avgState(wheel_deformation)                      AS avg_wheel_def,
    maxState(wheel_deformation)                      AS max_wheel_def,
    avgState(rock_impact_force)                      AS avg_impact_force,
    maxState(rock_impact_force)                      AS max_impact_force,
    countState()                                     AS alert_count,
    sumState(toUInt64(0))                            AS damage_level_sum,
    sumState(0.5 * rock_mass * rock_velocity * rock_velocity) AS energy_joules_sum
FROM sensor_data
GROUP BY vehicle_id, toDate(timestamp);

-- ============================================================
-- 10. 材料参数参考表 (ReplacingMergeTree)
-- ============================================================
CREATE TABLE IF NOT EXISTS material_properties (
    material_id         UInt8,
    material_type       LowCardinality(String),
    density             Float64         COMMENT 'kg/m³',
    youngs_modulus_gpa  Float64         COMMENT 'GPa',
    poisson_ratio       Float64,
    yield_strength_mpa  Float64,
    ultimate_strength_mpa Float64,
    toughness_mj_m3     Float64,
    sea_kj_kg           Float64         COMMENT 'Specific Energy Absorption kJ/kg',
    cost_per_kg         Float64,
    durability_score    Float32         COMMENT '0~10耐久性评分',
    historical_period   LowCardinality(String),
    jc_params           String          COMMENT 'Johnson-Cook参数(JSON: A,B,n,C,m,Tm,Tr,eps0)'
)
ENGINE = ReplacingMergeTree()
ORDER BY material_id
PRIMARY KEY material_id
COMMENT '材料参数表(含Johnson-Cook)';

INSERT INTO material_properties VALUES
(1, 'cowhide',    860.0,  0.15, 0.40,  25.0,  60.0,  15.0,  80.0,   3.0, 7.5,  'Spring and Autumn', '{"A":25e6,"B":40e6,"n":0.35,"C":0.04,"m":0.8,"T_melt":573,"T_ref":293,"eps_dot_0":1.0}'),
(2, 'wood',       650.0, 10.00, 0.35,  60.0, 120.0,  10.0,  50.0,   1.0, 6.0,  'Spring and Autumn', '{"A":60e6,"B":120e6,"n":0.45,"C":0.06,"m":1.0,"T_melt":600,"T_ref":293,"eps_dot_0":1.0}'),
(3, 'iron',      7850.0, 206.0, 0.29, 235.0, 400.0,  80.0, 200.0,  10.0, 9.0,  'Warring States',    '{"A":235e6,"B":275e6,"n":0.36,"C":0.022,"m":1.03,"T_melt":1811,"T_ref":293,"eps_dot_0":1.0}'),
(4, 'composite',  900.0,  15.0, 0.33, 120.0, 250.0,  45.0, 300.0,   5.0, 8.5,  'Spring and Autumn', '{"A":120e6,"B":180e6,"n":0.42,"C":0.05,"m":0.9,"T_melt":700,"T_ref":293,"eps_dot_0":1.0}');

-- ============================================================
-- 11. 车辆台账
-- ============================================================
CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id          UInt32,
    vehicle_name        String,
    build_date          Date,
    protection_config   String          COMMENT 'JSON 材料层配置',
    dimensions          String          COMMENT 'JSON 尺寸参数',
    default_material    LowCardinality(String),
    protection_mm       Float32,
    status              UInt8           COMMENT '0停用 1在用 2维护 3报废'
)
ENGINE = ReplacingMergeTree()
ORDER BY vehicle_id
PRIMARY KEY vehicle_id
COMMENT '轒辒车辆台账';

INSERT INTO vehicles VALUES
(1, '先锋一号', '2025-03-15', '{"layers":[{"material":"wood","thickness":80},{"material":"cowhide","thickness":20}]}', '{"length":6.5,"width":2.8,"height":3.2}', 'wood',      100.0, 1),
(2, '雷霆二号', '2025-04-20', '{"layers":[{"material":"wood","thickness":100},{"material":"iron","thickness":5}]}',   '{"length":7.0,"width":3.0,"height":3.5}', 'composite', 105.0, 1),
(3, '攻坚三号', '2025-05-10', '{"layers":[{"material":"wood","thickness":60},{"material":"cowhide","thickness":30},{"material":"wood","thickness":40}]}', '{"length":6.8,"width":2.9,"height":3.3}', 'composite', 130.0, 1);

-- ============================================================
-- 12. 字典表 - 告警级别/破坏模式 等
-- ============================================================
CREATE TABLE IF NOT EXISTS dict_alert_levels (
    level       UInt8,
    code        String,
    name_zh     String,
    color_hex   String,
    priority    UInt8
)
ENGINE = TinyLog();

INSERT INTO dict_alert_levels VALUES
(1, 'INFO',    '提示',   '#4488ff', 4),
(2, 'WARNING', '警告',   '#ffaa00', 3),
(3, 'SEVERE',  '严重',   '#ff5522', 2),
(4, 'CRITICAL','紧急',   '#ff0066', 1);

CREATE TABLE IF NOT EXISTS dict_failure_modes (
    mode_code   LowCardinality(String),
    name_zh     String,
    description String
)
ENGINE = TinyLog();

INSERT INTO dict_failure_modes VALUES
('bending',   '弯曲破坏',   '顶棚薄板在冲击弯矩下发生塑性屈服'),
('shear',     '剪切破坏',   '沿冲击边界发生剪切开裂'),
('punching',  '冲切破坏',   '局部侵彻后整体剥落'),
('combined',  '组合破坏',   '弯+剪+冲切耦合破坏');

-- ============================================================
-- 13. 运维表 - 系统运行健康度
-- ============================================================
CREATE TABLE IF NOT EXISTS system_health_metrics (
    component       LowCardinality(String),
    timestamp       DateTime64(3, 'UTC'),
    metric_name     LowCardinality(String),
    metric_value    Float64,
    labels          Map(String, String)
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (component, metric_name, timestamp)
TTL timestamp + INTERVAL 15 DAY
COMMENT '系统组件健康指标(保留15天)';

-- ============================================================
-- 14. 查询性能优化 - 投影(Projections)
-- ============================================================
ALTER TABLE sensor_data ADD PROJECTION IF NOT EXISTS proj_by_material
(
    SELECT * ORDER BY protection_material, vehicle_id, timestamp
);

ALTER TABLE alert_records ADD PROJECTION IF NOT EXISTS proj_by_level_type
(
    SELECT alert_id, vehicle_id, timestamp, alert_type, alert_level, measured_value, threshold_value
    ORDER BY alert_level, alert_type, timestamp
);

-- ============================================================
-- 15. 车辆防护能力对比结果表 (永久保留)
-- ============================================================
CREATE TABLE IF NOT EXISTS vehicle_comparison_results (
    comparison_id       UInt64,
    timestamp           DateTime64(3, 'UTC'),
    rock_mass_kg        Float64,
    rock_velocity_ms    Float64,
    impact_location_x   Float64,
    impact_location_y   Float64,
    temperature_K       Float64,
    use_johnson_cook    UInt8,
    best_vehicle_id     LowCardinality(String),
    comparison_type     LowCardinality(String)  COMMENT 'custom/ancient/cross_era',
    insights            Array(String)
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (comparison_id, timestamp)
PRIMARY KEY comparison_id
TTL timestamp + INTERVAL 2 YEAR
COMMENT '车辆防护能力对比分析结果(保留2年)'
SETTINGS index_granularity = 1024;

CREATE TABLE IF NOT EXISTS vehicle_comparison_items (
    comparison_id       UInt64,
    vehicle_id          LowCardinality(String),
    display_name        String,
    era                 LowCardinality(String)  COMMENT 'ancient/modern',
    roof_max_deformation_mm   Float64,
    roof_plastic_strain       Float64,
    roof_von_mises_stress_mpa Float64,
    impact_energy_j           Float64,
    absorbed_energy_j         Float64,
    damage_level              UInt8,
    penetration_depth_mm      Float64,
    is_penetrated             UInt8,
    failure_mode              LowCardinality(String),
    protection_efficiency_score  Float64,
    weight_normalized_score      Float64,
    cost_normalized_score        Float64,
    overall_score                Float64,
    rank_position                UInt8
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(toDateTime(0))
ORDER BY (comparison_id, rank_position)
PRIMARY KEY (comparison_id, rank_position)
COMMENT '车辆对比明细项'
SETTINGS index_granularity = 1024;

-- ============================================================
-- 16. 队形优化结果表 (永久保留)
-- ============================================================
CREATE TABLE IF NOT EXISTS formation_optimization_results (
    optimization_id     UInt64,
    timestamp           DateTime64(3, 'UTC'),
    vehicle_count       UInt32,
    wall_height_m       Float64,
    wall_length_m       Float64,
    rock_fall_rate      Float64,
    avg_rock_mass_kg    Float64,
    best_formation_type LowCardinality(String),
    best_spacing_m      Float64,
    best_attack_width_m Float64,
    survival_probability  Float64,
    avg_coverage_score    Float64,
    total_progress_rate   Float64,
    recommendations       Array(String)
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (optimization_id, timestamp)
PRIMARY KEY optimization_id
TTL timestamp + INTERVAL 2 YEAR
COMMENT '队形优化分析结果(保留2年)'
SETTINGS index_granularity = 1024;

CREATE TABLE IF NOT EXISTS formation_vehicle_layouts (
    optimization_id     UInt64,
    formation_type      LowCardinality(String),
    vehicle_index       UInt32,
    vehicle_type        LowCardinality(String),
    position_x          Float64,
    position_y          Float64,
    heading_deg         Float64,
    spacing_m           Float64,
    is_lead             UInt8
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(toDateTime(0))
ORDER BY (optimization_id, formation_type, vehicle_index)
PRIMARY KEY (optimization_id, formation_type, vehicle_index)
COMMENT '队形车辆布局明细'
SETTINGS index_granularity = 2048;

-- ============================================================
-- 17. 虚拟驾驶用户会话与状态表 (保留30天)
-- ============================================================
CREATE TABLE IF NOT EXISTS user_sessions (
    session_id          String,
    created_at          DateTime64(3, 'UTC'),
    last_active_at      DateTime64(3, 'UTC'),
    user_nickname       String,
    vehicle_type        LowCardinality(String),
    total_distance_m    Float64 DEFAULT 0,
    total_impacts       UInt32 DEFAULT 0,
    total_damage        Float64 DEFAULT 0,
    max_health          Float64 DEFAULT 100.0,
    min_health          Float64 DEFAULT 100.0,
    status              LowCardinality(String) DEFAULT 'active' COMMENT 'active/finished/destroyed'
)
ENGINE = ReplacingMergeTree(last_active_at)
PARTITION BY toYYYYMM(created_at)
ORDER BY (session_id, created_at)
PRIMARY KEY session_id
TTL created_at + INTERVAL 30 DAY
COMMENT '虚拟驾驶用户会话(保留30天)'
SETTINGS index_granularity = 256;

CREATE TABLE IF NOT EXISTS user_vehicle_state_log (
    session_id          String,
    timestamp           DateTime64(3, 'UTC'),
    position_x          Float64,
    position_y          Float64,
    heading_deg         Float64,
    speed_ms            Float64,
    health_percent      Float64,
    armor_integrity_percent Float64,
    impacts_received    UInt32,
    distance_traveled_m Float64
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (session_id, timestamp)
TTL timestamp + INTERVAL 7 DAY
COMMENT '用户车辆状态轨迹日志(保留7天)'
SETTINGS index_granularity = 4096;

CREATE TABLE IF NOT EXISTS user_rock_attack_log (
    event_id            UInt64,
    session_id          String,
    timestamp           DateTime64(3, 'UTC'),
    impact_x            Float64,
    impact_y            Float64,
    rock_mass_kg        Float64,
    rock_velocity_ms    Float64,
    damage_dealt        Float64,
    is_manual_trigger   UInt8 DEFAULT 0,
    force_multiplier    Float64 DEFAULT 1.0
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (session_id, event_id, timestamp)
TTL timestamp + INTERVAL 15 DAY
COMMENT '用户滚石攻击事件日志(保留15天)'
SETTINGS index_granularity = 4096;

-- ============================================================
-- 18. 新功能字典表
-- ============================================================
CREATE TABLE IF NOT EXISTS dict_vehicle_eras (
    era_code    LowCardinality(String),
    name_zh     String,
    description String
)
ENGINE = TinyLog();

INSERT INTO dict_vehicle_eras VALUES
('ancient', '古代',   '冷兵器时代攻城器械, 使用木材、牛皮、铸铁等原始材料'),
('modern',  '现代',   '工业时代后装甲车辆, 使用钢、铝合金、复合材料、贫铀等');

CREATE TABLE IF NOT EXISTS dict_vehicle_types (
    type_code   LowCardinality(String),
    name_zh     String,
    era         LowCardinality(String),
    description String
)
ENGINE = TinyLog();

INSERT INTO dict_vehicle_types VALUES
('FENYUN',      '轒辒车',   'ancient', '掩护型攻城车, 顶部防护优秀, 用于推进至城下'),
('CHONGCHE',    '冲车',     'ancient', '重型突击车, 含撞城槌和楼车两种子类'),
('YUNTI',       '云梯',     'ancient', '登城型车辆, 防护较弱但机动性强'),
('MODERN_APC',  '装甲运兵车', 'modern',  '人员输送型, 防护适中, 机动性强'),
('MODERN_IFV',  '步兵战车',   'modern',  '火力支援型, 复合装甲, 可与步兵协同'),
('MODERN_TANK', '主战坦克',   'modern',  '重型突击型, 装甲极强, 火力凶猛');

CREATE TABLE IF NOT EXISTS dict_formation_types (
    type_code   LowCardinality(String),
    name_zh     String,
    description String,
    survival_weight Float64,
    coverage_weight Float64,
    progress_weight Float64
)
ENGINE = TinyLog();

INSERT INTO dict_formation_types VALUES
('LINE',     '横排',   '多车并列推进, 覆盖面广但速度最慢',    0.55, 1.00, 0.60),
('WEDGE',    '楔形',   '尖刀阵型, 推进最快但前锋伤亡大',      0.65, 0.55, 1.00),
('ECHELON',  '梯形',   '平衡型, 兼顾覆盖和推进效率',          0.75, 0.80, 0.80),
('V_SHAPE',  'V形',    '保护中路, 两翼前出分散火力',          0.85, 0.70, 0.75),
('COLUMN',   '纵队',   '纵深排列, 集中突破但覆盖面小',        0.70, 0.45, 0.85),
('DIAMOND',  '菱形',   '全方位防护, 适合混战和多路防御',      0.90, 0.65, 0.65);

-- ============================================================
-- 完成信息
-- ============================================================
SELECT 'ClickHouse初始化脚本执行完成 - v1.3 (多级TTL + 降采样 + 车辆对比 + 队形优化 + 虚拟驾驶)' AS status;
