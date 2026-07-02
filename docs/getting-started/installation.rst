.. _installation:

Installation
============

Installing UNIT
---------------

UNIT can be installed via `Git <https://git-scm.com/>`_ and
`CMake <https://cmake.org/>`_:

.. code-block:: bash
   :linenos:
   :caption: :iconify:`devicon-plain:bash` bash

   git clone https://github.com/ZeroIntensity/unit && cd unit
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   sudo cmake --install build


This installs the library and headers to your system's default prefix
(on Linux, this is ``/usr/local``). To install elsewhere, pass the
``CMAKE_INSTALL_PREFIX`` setting:

.. code-block:: bash
   :linenos:
   :caption: :iconify:`devicon-plain:bash` bash

   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
   cmake --build build
   cmake --install build


Python bindings
***************

Alternatively, if you plan on using the Python bindings without the C API or
C++ API, you can solely install that using your preferred package manager:


.. tabs::

   .. tab:: pip

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         pip install unit-compiler

   .. tab:: uv

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         uv add unit-compiler

   .. tab:: Poetry

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         poetry add unit-compiler

   .. tab:: PDM

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         pdm add unit-compiler

   .. tab:: pipenv

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         pipenv install unit-compiler

   .. tab:: Conda

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         conda install -c conda-forge unit-compiler

   .. tab:: Pixi

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         pixi add unit-compiler

   .. tab:: Hatch

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         hatch dep add unit-compiler


Using UNIT in your project
--------------------------


.. tabs::

   .. tab:: CMake

      .. code-block:: cmake
         :caption: :iconify:`file-icons:cmake` CMakeLists.txt

         find_package(unit REQUIRED)
         target_link_libraries(my_program PRIVATE unit::unit)

   .. tab:: Meson

      .. code-block:: meson
         :caption: :iconify:`file-icons:meson` meson.build

         unit_dep = dependency('unit')
         executable('my_program', 'main.c', dependencies: unit_dep)

   .. tab:: pkg-config

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         gcc -o my_program main.c $(pkg-config --cflags --libs unit)

   .. tab:: Makefile + pkg-config

      .. code-block:: make
         :caption: :iconify:`vscode-icons:file-type-makefile` Makefile

         CFLAGS := $(shell pkg-config --cflags unit)
         LDFLAGS := $(shell pkg-config --libs unit)

   .. tab:: GCC/Clang

      .. code-block:: bash
         :caption: :iconify:`devicon-plain:bash` bash

         gcc -o my_program main.c -lunit

   .. tab:: MSVC

      .. code-block:: powershell
         :caption: :iconify:`mdi:powershell` Powershell

         cl /I path\to\unit\include main.c /link path\to\unit\lib\unit.lib
