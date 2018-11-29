#pragma once
#include <string>
#include <vector>

namespace Emu::IR {
class PassManager;

class Pass {
public:
  virtual std::string GetName() = 0;

protected:
friend PassManager;
  Pass() {}
  virtual void Run() = 0;
};

class BlockPass : public Pass {
public:

private:
  virtual void Run() override final { RunOnBlock(); }
  virtual void RunOnBlock() = 0;
};

class FunctionPass : public Pass {
public:

private:
  virtual void Run() override final { RunOnFunction(); }
  virtual void RunOnFunction() = 0;
};

class PassManager {
public:
  virtual void Run() = 0;
  void AddPass(Pass* pass) { passes.emplace_back(); }

private:
  std::vector<Pass*> passes;
};

class BlockPassManager final : public PassManager {
public:
  void Run();
};

class FunctionPassManager final : public PassManager {
public:
  void Run();
};
}
