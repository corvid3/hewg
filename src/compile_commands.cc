#include "compile_commands.hh"
#include <jayson.hh>


std::string
serialize_compile_commands(std::vector<CompileCommand> commands)
{
    return jayson::serialize(commands).serialize(true);
}