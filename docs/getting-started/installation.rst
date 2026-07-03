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

If you plan on using the Python bindings, you can install the ``unit-compiler``
package from PyPI. UNIT is under heavy development, so it is recommended that
you install the development package:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   pip install --pre -i https://test.pypi.org/simple/ unit-compiler


Alternatively, if you cloned UNIT and built from source, you can simply
install from the root directory:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   cd unit
   pip install .


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
