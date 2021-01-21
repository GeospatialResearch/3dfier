---
title: Installation
keywords: 3dfier installation ubuntu docker windows compile
summary: These instructions will help you to install 3dfier on various operating systems. For Windows please use the binary files and do not compile from
sidebar: 3dfier_sidebar
permalink: installation.html
---

## Install on Windows using binaries
Binary releases exist only for Windows users. Others will have to follow one of the other installation guides for [Linux](#ubuntu-1604) or [Docker](#docker)
There exists a ready-to-use version of [3dfier for Windows 64-bit](https://github.com/{{site.repository}}/releases/latest). Download and extract the files to any given folder and follow the instructions in the [Get started guide]({{site.baseurl}}/index).

### Release binaries content
Description of files in the zip file

All dll files distributed with 3dfier belong to GDAL or other packages used in the GDAL drivers. Other depencencies used are statically built within the executable. 

Filename | Package
---------|--------
3dfier.exe | 3dfier
expat.dll | GDAL
freexl.dll | GDAL
gdal204.dll | GDAL
geos.dll | GDAL
geos_c.dll | GDAL
hdf5.dll | GDAL
hdf5_hl.dll | GDAL
iconv-2.dll | GDAL
iconv.dll | GDAL
jpeg.dll | GDAL
libcurl.dll | GDAL
libeay32.dll | GDAL
libgmp-10.dll | GDAL
liblzma.dll | GDAL
libmpfr-4.dll | GDAL
libmysql.dll | GDAL
libpng16.dll | GDAL
libpq.dll | GDAL
libxml2.dll | GDAL
lwgeom.dll | GDAL
netcdf.dll | GDAL
ogdi.dll | GDAL
openjp2.dllv | GDAL
proj_5_2.dll | GDAL
spatialite.dll | GDAL
sqlite3.dll | GDAL
ssleay32.dll | GDAL
szip.dll | GDAL
xerces-c_3_2.dll | GDAL
zlib1.dll | GDAL
zstd.dll | GDAL


## macOS

You need to install the following free libraries:

  1. [CMake](http://www.cmake.org)
  1. [CGAL v5.0+](http://www.cgal.org) 
  1. [GDAL](https://gdal.org/)
  1. [yaml-cpp](https://github.com/jbeder/yaml-cpp)
  1. [LASlib](https://github.com/LAStools/LAStools/tree/master/LASlib)
  1. [LASzip](https://github.com/LAStools/LAStools/tree/master/LASzip)

We suggest using [Homebrew](http://brew.sh/) for the first 4:

    $ brew install cgal
    $ brew install gdal
    $ brew install cmake

For LASlib/LASzip, follow the instruction and use the `CMakeLists.txt`.

To compile 3dfier:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make install
    $ 3dfier


## Ubuntu 20.04
### 1. Adding *ubuntugis-unstable* repository
To install *GDAL* on Ubuntu 20.04 LTS it is probably the easiest to add the [*ubuntugis-unstable*](https://launchpad.net/~ubuntugis/+archive/ubuntu/ubuntugis-unstable?field.series_filter=xenial). It contains *GDAL* >= 2.1 under (`libgdal-dev`) package.

*Note: ubuntugis-stable repository doesn't contain any Ubuntu 20.04 packages yet.*

Add the *ubuntugis-unstable* repository by running:
```
sudo add-apt-repository ppa:ubuntugis/ubuntugis-unstable
sudo apt-get update
```

### 2. Install dependencies
*CGAL* (`libcgal-dev`), *Boost* (`libboost-all-dev`) and *yaml-cpp* (`libyaml-cpp-dev`) are part of the *Ubuntu Universe* repository.

Once you have all the repos added, you can use a package manager, e.g. `apt` or *Synaptic* to install them. 

E.g. using apt-get:
```
sudo apt-get install libcgal-dev libboost-all-dev libyaml-cpp-dev libgdal-dev
```

### 3. Compile LAStools
*LAStools* contains both the *LASlib* and *LASzip*.

This step requires *CMake* and *UnZip* packages. Some Linux distributions don't have them preinstalled:
```
sudo apt-get install -y unzip cmake
```
Download and compile *LAStools* with the following:
```
wget  http://lastools.github.io/download/LAStools.zip
unzip LAStools.zip
cd LAStools; mv LASlib/src/LASlib-config.cmake LASlib/src/laslib-config.cmake
mkdir build; cd build
cmake ..
sudo make install
```

### 4. Compile 3dfier

Download and compile 3dfier:
```
git clone https://github.com/tudelft3d/3dfier.git
cd 3dfier; mkdir build; cd build
cmake ..
sudo make install
```
Test the installation by trying out the [first run]({{site.baseurl}}/first_run).

## Windows
This guide will talk you through the compilation of 3dfier on Windows 10 64-bit using Visual Studio (steps are identical for Visual Studio 2017 and 2019).

There are some steps to be taken to prepare the build environment for 3dfier. Most important is installing software to compile and downloading the libraries 3dfier is depending on.

*Note 1: Versions used in this guide are the versions used at time of writing. Future versions of libraries could be supported but usage can change and the Visual Studio Solution might need changing for them to work.*

*Note 2: In this guide we build 3dfier in 64-bit (x64) and all dependencies must also be built in 64-bit. For project files created with CMake the `-A x64` switch should explicitly be used.*

*Note 3: Since CGAL 5.0 the library is header only. Building the library is not needed anymore. The current Visual Studio project file in the repository is made for version 4.xx, when using verions >5.0 one should remove the library includes from the project file.*

### 1. Running installers
First you will need to download and install in their default directorties:
1. [Visual Studio Community (2017 or later)](https://www.visualstudio.com/downloads/). Install at least the C++ part.
1. [CMake (3.15 or later)](https://cmake.org/download/), download and install `Windows win64-x64 Installer`. Add variable to the PATH during installation.
1. [Boost precompiled binaries (1.71 or later)](https://sourceforge.net/projects/boost/files/boost-binaries). Pick the latest version that is built for your [MSVC++ compiler version](https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B). Install boost using the installer.
1. [OSGeo4W (with GDAL 2.3.0 or later)](https://trac.osgeo.org/osgeo4w), download the [64-bit installer](http://download.osgeo.org/osgeo4w/osgeo4w-setup-x86_64.exe). From this package you will need to install at least the GDAL package.
1. [CGAL (4.12 or later)](https://github.com/CGAL/cgal/releases), download `CGAL-4.12-Setup.exe` (or newer) and install. Select *GMP and MPFR precompiled libs*, *Environment variables to set CGAL_DIR* and *Add CGAL/auxilary/gmp/lib to the PATH* during setup.

### 2. Compilation of dependencies
Next, we need to download and compile Yaml-cpp and LAStools. 

#### Yaml-cpp
Download [yaml-cpp (0.5.3 or later)](https://github.com/jbeder/yaml-cpp/releases) and extract to e.g. `C:\dev\yaml-cpp`. There are two options of getting the Visual Studio project files using CMake:

1. using CMake GUI ([tutorial here](https://cmake.org/runningcmake/)).

1. using command line. 
Open a Command prompt (press windows button+R, type cmd and press enter) and navigate to the yaml-cpp directory:
```
cd C:\dev\yaml-cpp
```
Generate the Visual Studio build files with
```
mkdir vs_build
cd vs_build
cmake .. -G "Visual Studio 15 2017 Win64"
```

After generation open the Visual Studio file `YAML_CPP.sln`. Set the solution configuration to `Release` in the main toolbar. From the menu bar select Build and click `Build Solution`.

#### LAStools
Download [LAStools](https://rapidlasso.com/lastools/) and extract to e.g. `C:\dev\lastools`.

Use CMake as explained previous for [Yaml-cpp](#yaml-cpp) to generate the Visual Studio solution files.

After generation open the Visual Studio file `LASlib.sln`. Set the solution configuration to `Release` in the main toolbar. From the menu bar select Build and click `Build Solution`.

### 3. Set environment variables
Go to `Control Panel > System > Advanced system settings > Environment Variables` and add the following user variables. Note that the version numbers and the installation paths may be different!
* `BOOST_ROOT`=`C:\boost_1_71_0`
* `BOOST_LIBRARYDIR`=`C:\boost_1_71_0\lib64-msvc-14.0`
* `CGAL_DIR`=`C:\dev\CGAL-4.12`
* `GDAL_ROOT`=`C:\OSGeo4W64`
* `LASLIB_ROOT`=`C:\dev\lastools\LASlib`
* `LASZIP_ROOT`=`C:\dev\lastools\LASzip`
* `OSGEO4W_ROOT`=`C:\OSGeo4W64`
* `YAML-CPP_DIR`=`C:\dev\yaml-cpp`

Go to `Control Panel > System > Advanced system settings > Environment Variables` and add the following directory to Path.
* `C:\OSGeo4W64\bin`

### 4. Compile 3dfier
Download and extract the source code from the menu on the left or fork directly from GitHub. Browse to the vs_build folder and open the Visual Studio file `3dfier.sln`.

If in any case the Visual Studio solution is not working its possible to generate them from the CMake files directly as explained previous for [Yaml-cpp](#yaml-cpp).

### 5. Run 3dfier!
If all is good you should now be able to run 3dfier! Go to the [First run]({{site.baseurl}}/first_run) and start producing models.

* * * 
#### Help, Visual Studio complains that some file can not be found!
Check whether the directories and files specified in the environment variables are correct. Also check these places in Visual Studio:
* the include folders in `Project > Properties > C/C++ > General > Additional Include Directories`
* the library folders in `Project > Properties > Linker > General > Additional Library Directories`
* the libraries files in `Project > Properties > Linker > Input > Additional Dependencies`

Make sure each directory or file exists on your drive. For example: you may need to change a version number somewhere.

## Docker
We offer built docker images from the `master`, `development` branches and each release. You'll find the images and instructions on using them at [Docker Hub](https://hub.docker.com/r/tudelft3d/3dfier).
