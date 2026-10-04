"""SPDX-License-Identifier: Apache-2.0 — Copyright 2026 Solvor Labs."""
from __future__ import annotations
import math
from pathlib import Path
from xml.etree import ElementTree as ET
from .cache import read_cache


def write_world(cache: Path, output: Path, *, mode: str = "overview",
                camera_pose: tuple[float, ...] | None = None,
                pose_topic: str | None = None, camera_target: str | None = None,
                intrinsics: tuple[float, ...] | None = None,
                screenshot: Path | None = None, force: bool = False) -> None:
    if mode not in {"overview", "diagnostic", "captured"}:
        raise ValueError("unsupported view mode")
    if bool(pose_topic) != bool(camera_target):
        raise ValueError("pose_topic and camera_target must be supplied together")
    if mode != "captured" and (intrinsics or pose_topic):
        raise ValueError("intrinsics and camera following require captured mode")
    if mode == "captured" and camera_pose is None:
        raise ValueError("captured mode requires a camera pose in Gazebo axes")
    if camera_pose and (len(camera_pose) != 6 or not all(math.isfinite(v) for v in camera_pose)):
        raise ValueError("camera pose must be finite x y z roll pitch yaw")
    if output.exists() and not force:
        raise FileExistsError(f"output exists: {output}")
    if screenshot:
        if screenshot.resolve() in {cache.resolve(), output.resolve()}:
            raise ValueError("screenshot must not overwrite the cache or world")
        if screenshot.exists() and not force:
            raise FileExistsError(f"screenshot exists: {screenshot}")
    minimum = [math.inf] * 3
    maximum = [-math.inf] * 3
    for point in read_cache(cache):
        for axis, value in enumerate(point.position):
            minimum[axis] = min(minimum[axis], value)
            maximum[axis] = max(maximum[axis], value)
    center = tuple((a+b)/2 for a, b in zip(minimum, maximum))
    extent = max(maximum[i] - minimum[i] for i in range(3)) or 1.0
    if camera_pose is None:
        offset = (extent * 1.1, -extent * 1.1, extent * 0.8)
        camera_pose = tuple(center[i]+offset[i] for i in range(3)) + (
            0.0, math.atan2(offset[2], math.hypot(*offset[:2])),
            math.atan2(-offset[1], -offset[0]))
    root = ET.Element("sdf", version="1.9")
    world = ET.SubElement(root, "world", name="gaussian_viewer")
    for filename, name in (
        ("gz-sim-physics-system", "gz::sim::systems::Physics"),
        ("gz-sim-user-commands-system", "gz::sim::systems::UserCommands"),
        ("gz-sim-scene-broadcaster-system", "gz::sim::systems::SceneBroadcaster"),
    ):
        ET.SubElement(world, "plugin", filename=filename, name=name)
    scene = ET.SubElement(world, "scene")
    ET.SubElement(scene, "ambient").text = "0.8 0.8 0.8 1"
    ET.SubElement(scene, "background").text = "0.055 0.06 0.075 1"
    ET.SubElement(scene, "shadows").text = "false"
    ET.SubElement(scene, "grid").text = "false"
    # A native visual establishes the GUI render lifecycle even when the
    # custom scene is the only content. It is placed far below the splats.
    model = ET.SubElement(world, "model", name="render_anchor")
    ET.SubElement(model, "static").text = "true"
    ET.SubElement(model, "pose").text = f"0 0 {minimum[2]-10*extent} 0 0 0"
    link = ET.SubElement(model, "link", name="anchor")
    visual = ET.SubElement(link, "visual", name="anchor")
    geometry = ET.SubElement(visual, "geometry")
    ET.SubElement(ET.SubElement(geometry, "sphere"), "radius").text = "0.00001"
    if mode == "diagnostic":
        floor = ET.SubElement(world, "model", name="diagnostic_floor")
        ET.SubElement(floor, "static").text = "true"
        ET.SubElement(floor, "pose").text = f"{center[0]} {center[1]} {minimum[2]-.03*extent} 0 0 0"
        visual = ET.SubElement(ET.SubElement(floor, "link", name="floor"), "visual", name="floor")
        ET.SubElement(ET.SubElement(ET.SubElement(visual, "geometry"), "box"), "size").text = f"{extent*1.3} {extent*1.3} {extent*.01}"
        ET.SubElement(ET.SubElement(visual, "material"), "diffuse").text = "0.12 0.15 0.2 1"
    gui = ET.SubElement(world, "gui", fullscreen="false")
    view = ET.SubElement(gui, "plugin", filename="MinimalScene", name="3D View")
    view_config = ET.SubElement(view, "gz-gui")
    ET.SubElement(view_config, "property", key="showTitleBar", type="bool").text = "false"
    ET.SubElement(view_config, "property", key="state", type="string").text = "docked"
    ET.SubElement(view, "engine").text = "ogre2"
    ET.SubElement(view, "scene").text = "scene"
    ET.SubElement(view, "ambient_light").text = "0.8 0.8 0.8"
    ET.SubElement(view, "background_color").text = "0.055 0.06 0.075"
    ET.SubElement(view, "camera_pose").text = " ".join(str(v) for v in camera_pose)
    plugin = ET.SubElement(gui, "plugin", filename="GaussianSplatRenderer", name="Gaussian splats")
    config = ET.SubElement(plugin, "gz-gui")
    for key, value, kind in (("visible", "false", "bool"), ("showTitleBar", "false", "bool"),
                             ("state", "floating", "string"), ("width", "1", "double"),
                             ("height", "1", "double")):
        ET.SubElement(config, "property", key=key, type=kind).text = value
    ET.SubElement(plugin, "cache_path").text = str(cache.resolve())
    ET.SubElement(plugin, "view_mode").text = mode
    ET.SubElement(plugin, "radius_sigma").text = "3"
    if pose_topic:
        ET.SubElement(plugin, "pose_topic").text = pose_topic
        ET.SubElement(plugin, "camera_target").text = camera_target
    if intrinsics:
        width, height, fx, fy, cx, cy = intrinsics
        if (not all(math.isfinite(v) for v in intrinsics) or
                not 1 <= width <= 16384 or not 1 <= height <= 16384 or
                int(width) != width or int(height) != height or
                fx <= 0 or fy <= 0):
            raise ValueError("invalid calibrated intrinsics")
        for field, value in zip(("width", "height", "fx", "fy", "cx", "cy"), intrinsics):
            ET.SubElement(plugin, f"capture_{field}").text = str(int(value) if field in {"width", "height"} else value)
    if screenshot:
        screenshot.parent.mkdir(parents=True, exist_ok=True)
        ET.SubElement(plugin, "capture_path").text = str(screenshot.resolve())
    manager = ET.SubElement(gui, "plugin", filename="GzSceneManager", name="Scene manager")
    manager_config = ET.SubElement(manager, "gz-gui")
    for key, value, kind in (("showTitleBar", "false", "bool"), ("state", "floating", "string"),
                             ("width", "1", "double"), ("height", "1", "double")):
        ET.SubElement(manager_config, "property", key=key, type=kind).text = value
    # Interactive orbit/pan/zoom requires the native camera-control plugin.
    if mode != "captured":
        ET.SubElement(gui, "plugin", filename="InteractiveViewControl", name="View control")
    output.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(root)
    ET.ElementTree(root).write(output, encoding="utf-8", xml_declaration=True)
