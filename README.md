# Nurbs Ray Plotter

_C way of utilizing C++ library openNURBSlib and rendering results in RayLib, both visually and sonically._

## Prerequesties

- RayLib https://github.com/raysan5/raylib
- openNURBS are submodule of project


## Build

There is a submodule openNURBSlib to operate. RayLib is considered already installed at this point.

To build the project on Linux do:

```bash
git clone https://github.com/K0F/NurbsRayPlotter.git
cd NurbsRayPlotter
git submodule update
cd externals/opennurbs
make
cd ../..
make
./nurbsRayPlotter
```

Other operating systems also possible but not tried.

Happy modding!
