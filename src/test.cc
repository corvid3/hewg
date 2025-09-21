#include <filesystem>

#include <crow.scl/scl.hh>

#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "thread_pool.hh"

struct CTestDescriptor
{
  int std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields =
    std::tuple<scl::field<&CTestDescriptor::std, "std">,
               scl::field<&CTestDescriptor::flags, "flags">,
               scl::field<&CTestDescriptor::sources, "sources">>;
  using scl_recurse = std::tuple<>;
};

struct CXXTestDescriptor
{
  int std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields =
    std::tuple<scl::field<&CTestDescriptor::std, "std">,
               scl::field<&CTestDescriptor::flags, "flags">,
               scl::field<&CTestDescriptor::sources, "sources">>;
  using scl_recurse = std::tuple<>;
};

struct TestDescriptor
{
  std::string name;
  CTestDescriptor c;
  CXXTestDescriptor cxx;
  std::vector<std::string> depends;

  using scl_fields = std::tuple<>;
  using scl_recurse =
    std::tuple<scl::field<&TestDescriptor::c, "c">,
               scl::field<&TestDescriptor::cxx, "cxx">,
               scl::field<&TestDescriptor::depends, "depends">>;
};

static TestDescriptor
read_test_descriptor(std::filesystem::path const& test_dir)
{
  auto const what = read_file(test_dir / "test.scl");
  scl::file file(what);
  TestDescriptor desc;
  scl::deserialize(desc, file, "test");
  return desc;
}

// returns a path to the built executable
static std::filesystem::path
build_test(ThreadPool& threads,
           TestDescriptor const& descriptor,
           std::filesystem::path const& test_dir)
{
  auto const cxx_src_dir = test_dir / "src";
  auto const c_src_dir = test_dir / "csrc";
  auto const include_dir = test_dir / "include";
  auto const resources_dir = test_dir / "resources";

  compile_c(threads, config, , , , , , , )
}

static void
run_test(ConfigurationFile const& config, std::filesystem::path const& test_dir)
{
  auto const desc = read_test_descriptor(test_dir);
}

void
test_all(ConfigurationFile const& config)
{
  std::filesystem::directory_iterator begin(std::filesystem::current_path() /
                                            "tests"),
    end;

  std::vector<std::filesystem::path> test_directories;

  for (; begin != end; begin++) {
    if (not begin->is_directory())
      continue;

    auto const& path = begin->path();
    if (path.filename() == "include" or path.filename() == "sharedResources")
      continue;

    test_directories.push_back(path);
  }
}
