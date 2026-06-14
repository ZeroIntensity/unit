Installation
============

Installing UNIT
---------------

UNIT can be installed via `Git <https://git-scm.com/>`_ and
`CMake <https://cmake.org/>`_:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ git clone https://github.com/ZeroIntensity/unit && cd unit
   $ cmake -B build -DCMAKE_BUILD_TYPE=Release
   $ cmake --build build
   $ sudo cmake --install build


This installs the library and headers to your system's default prefix
(on Linux, this is ``/usr/local``). To install elsewhere, pass the
``CMAKE_INSTALL_PREFIX`` setting:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
   $ cmake --build build
   $ cmake --install build


Using UNIT in your project
--------------------------


CMake
*****

Add the following to your ``CMakeLists.txt`` file:``

.. code-block:: cmake
   :caption: :iconify:`file-icons:cmake` CMakeLists.txt

   find_package(unit REQUIRED)
   target_link_libraries(my_program PRIVATE unit::unit)


Meson
*****

.. code-block:: meson
   :caption: :iconify:`file-icons:meson` meson.build

   unit_dep = dependency('unit')
   executable('my_program', 'main.c', dependencies: unit_dep)


pkg-config
**********

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ gcc -o my_program main.c $(pkg-config --cflags --libs unit)


Makefile + pkg-config
*********************

.. code-block:: make
   :caption: :iconify:`vscode-icons:file-type-makefile` Makefile

   CFLAGS := $(shell pkg-config --cflags unit)
   LDFLAGS := $(shell pkg-config --libs unit)


GCC/Clang
*********

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ gcc -o my_program main.c -lunit


MSVC
****

.. code-block:: powershell
   :caption: :iconify:`mdi:powershell` Powershell

   PS C:\Users\UNIT> cl /I path\to\unit\include main.c /link path\to\unit\lib\unit.lib
