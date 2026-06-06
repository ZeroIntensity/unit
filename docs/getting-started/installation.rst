Installation
============

UNIT can be installed via `Git <https://git-scm.com/>`_ and
`CMake <https://cmake.org/>`_:

.. code-block:: bash

   git clone https://github.com/ZeroIntensity/unit && cd unit
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   sudo cmake --install build


This installs the library and headers to your system's default prefix
(on Linux, this is ``/usr/local``). To install elsewhere, pass the
``CMAKE_INSTALL_PREFIX`` setting:

.. code-block:: bash

   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
   cmake --build build
   cmake --install build


Using UNIT in your project
--------------------------


CMake
*****

Add the following to your ``CMakeLists.txt`` file:``

.. code-block:: cmake

   find_package(unit REQUIRED)
   target_link_libraries(my_program PRIVATE unit::unit)


Meson
*****

.. code-block:: meson

   unit_dep = dependency('unit')
   executable('my_program', 'main.c', dependencies: unit_dep)


pkg-config
**********

.. code-block:: bash

   gcc -o my_program main.c $(pkg-config --cflags --libs unit)


Makefile + pkg-config
*********************

.. code-block:: make

   CFLAGS := $(shell pkg-config --cflags unit)
   LDFLAGS := $(shell pkg-config --libs unit)


GCC/Clang (direct)
******************

.. code-block:: bash

   gcc -o my_program main.c -lunit


MSVC
****

.. code-block:: powershell

   cl /I path\to\unit\include main.c /link path\to\unit\lib\unit.lib
