#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
轒辒车传感器模拟器  v1.2  (工程化版本)
====================================================================
模拟春秋时期轒辒车攻城场景下，每辆车每分钟通过模拟传感器上报数据。

新增特性：
    * 可配置滚石质量范围  (--rock-mass-min / --rock-mass-max)
    * 可配置滚石落下高度  (--drop-height-min / --drop-height-max)
    * 可配置滚石直径      (--rock-diameter)
    * 可配置冲击频率分布  (--impact-profile: light/normal/heavy/barrage)
    * 可配置防护材料磨损系数
    * 支持环境变量 + YAML/JSON 配置文件
    * 上报数据包含 rock_diameter_mm / drop_height_m 字段
"""

import json
import os
import time
import random
import math
import threading
import argparse
import sys
from datetime import datetime, timezone
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError
from dataclasses import dataclass, field, asdict
from typing import Tuple


MATERIAL_PROPERTIES = {
    "cowhide": {
        "density": 860.0, "youngs_modulus_gpa": 0.15, "poisson_ratio": 0.40,
        "yield_strength_mpa": 25.0, "ultimate_strength_mpa": 60.0,
        "toughness_mj_m3": 15.0, "sea_kj_kg": 80.0,
        "base_thickness_mm": 20.0, "wear_coeff": 0.55,
        "jc": {"A": 25e6, "B": 40e6, "n": 0.35, "C": 0.04, "m": 0.8}
    },
    "wood": {
        "density": 650.0, "youngs_modulus_gpa": 10.0, "poisson_ratio": 0.35,
        "yield_strength_mpa": 60.0, "ultimate_strength_mpa": 120.0,
        "toughness_mj_m3": 10.0, "sea_kj_kg": 50.0,
        "base_thickness_mm": 80.0, "wear_coeff": 0.30,
        "jc": {"A": 60e6, "B": 120e6, "n": 0.45, "C": 0.06, "m": 1.0}
    },
    "iron": {
        "density": 7850.0, "youngs_modulus_gpa": 206.0, "poisson_ratio": 0.29,
        "yield_strength_mpa": 235.0, "ultimate_strength_mpa": 400.0,
        "toughness_mj_m3": 80.0, "sea_kj_kg": 200.0,
        "base_thickness_mm": 5.0, "wear_coeff": 0.08,
        "jc": {"A": 235e6, "B": 275e6, "n": 0.36, "C": 0.022, "m": 1.03}
    },
    "composite": {
        "density": 900.0, "youngs_modulus_gpa": 15.0, "poisson_ratio": 0.33,
        "yield_strength_mpa": 120.0, "ultimate_strength_mpa": 250.0,
        "toughness_mj_m3": 45.0, "sea_kj_kg": 300.0,
        "base_thickness_mm": 50.0, "wear_coeff": 0.22,
        "jc": {"A": 120e6, "B": 180e6, "n": 0.42, "C": 0.05, "m": 0.9}
    }
}

VEHICLE_CONFIGS = {
    1: {"name": "先锋一号", "material": "wood",      "thickness": 80.0,  "length": 6.5, "width": 2.8},
    2: {"name": "雷霆二号", "material": "iron",      "thickness": 105.0, "length": 7.0, "width": 3.0},
    3: {"name": "攻坚三号", "material": "composite", "thickness": 130.0, "length": 6.8, "width": 2.9},
    4: {"name": "破阵四号", "material": "cowhide",   "thickness": 40.0,  "length": 6.0, "width": 2.6},
    5: {"name": "登城五号", "material": "composite", "thickness": 160.0, "length": 7.5, "width": 3.2},
}

# 冲击场景预设
IMPACT_PROFILES = {
    # (质量范围kg, 高度范围m, 直径范围cm, 非冲击概率no_impact_prob)
    "light":   {"mass": (10, 50),   "height": (3, 10),   "diameter": (8, 20),  "no_impact": 0.25},
    "normal":  {"mass": (20, 120),  "height": (5, 20),   "diameter": (12, 35), "no_impact": 0.10},
    "heavy":   {"mass": (50, 250),  "height": (10, 30),  "diameter": (25, 60), "no_impact": 0.0},
    "barrage": {"mass": (30, 180),  "height": (15, 40),  "diameter": (20, 50), "no_impact": 0.0},
    "sniper":  {"mass": (80, 200),  "height": (25, 50),  "diameter": (30, 45), "no_impact": 0.40},
}

# 滚石密度 (kg/m³) - 花岗岩约 2600, 石灰岩约 2400
ROCK_DENSITY_KG_M3 = 2550.0
GRAVITY = 9.81


@dataclass
class RockSpec:
    mass_min_kg: float = 20.0
    mass_max_kg: float = 200.0
    height_min_m: float = 5.0
    height_max_m: float = 25.0
    diameter_min_cm: float = 10.0
    diameter_max_cm: float = 50.0
    no_impact_prob: float = 0.10

    @staticmethod
    def from_profile(name: str):
        p = IMPACT_PROFILES.get(name, IMPACT_PROFILES["normal"])
        return RockSpec(
            mass_min_kg=p["mass"][0], mass_max_kg=p["mass"][1],
            height_min_m=p["height"][0], height_max_m=p["height"][1],
            diameter_min_cm=p["diameter"][0], diameter_max_cm=p["diameter"][1],
            no_impact_prob=p["no_impact"],
        )


@dataclass
class VehicleSimOptions:
    vehicle_id: int
    api_url: str = "http://127.0.0.1:8080"
    rock: RockSpec = field(default_factory=RockSpec)
    wear_scale: float = 1.0          # 磨损倍率 (0.0 = 永不磨损, 2.0 = 磨损加倍)
    temp_base: float = 20.0          # 基础环境温度
    fixed_drop_height_m: float = 0.0 # >0 则固定冲击高度
    fixed_rock_mass_kg: float = 0.0  # >0 则固定滚石质量
    fixed_rock_diameter_cm: float = 0.0  # >0 则固定滚石直径


class VehicleSimulator:
    def __init__(self, opts: VehicleSimOptions):
        self.opts = opts
        self.vehicle_id = opts.vehicle_id
        self.api_url = opts.api_url
        cfg = VEHICLE_CONFIGS.get(opts.vehicle_id, VEHICLE_CONFIGS[1])
        self.name = cfg["name"]
        self.material = cfg["material"]
        self.initial_thickness = cfg["thickness"]
        self.current_thickness = self.initial_thickness
        self.length = cfg["length"]
        self.width = cfg["width"]
        self.ambient_temp = opts.temp_base + random.uniform(-5, 5)
        self.running = False
        self.cumulative_damage = 0.0
        self.impact_count = 0
        self.no_impact_count = 0
        self.lock = threading.Lock()

    # ------------------------------------------------------------------
    # 物理模型
    # ------------------------------------------------------------------
    @staticmethod
    def _rock_diameter_from_mass(mass_kg: float) -> float:
        """质量 -> 等效球体直径 (cm)"""
        volume_m3 = mass_kg / ROCK_DENSITY_KG_M3
        radius_m = ((3 * volume_m3) / (4 * math.pi)) ** (1.0 / 3.0)
        return radius_m * 2 * 100.0  # m -> cm

    @staticmethod
    def _rock_mass_from_diameter(diameter_cm: float) -> float:
        """直径(cm) -> 等效球体质量(kg)"""
        radius_m = (diameter_cm / 2.0) / 100.0
        volume_m3 = (4.0 / 3.0) * math.pi * (radius_m ** 3)
        return volume_m3 * ROCK_DENSITY_KG_M3

    def _pick_rock_params(self) -> Tuple[float, float, float]:
        """
        决定本次冲击参数：(质量kg, 高度m, 直径cm)
        用户设置了 fixed 参数则使用用户值；否则在范围内随机；
        三者不一致时取质量为锚，修正直径。
        """
        rock = self.opts.rock
        if self.opts.fixed_rock_mass_kg > 0:
            mass = self.opts.fixed_rock_mass_kg
        else:
            mass = random.uniform(rock.mass_min_kg, rock.mass_max_kg)

        if self.opts.fixed_drop_height_m > 0:
            height = self.opts.fixed_drop_height_m
        else:
            height = random.uniform(rock.height_min_m, rock.height_max_m)

        if self.opts.fixed_rock_diameter_cm > 0:
            diameter_cm = self.opts.fixed_rock_diameter_cm
            mass = self._rock_mass_from_diameter(diameter_cm)
        else:
            diameter_cm = random.uniform(rock.diameter_min_cm, rock.diameter_max_cm)
            # 让直径和质量统计上一致（取二者主导）
            mass_from_d = self._rock_mass_from_diameter(diameter_cm)
            # 让质量在范围和直径推导值之间取一个折中
            mass = 0.6 * mass + 0.4 * mass_from_d
            mass = max(rock.mass_min_kg, min(mass, rock.mass_max_kg))

        return mass, height, diameter_cm

    def generate_rock_impact(self):
        # 本轮是否真的有冲击
        if random.random() < self.opts.rock.no_impact_prob:
            self.no_impact_count += 1
            # 输出一个微小的"环境振动"记录，避免全零
            return {
                "rock_mass": 0.0,
                "rock_diameter_mm": 0.0,
                "drop_height_m": 0.0,
                "rock_velocity": 0.0,
                "rock_impact_force": round(random.uniform(0.1, 0.8), 2),
                "roof_stress": round(random.uniform(2, 8), 2),
                "impact_location_x": round(self.length / 2, 3),
                "impact_location_y": round(self.width / 2, 3),
                "impact_energy_j": 0.0,
                "has_impact": False,
            }

        mass_kg, height_m, diameter_cm = self._pick_rock_params()
        # v = sqrt(2gh)
        velocity = math.sqrt(2 * GRAVITY * height_m)
        radius_m = (diameter_cm / 2.0) / 100.0
        contact_area_m2 = math.pi * radius_m * radius_m * 0.6  # 60%有效接触
        impact_energy_j = 0.5 * mass_kg * velocity * velocity

        # 接触时间 Hertz 接触模型粗略估计 (ms级)
        contact_time_s = 0.002 + (mass_kg ** 0.33) * 0.0008 + random.uniform(0, 0.003)
        impulse = math.sqrt(2 * impact_energy_j * mass_kg)
        impact_force_kN = (impulse / contact_time_s) / 1000.0

        mat = MATERIAL_PROPERTIES[self.material]
        sigma_y = mat["yield_strength_mpa"]
        sigma_u = mat["ultimate_strength_mpa"]
        # 应力 = 力/面积 + 随机波动，但不超过 1.5σ_u
        try:
            stress_mpa = impact_force_kN * 1000 / max(contact_area_m2 * 1e6, 1e-3)
        except Exception:
            stress_mpa = sigma_y
        stress_mpa *= (0.7 + random.random() * 0.6)
        stress_mpa = max(5.0, min(stress_mpa, sigma_u * 1.5))

        impact_x = random.uniform(0.5, self.length - 0.5)
        impact_y = random.uniform(0.3, self.width - 0.3)

        return {
            "rock_mass": round(mass_kg, 2),
            "rock_diameter_mm": round(diameter_cm * 10.0, 1),
            "drop_height_m": round(height_m, 2),
            "rock_velocity": round(velocity, 2),
            "rock_impact_force": round(impact_force_kN, 2),
            "roof_stress": round(stress_mpa, 2),
            "impact_location_x": round(impact_x, 3),
            "impact_location_y": round(impact_y, 3),
            "impact_energy_j": round(impact_energy_j, 2),
            "has_impact": True,
        }

    def calc_thickness_loss(self, impact):
        if not impact["has_impact"]:
            return 0.0
        mat = MATERIAL_PROPERTIES[self.material]
        toughness_pa = mat["toughness_mj_m3"] * 1e6
        radius_m = (impact["rock_diameter_mm"] / 2.0) / 1000.0
        contact_area = math.pi * radius_m * radius_m * 0.6
        volume_absorbed = impact["impact_energy_j"] / max(toughness_pa, 1.0)
        depth_mm = (volume_absorbed / max(contact_area, 1e-6)) * 1000.0
        depth_mm *= mat.get("wear_coeff", 0.3) * self.opts.wear_scale
        return min(depth_mm, 8.0)

    def calc_wheel_deformation(self):
        base = 1.0 + self.impact_count * 0.018
        base += self.cumulative_damage * 0.45
        return round(min(base + random.uniform(-0.2, 0.8), 25.0), 3)

    # ------------------------------------------------------------------
    # 读取传感器 + 上报
    # ------------------------------------------------------------------
    def read_sensors(self):
        impact = self.generate_rock_impact()

        with self.lock:
            loss = self.calc_thickness_loss(impact)
            self.current_thickness = max(self.current_thickness - loss, 1.0)
            if impact["has_impact"]:
                self.impact_count += 1
                ult = MATERIAL_PROPERTIES[self.material]["ultimate_strength_mpa"]
                self.cumulative_damage += impact["roof_stress"] / ult
            self.ambient_temp += random.uniform(-0.4, 0.4)
            self.ambient_temp = max(-15.0, min(self.ambient_temp, 50.0))
            thickness = self.current_thickness

        payload = {
            "vehicle_id": self.vehicle_id,
            "timestamp_ms": int(time.time() * 1000),
            "roof_stress": impact["roof_stress"],
            "wheel_deformation": self.calc_wheel_deformation(),
            "rock_impact_force": impact["rock_impact_force"],
            "protection_thickness": round(thickness, 2),
            "protection_material": self.material,
            "ambient_temp": round(self.ambient_temp, 1),
            "impact_location_x": impact["impact_location_x"],
            "impact_location_y": impact["impact_location_y"],
            "rock_mass": impact["rock_mass"],
            "rock_velocity": impact["rock_velocity"],
            "rock_diameter_mm": impact["rock_diameter_mm"],
            "drop_height_m": impact["drop_height_m"],
        }
        if impact["has_impact"]:
            payload["impact_energy_j"] = impact["impact_energy_j"]
        return payload

    def send_data(self, data):
        url = f"{self.api_url}/api/sensor"
        body = json.dumps(data).encode("utf-8")
        req = Request(url, data=body, method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("User-Agent", f"FenyunSimulator/1.2 (Vehicle-{self.vehicle_id})")
        try:
            with urlopen(req, timeout=8) as resp:
                if resp.status in (200, 201):
                    return True
                else:
                    print(f"[{self.name}] HTTP {resp.status}: {resp.read()[:200]}")
                    return False
        except (URLError, HTTPError) as e:
            print(f"[{self.name}] 发送失败: {e}")
            return False

    def run_once(self):
        data = self.read_sensors()
        ts_str = datetime.fromtimestamp(data["timestamp_ms"] / 1000, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        status = "冲击" if data["rock_mass"] > 0 else "----"
        print(
            f"[{ts_str}] {self.name}(ID:{self.vehicle_id:>2}) | "
            f"{status:4} | "
            f"应力:{data['roof_stress']:>7.1f}MPa | "
            f"力:{data['rock_impact_force']:>6.1f}kN | "
            f"厚度:{data['protection_thickness']:>6.1f}mm | "
            f"石:{data['rock_mass']:>6.1f}kg/Φ{data['rock_diameter_mm']:>6.1f}mm "
            f"@{data['drop_height_m']:>5.1f}m v={data['rock_velocity']:>5.1f}m/s"
        )
        return self.send_data(data)

    def run_loop(self, interval_sec=60):
        self.running = True
        rock = self.opts.rock
        print(
            f"[{self.name}] 模拟器启动  "
            f"质量:{rock.mass_min_kg:.0f}~{rock.mass_max_kg:.0f}kg  "
            f"高度:{rock.height_min_m:.0f}~{rock.height_max_m:.0f}m  "
            f"间隔:{interval_sec}s"
        )
        while self.running:
            try:
                self.run_once()
            except Exception as e:
                print(f"[{self.name}] 异常: {e}")
            for _ in range(int(interval_sec * 10)):
                if not self.running:
                    break
                time.sleep(0.1)
        print(f"[{self.name}] 模拟器停止 | 有效冲击{self.impact_count}次  无冲击{self.no_impact_count}次")


# ----------------------------------------------------------------------
# 多车辆管理
# ----------------------------------------------------------------------
def build_default_options(args) -> VehicleSimOptions:
    rock = RockSpec(
        mass_min_kg=args.rock_mass_min,
        mass_max_kg=args.rock_mass_max,
        height_min_m=args.drop_height_min,
        height_max_m=args.drop_height_max,
        diameter_min_cm=args.rock_diameter_min,
        diameter_max_cm=args.rock_diameter_max,
        no_impact_prob=args.no_impact_prob,
    )
    # 如果选了场景预设，覆盖范围
    if args.impact_profile:
        rock = RockSpec.from_profile(args.impact_profile)

    def_opts = VehicleSimOptions(
        vehicle_id=0,
        api_url=args.api,
        rock=rock,
        wear_scale=args.wear_scale,
        temp_base=args.temp,
        fixed_drop_height_m=args.fixed_drop_height,
        fixed_rock_mass_kg=args.fixed_rock_mass,
        fixed_rock_diameter_cm=args.fixed_rock_diameter,
    )
    return def_opts


def run_batch_simulation(args):
    vehicles = []
    threads = []
    opts_template = build_default_options(args)

    for vid in range(1, args.vehicles + 1):
        opts = VehicleSimOptions(
            vehicle_id=vid,
            api_url=opts_template.api_url,
            rock=opts_template.rock,
            wear_scale=opts_template.wear_scale,
            temp_base=opts_template.temp_base + (vid - 1) * 1.5,
            fixed_drop_height_m=opts_template.fixed_drop_height_m,
            fixed_rock_mass_kg=opts_template.fixed_rock_mass_kg,
            fixed_rock_diameter_cm=opts_template.fixed_rock_diameter_cm,
        )
        sim = VehicleSimulator(opts)
        vehicles.append(sim)
        t = threading.Thread(target=sim.run_loop, args=(args.interval,), daemon=True)
        threads.append(t)
        t.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n正在停止模拟器...")
        for v in vehicles:
            v.running = False
        for t in threads:
            t.join(timeout=3)
        print("模拟器已停止")


def run_single_shot(args):
    print(f"执行单轮模拟上报，{args.vehicles}辆车...")
    tpl = build_default_options(args)
    for vid in range(1, args.vehicles + 1):
        opts = VehicleSimOptions(
            vehicle_id=vid, api_url=tpl.api_url, rock=tpl.rock,
            wear_scale=tpl.wear_scale, temp_base=tpl.temp_base,
            fixed_drop_height_m=tpl.fixed_drop_height_m,
            fixed_rock_mass_kg=tpl.fixed_rock_mass_kg,
            fixed_rock_diameter_cm=tpl.fixed_rock_diameter_cm,
        )
        sim = VehicleSimulator(opts)
        sim.run_once()


def run_stress_test(num_reports, api_url):
    print(f"压力测试：发送 {num_reports} 条批量数据...")
    opts = VehicleSimOptions(vehicle_id=1, api_url=api_url)
    sim = VehicleSimulator(opts)
    batch = []
    for i in range(num_reports):
        data = sim.read_sensors()
        data["timestamp_ms"] = int(time.time() * 1000) - (num_reports - i) * 1000
        batch.append(data)

    url = f"{api_url}/api/sensor/batch"
    body = json.dumps(batch).encode("utf-8")
    req = Request(url, data=body, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("User-Agent", "FenyunSimulator/1.2 (StressTest)")
    try:
        with urlopen(req, timeout=30) as resp:
            print(f"批量发送完成: HTTP {resp.status}, 响应: {resp.read().decode()}")
    except Exception as e:
        print(f"批量发送失败: {e}")


def main():
    parser = argparse.ArgumentParser(
        description="轒辒车传感器模拟器 v1.2",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
场景预设 (--impact-profile):
  light     轻袭    10~50kg   /3~10m    25%无冲击
  normal    常规    20~120kg  /5~20m    10%无冲击  (默认)
  heavy     猛攻    50~250kg  /10~30m   0%无冲击
  barrage   密集滚石 30~180kg /15~40m   0%无冲击
  sniper    定点打击 80~200kg  /25~50m   40%无冲击

示例:
  # 使用默认配置 (常规冲击场景，1分钟上报)
  python sensor_simulator.py

  # 猛攻场景，30秒上报一次
  python sensor_simulator.py --impact-profile heavy --interval 30

  # 固定滚石质量100kg，固定冲击高度15m
  python sensor_simulator.py --fixed-rock-mass 100 --fixed-drop-height 15

  # 固定滚石直径30cm（质量由直径推导）
  python sensor_simulator.py --fixed-rock-diameter 30

  # 自定义滚石质量范围、高度范围
  python sensor_simulator.py --rock-mass-min 50 --rock-mass-max 300 \\
                             --drop-height-min 8  --drop-height-max 40

  # 压力测试：批量发送 2000 条
  python sensor_simulator.py --stress 2000
"""
    )
    parser.add_argument("--api", default=os.environ.get("FENYUN_API", "http://127.0.0.1:8080"),
                        help="后端API地址 (环境变量 FENYUN_API)")
    parser.add_argument("--vehicles", type=int,
                        default=int(os.environ.get("FENYUN_VEHICLES", "3")),
                        help="模拟车辆数量 (1-5, 环境变量 FENYUN_VEHICLES)")
    parser.add_argument("--interval", type=int,
                        default=int(os.environ.get("FENYUN_INTERVAL", "60")),
                        help="上报间隔秒数 (默认60)")
    parser.add_argument("--temp", type=float, default=20.0, help="初始基础温度(℃)")

    # 场景 / 参数范围
    parser.add_argument("--impact-profile",
                        choices=list(IMPACT_PROFILES.keys()),
                        help="冲击场景预设 (覆盖以下范围参数)")
    parser.add_argument("--rock-mass-min", type=float, default=20.0, help="滚石最小质量kg")
    parser.add_argument("--rock-mass-max", type=float, default=200.0, help="滚石最大质量kg")
    parser.add_argument("--drop-height-min", type=float, default=5.0, help="最小落下高度m")
    parser.add_argument("--drop-height-max", type=float, default=25.0, help="最大落下高度m")
    parser.add_argument("--rock-diameter-min", type=float, default=10.0, help="最小直径cm")
    parser.add_argument("--rock-diameter-max", type=float, default=50.0, help="最大直径cm")
    parser.add_argument("--no-impact-prob", type=float, default=0.10, help="无冲击概率(0~1)")

    # 固定参数（可用于确定性仿真 / 重现实验）
    parser.add_argument("--fixed-rock-mass", type=float, default=0.0,
                        help="固定滚石质量kg (>0有效，优先于范围)")
    parser.add_argument("--fixed-drop-height", type=float, default=0.0,
                        help="固定冲击高度m (>0有效)")
    parser.add_argument("--fixed-rock-diameter", type=float, default=0.0,
                        help="固定滚石直径cm (>0有效，会反推质量)")

    parser.add_argument("--wear-scale", type=float, default=1.0, help="防护层磨损倍率 (0=永不磨损)")
    parser.add_argument("--once", action="store_true", help="只上报一次后退出")
    parser.add_argument("--stress", type=int, metavar="N", help="压力测试模式，发送N条批量数据")
    parser.add_argument("--config", type=str, help="从JSON配置文件加载参数 (优先级最低)")

    args = parser.parse_args()
    args.vehicles = max(1, min(args.vehicles, len(VEHICLE_CONFIGS)))

    # 从 JSON 配置文件加载
    if args.config and os.path.exists(args.config):
        with open(args.config, "r", encoding="utf-8") as fp:
            cfg = json.load(fp)
        for k, v in cfg.items():
            if hasattr(args, k):
                setattr(args, k, v)

    print("=" * 78)
    print("  轒辒车传感器模拟器  v1.2")
    print("  模拟春秋时期攻城冲车防护系统传感器数据上报")
    print("=" * 78)
    print(f"API 地址:       {args.api}")
    print(f"车辆数量:       {args.vehicles}")
    print(f"上报间隔:       {args.interval}秒")
    print(f"磨损倍率:       {args.wear_scale}")
    if args.impact_profile:
        print(f"冲击场景预设:   {args.impact_profile}")
    print(f"滚石质量范围:   {args.rock_mass_min:.0f} ~ {args.rock_mass_max:.0f} kg")
    print(f"落下高度范围:   {args.drop_height_min:.0f} ~ {args.drop_height_max:.0f} m")
    print(f"滚石直径范围:   {args.rock_diameter_min:.0f} ~ {args.rock_diameter_max:.0f} cm")
    print(f"无冲击概率:     {args.no_impact_prob*100:.0f}%")
    if args.fixed_rock_mass > 0:
        print(f"[固定参数] 质量 = {args.fixed_rock_mass:.1f} kg")
    if args.fixed_drop_height > 0:
        print(f"[固定参数] 高度 = {args.fixed_drop_height:.1f} m")
    if args.fixed_rock_diameter > 0:
        print(f"[固定参数] 直径 = {args.fixed_rock_diameter:.1f} cm → 质量 = "
              f"{VehicleSimulator._rock_mass_from_diameter(args.fixed_rock_diameter):.1f} kg")
    print("=" * 78)

    if args.stress:
        run_stress_test(args.stress, args.api)
    elif args.once:
        run_single_shot(args)
    else:
        print("按 Ctrl+C 停止\n")
        run_batch_simulation(args)


if __name__ == "__main__":
    main()
