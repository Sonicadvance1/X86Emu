#include "Core/Core.h"
#include "ELFLoader.h"
#include "LogManager.h"

void MsgHandler(LogMan::DebugLevels Level, std::string const &Message) {
  const char *CharLevel{nullptr};

  switch (Level) {
  case LogMan::NONE:
    CharLevel = "NONE";
    break;
  case LogMan::ASSERT:
    CharLevel = "ASSERT";
    break;
  case LogMan::ERROR:
    CharLevel = "ERROR";
    break;
  case LogMan::DEBUG:
    CharLevel = "DEBUG";
    break;
  case LogMan::INFO:
    CharLevel = "Info";
    break;
  default:
    CharLevel = "???";
    break;
  }
  printf("[%s] %s\n", CharLevel, Message.c_str());
}

void AssertHandler(std::string const &Message) {
  printf("[ASSERT] %s\n", Message.c_str());
}

int main(int argc, char **argv) {
  LogMan::Throw::InstallHandler(AssertHandler);
  LogMan::Msg::InstallHandler(MsgHandler);

  LogMan::Throw::A(argc > 1, "Not enough arguments");

  Emu::Core localCore;
  bool Result = localCore.Load(argv[1], {});
  printf("Managed to load? %s\n", Result ? "Yes" : "No");

  return 0;
}
