// hewg's source code relies on these extern variables, which are is created
// by hewg when running the build command. the bootstrap makefile will compile
// this file so the first build of hewg will at least work,
// then subsequent rebuilds of hewg with itself will
// manage its own hewg version variables that overrides
// this declaration in the linking stage
__attribute__((weak)) int __hewg_version_package_hewg[3] = { 0, 0, 0 };
__attribute__((weak)) char const* __hewg_prerelease_package_hewg =
  (char const*)0;
__attribute__((weak)) char const* __hewg_metadata_package_hewg = (char const*)0;

// this isn't automated, we just pass it a fake
// timestamp; when hewg rebuilds itself it'll generate the
// correct timestamp
__attribute__((weak)) long __hewg_build_date_package_hewg = 0;
