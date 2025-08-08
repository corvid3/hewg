

#include <jayson.hh>
#include <string>
#include <vector>

struct CompileCommand
{
  std::string directory;
  std::vector<std::string> arguments;
  std::string file;

  using jayson_fields =
    std::tuple<jayson::obj_field<"directory", &CompileCommand::directory>,
               jayson::obj_field<"arguments", &CompileCommand::arguments>,
               jayson::obj_field<"file", &CompileCommand::file>>;
};

std::string
serialize_compile_commands(std::vector<CompileCommand> const& commands);