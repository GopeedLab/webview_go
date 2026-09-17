#include "../libs/webview/include/webview.h"
#include <memory>
class test_engine : public webview::detail::engine_base {
 public:
  test_engine(std::vector<std::function<void()>> &tasks, int &evaluations)
      : tasks(tasks), evaluations(evaluations) {}
  void close() { cancel_pending_resolves(); }
 private:
  void navigate_impl(const std::string &) override {}
  void *window_impl() override { return nullptr; }
  void *widget_impl() override { return nullptr; }
  void *browser_controller_impl() override { return nullptr; }
  void run_impl() override {}
  void terminate_impl() override {}
  void dispatch_impl(std::function<void()> fn) override { tasks.push_back(fn); }
  void set_title_impl(const std::string &) override {}
  void set_size_impl(int, int, webview_hint_t) override {}
  void set_html_impl(const std::string &) override {}
  void set_user_agent_impl(const std::string &) override {}
  void init_impl(const std::string &) override {}
  void eval_impl(const std::string &) override { ++evaluations; }
  std::vector<std::function<void()>> &tasks;
  int &evaluations;
};
int main() {
  std::vector<std::function<void()>> tasks;
  int evaluations = 0;
  auto engine = std::unique_ptr<test_engine>(new test_engine(tasks, evaluations));
  // Live bindings must still resolve normally.
  engine->resolve("1", 0, "null");
  tasks.front()();
  tasks.clear();
  assert(evaluations == 1);

  // Destruction may pump the event loop before the C++ object is freed.
  engine->resolve("2", 0, "null");
  engine->close();
  tasks.front()();
  tasks.clear();
  assert(evaluations == 1);

  // A shared event loop may run queued binding responses after deletion.
  engine.reset(new test_engine(tasks, evaluations));
  engine->resolve("3", 0, "null");
  engine.reset();
  tasks.front()();
  assert(evaluations == 1);
}
