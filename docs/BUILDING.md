# Building Metadata Isolation

The supported source build targets are Windows Win32 and x64, Release
configuration, using the Visual Studio v145 C++ toolchain and C++23.
The repository includes the required foobar2000 SDK and WTL source/headers
under their upstream licenses.

GitHub Actions FAST runs project validation, compile and deterministic static
safety contracts. The explicit manual candidate workflow additionally packages
and hashes the combined component with x86 DLL at the archive root and x64 DLL
under `x64/`. Production foobar2000 runtime acceptance is a separate gate.

The user's local foobar2000 installation, profile backups, paths and OAuth
configurations are **not** public build prerequisites and are not included.