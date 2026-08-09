$ErrorActionPreference = 'Stop'

cmake -B build -S . `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' `
  -DBUILD_TESTING=ON `
  -DCMAKE_PREFIX_PATH="$env:DAWN_PREFIX;$env:VCPKG_ROOT/installed/x64-windows" `
  -DDawn_DIR="$env:Dawn_DIR" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DFREETYPE_INCLUDE_DIR_ft2build="$env:VCPKG_ROOT/installed/x64-windows/include/freetype2" `
  -DFREETYPE_INCLUDE_DIR_freetype2="$env:VCPKG_ROOT/installed/x64-windows/include/freetype2" `
  -DFREETYPE_LIBRARY_RELEASE="$env:VCPKG_ROOT/installed/x64-windows/lib/freetype.lib" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

cmake --build build --target woki_tests woki_extensions studio -j 4
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

ctest --test-dir build --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}
