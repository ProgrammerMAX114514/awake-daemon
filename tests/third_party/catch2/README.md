# Catch2 (vendored)

`catch.hpp` is the Catch2 v2.13.10 **single-header** distribution, taken
unchanged from

    https://github.com/catchorg/Catch2/releases/tag/v2.13.10
    single_include/catch2/catch.hpp

It is vendored (instead of fetched by CMake) so that configuring and building
the tests never needs network access - CI and an offline developer machine
behave the same way.

Do not edit `catch.hpp`. To upgrade, replace the file and `LICENSE.txt` with a
newer v2 release and update the version above.

License: Boost Software License 1.0, see `LICENSE.txt` in this directory.
