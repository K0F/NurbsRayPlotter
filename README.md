# Nurbs Ray Plotter

 _This is an attempt to render NURBS curves, bridging OpenNurbsLib and RayLib._

## Build the source

There is a submodule opennurbs lib to operate. RayLib is consedered already installed at this point. To do the rest:

```bash
	git clone ... repo url
	git submodule update
	cd externals/opennurbs
	make
	cd ../..
	make
```
