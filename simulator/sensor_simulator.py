#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
轒辒车传感器模拟器
模拟春秋时期轒辒车攻城场景下，每辆车每分钟通过模拟传感器上报数据。

上报字段：
- vehicle_id: 车辆ID
- timestamp_ms: 时间戳(毫秒)
- roof_stress: 顶棚应力(MPa)
- wheel_deformation: 车轮变形(mm)
- rock_impact_force: 滚石冲击力(kN)
- protection_thickness: 防护层剩余厚度(mm)
- protection_material: 防护材料类型(cowhide/wood/iron/composite)
- ambient_temp: 环境温度(℃)
- impact_location_x/y: 冲击点坐标
- rock_mass: 滚石质量(kg)
- rock_velocity: 滚石速度(m/s)
"""

import json
import time
import random
import math
import threading
import argparse
import sys
from datetime import datetime, timezone
from urllib.request import Request, urlopen
from urllib.error import URLError, HTTPError


MATERIAL_PROPERTIES = {
    "cowhide": {
        "density": 860.0, "youngs_modulus_gpa": 0.15, "poisson_ratio": 0.40,
        "yield_strength_mpa": 25.0, "ultimate_strength_mpa": 60.0,
        "toughness_mj_m3": 15.0, "specific_energy_absorption_kj_kg": 80.0,
        "base_thickness_mm": 20.0
    },
    "wood": {
        "density": 650.0, "youngs_modulus_gpa": 10.0, "poisson_ratio": 0.35,
        "yield_strength_mpa": 60.0, "ultimate_strength_mpa": 120.0,
        "toughness_mj_m3": 10.0, "specific_energy_absorption_kj_kg": 50.0,
        "base_thickness_mm": 80.0
    },
    "iron": {
        "density": 7850.0, "youngs_modulus_gpa": 206.0, "poisson_ratio": 0.29,
        "yield_strength_mpa": 235.0, "ultimate_strength_mpa": 400.0,
        "toughness_mj_m3": 80.0, "specific_energy_absorption_kj_kg": 200.0,
        "base_thickness_mm": 5.0
    },
    "composite": {
        "density": 900.0, "youngs_modulus_gpa": 15.0, "poisson_ratio": 0.33,
        "yield_strength_mpa": 120.0, "ultimate_strength_mpa": 250.0,
        "toughness_mj_m3": 45.0, "specific_energy_absorption_kj_kg": 300.0,
        "base_thickness_mm": 50.0
    }
}


VEHICLE_CONFIGS = {
    1: {"name": "先锋一号", "material": "wood", "thickness": 80.0, "length": 6.5, "width": 2.8},
    2: {"name": "雷霆二号", "material": "iron", "thickness": 105.0, "length": 7.0, "width": 3.0},
    3: {"name": "攻坚三号", "material": "composite", "thickness": 130.0, "length": 6.8, "width": 2.9}
}


class VehicleSimulator:
    def __init__(self, vehicle_id, api_url="http://127.0.0.1:8080"):
        self.vehicle_id = vehicle_id
        self.api_url = api_url
        config = VEHICLE_CONFIGS.get(vehicle_id, VEHICLE_CONFIGS[1])
        self.name = config["name"]
        self.material = config["material"]
        self.initial_thickness = config["thickness"]
        self.current_thickness = self.initial_thickness
        self.length = config["length"]
        self.width = config["width"]
        self.ambient_temp = 20.0 + random.uniform(-5, 5)
        self.running = False
        self.cumulative_damage = 0.0
        self.impact_count = 0
        self.lock = threading.Lock()
        self.stress_history = []

    def generate_rock_impact(self):
        rock_mass = random.uniform(20, 200)
        drop_height = random.uniform(5, 25)
        rock_velocity = math.sqrt(2 * 9.81 * drop_height)
        contact_area = math.pi * (rock_mass / (4.0 / 3.0 * math.pi * 2600.0)) ** (2.0 / 3.0)
        impact_energy = 0.5 * rock_mass * rock_velocity ** 2
        impulse = math.sqrt(2 * impact_energy * 100.0)
        contact_time = 0.003 + random.uniform(0, 0.005)
        impact_force_kN = (impulse / contact_time) / 1000.0

        mat = MATERIAL_PROPERTIES[self.material]
        ultimate_stress = mat["ultimate_strength_mpa"]
        roof_stress = min(impact_force_kN * 1000 / (contact_area * 1e6) * 100 + random.uniform(-5, 10),
                          ultimate_stress * 1.5)

        impact_x = random.uniform(0.5, self.length - 0.5)
        impact_y = random.uniform(0.3, self.width - 0.3)

        return {
            "rock_mass": round(rock_mass, 2),
            "rock_velocity": round(rock_velocity, 2),
            "rock_impact_force": round(impact_force_kN, 2),
            "roof_stress": round(max(roof_stress, 5), 2),
            "impact_location_x": round(impact_x, 3),
            "impact_location_y": round(impact_y, 3),
            "impact_energy_j": round(impact_energy, 2)
        }

    def calc_thickness_loss(self, impact):
        mat = MATERIAL_PROPERTIES[self.material]
        toughness_pa = mat["toughness_mj_m3"] * 1e6
        contact_area = math.pi * (impact["rock_mass"] / (4.0 / 3.0 * math.pi * 2600.0)) ** (2.0 / 3.0)
        volume_absorbed = impact["impact_energy_j"] / max(toughness_pa, 1.0)
        depth_mm = (volume_absorbed / max(contact_area, 1e-6)) * 1000.0
        return min(depth_mm * 0.3, 5.0)

    def calc_wheel_deformation(self):
        base = 1.0 + self.impact_count * 0.02
        base += self.cumulative_damage * 0.5
        return round(min(base + random.uniform(-0.2, 0.8), 20.0), 3)

    def read_sensors(self):
        impact = self.generate_rock_impact()

        with self.lock:
            thickness_loss = self.calc_thickness_loss(impact)
            self.current_thickness = max(self.current_thickness - thickness_loss, 1.0)
            self.cumulative_damage += impact["roof_stress"] / MATERIAL_PROPERTIES[self.material]["ultimate_strength_mpa"]
            self.impact_count += 1
            self.ambient_temp += random.uniform(-0.5, 0.5)
            self.ambient_temp = max(-10.0, min(self.ambient_temp, 45.0))
            thickness = self.current_thickness

        return {
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
            "rock_velocity": impact["rock_velocity"]
        }

    def send_data(self, data):
        url = f"{self.api_url}/api/sensor"
        body = json.dumps(data).encode("utf-8")
        req = Request(url, data=body, method="POST")
        req.add_header("Content-Type", "application/json")
        try:
            with urlopen(req, timeout=5) as resp:
                if resp.status == 201:
                    return True
                else:
                    print(f"[{self.name}] HTTP {resp.status}: {resp.read()}")
                    return False
        except (URLError, HTTPError) as e:
            print(f"[{self.name}] 发送失败: {e}")
            return False

    def run_once(self):
        data = self.read_sensors()
        ts_str = datetime.fromtimestamp(data["timestamp_ms"] / 1000, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{ts_str}] {self.name}(ID:{self.vehicle_id}) | "
              f"应力:{data['roof_stress']:.1f}MPa | "
              f"冲击力:{data['rock_impact_force']:.1f}kN | "
              f"厚度:{data['protection_thickness']:.1f}mm | "
              f"滚石:{data['rock_mass']:.1f}kg@{data['rock_velocity']:.1f}m/s")
        return self.send_data(data)

    def run_loop(self, interval_sec=60):
        self.running = True
        print(f"[{self.name}] 模拟器启动，每{interval_sec}秒上报一次...")
        while self.running:
            try:
                self.run_once()
            except Exception as e:
                print(f"[{self.name}] 异常: {e}")
            for _ in range(interval_sec * 10):
                if not self.running:
                    break
                time.sleep(0.1)
        print(f"[{self.name}] 模拟器停止")


def run_batch_simulation(num_vehicles=3, api_url="http://127.0.0.1:8080", interval_sec=60):
    vehicles = []
    threads = []

    for vid in range(1, num_vehicles + 1):
        sim = VehicleSimulator(vid, api_url)
        vehicles.append(sim)
        t = threading.Thread(target=sim.run_loop, args=(interval_sec,), daemon=True)
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


def run_single_shot(num_vehicles=3, api_url="http://127.0.0.1:8080"):
    print(f"执行单轮模拟上报，{num_vehicles}辆车...")
    for vid in range(1, num_vehicles + 1):
        sim = VehicleSimulator(vid, api_url)
        sim.run_once()


def run_stress_test(num_reports=100, api_url="http://127.0.0.1:8080"):
    print(f"压力测试：发送 {num_reports} 条批量数据...")
    sim = VehicleSimulator(1, api_url)
    batch = []
    for i in range(num_reports):
        data = sim.read_sensors()
        data["timestamp_ms"] = int(time.time() * 1000) - (num_reports - i) * 1000
        batch.append(data)

    url = f"{api_url}/api/sensor/batch"
    body = json.dumps(batch).encode("utf-8")
    req = Request(url, data=body, method="POST")
    req.add_header("Content-Type", "application/json")
    try:
        with urlopen(req, timeout=30) as resp:
            print(f"批量发送完成: HTTP {resp.status}, 响应: {resp.read().decode()}")
    except Exception as e:
        print(f"批量发送失败: {e}")


def main():
    parser = argparse.ArgumentParser(description="轒辒车传感器模拟器")
    parser.add_argument("--api", default="http://127.0.0.1:8080", help="后端API地址")
    parser.add_argument("--vehicles", type=int, default=3, help="模拟车辆数量 (1-5)")
    parser.add_argument("--interval", type=int, default=60, help="上报间隔秒数 (默认60秒/1分钟)")
    parser.add_argument("--once", action="store_true", help="只上报一次后退出")
    parser.add_argument("--stress", type=int, metavar="N", help="压力测试模式，发送N条数据")

    args = parser.parse_args()
    args.vehicles = max(1, min(args.vehicles, 5))

    print("=" * 60)
    print("  轒辒车传感器模拟器")
    print("  模拟春秋时期攻城冲车防护系统传感器数据上报")
    print("=" * 60)
    print(f"API 地址:   {args.api}")
    print(f"车辆数量:   {args.vehicles}")
    print(f"上报间隔:   {args.interval}秒")

    if args.stress:
        run_stress_test(args.stress, args.api)
    elif args.once:
        run_single_shot(args.vehicles, args.api)
    else:
        print("按 Ctrl+C 停止\n")
        run_batch_simulation(args.vehicles, args.api, args.interval)


if __name__ == "__main__":
    main()
