# Synthetic room

`gzgs demo --output-dir demo-output` generates an original redistributable
scene with checkerboard floor, two coloured walls and an elongated red Gaussian.
It is deliberately synthetic: no private phone video, reconstruction or model
weights are bundled. The cache and SDF are generated on your machine so file
paths are correct. Run `scripts/run_demo.sh` after building.

Use `gzgs world demo-output/room.gzgs diagnostic.sdf --mode diagnostic` to see a
native Gazebo floor under the splats. Use `scripts/gui_smoke.sh` to verify that
a captured viewport contains coloured scene content instead of a blank image.
