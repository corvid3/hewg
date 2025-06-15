// hewg's source code relies on this extern variable, which itself is created by
// hewg when running the build command. the bootstrap makefile will compile
// this file so the first build of hewg will at least work,
// then subsequent rebuilds of hewg with itself will
// create its own hewg version variable that overrides
// this declaration in the linking stage
__attribute__((weak)) int __hewg_version[3] = { 0, 0, 0 };
