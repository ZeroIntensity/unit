
.. _c-versions:

Versions
========

.. c:macro:: UNIT_VERSION_MAJOR

   Major version number.


.. c:macro:: UNIT_VERSION_MINOR

   Minor version number.


.. c:macro:: UNIT_VERSION_PATCH

   Patch version number.


.. c:macro:: UNIT_VERSION_DEV

   Development revision. ``0`` for release builds, ``1+`` for development
   builds.


.. c:macro:: UNIT_VERSION_STRING

   Version as a string. Release builds produce ``"X.Y.Z"``, development
   builds produce ``"X.Y.Z.devN"``.


.. c:macro:: UNIT_VERSION_HEX

   Version as a packed integer for compile-time comparisons. Use
   :c:macro:`UNIT_PACK_VERSION` to construct comparison values.


.. c:macro:: UNIT_PACK_VERSION(major, minor, patch)

   Pack a version for comparison against :c:macro:`UNIT_VERSION_HEX`.
   The low byte is set to ``0xff`` (release), so development versions
   of the same major/minor/patch always compare less.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      #if UNIT_VERSION_HEX >= UNIT_PACK_VERSION(0, 2, 0)
          // use feature added in 0.2.0
      #endif


.. c:macro:: UNIT_PACK_VERSION_FULL(major, minor, patch, dev)

   Pack a version including the development revision. ``dev=0`` maps
   to ``0xff`` (release).

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      #if UNIT_VERSION_HEX >= UNIT_PACK_VERSION_FULL(0, 2, 0, 3)
          // use feature added in 0.2.0.dev3
      #endif


.. c:function:: const char *UNIT_GetVersion(void)

   Return the version string at runtime.


.. c:function:: uint32_t UNIT_GetVersionHex(void)

   Return the packed version integer at runtime.
