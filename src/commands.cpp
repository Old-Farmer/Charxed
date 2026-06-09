#include "constants.h"
#include "editor.h"
#include "version.h"

namespace charxed {

namespace {
constexpr std::string_view kSmile = R"(
      _____
   .-'     '-.
  /  _   _    \
 |  (o) (o)    |
 |      ^      |
 |   \_____/   |
  \           /
   '-._____.-'
)";
}

#define CHX_CMD command_manager_.AddCommand
void Editor::InitCommands() {
#define CHX_ENSURE_ARGEXITS(v) CHX_ASSERT(args[v].has_value())
    CHX_CMD({"quit", "q", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 Quit(false);
             }});
    CHX_CMD({"quit!", "q!", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 Quit(true);
             }});
    CHX_CMD({"help",
             "h",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 EnsureInEditorContext();
                 if (!args[0].has_value()) {
                     Help(kHelpDoc);
                 } else {
                     Help(std::get<std::string>(*args[0]));
                 }
             },
             1,
             1});
    CHX_CMD({"saveas",
             "sa",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 if (context_ != Context::kEditor) return;
                 CHX_ENSURE_ARGEXITS(0);
                 SaveCurrentBufferAs(Path(std::get<std::string>(*args[0])));
             },
             1});
    CHX_CMD({"edit",
             "e",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 CHX_ENSURE_ARGEXITS(0);
                 EnsureInEditorContext();
                 Edit(std::get<std::string>(*args[0]));
             },
             1});
    CHX_CMD({"buffer",
             "b",
             "",
             {Type::kString},
             [this](const CommandArgs& args) {
                 CHX_ENSURE_ARGEXITS(0);
                 EnsureInEditorContext();
                 const std::string& name_str = std::get<std::string>(*args[0]);
                 Buffer* b = buffer_manager_->FindBuffer(name_str);
                 if (b) {
                     cursor_.t_win->AttachBuffer(b);
                 }
             },
             1});
    CHX_CMD({"bdelete", "bd", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 if (context_ != Context::kEditor) return;
                 RemoveCurrentBuffer();
             }});
    CHX_CMD({"smile", "", "", {}, [this](const CommandArgs& args) {
                 (void)args;
                 NotifyUser(kSmile);
             }});
    CHX_CMD({"about", "", "", {Type::kString}, [this](const CommandArgs& args) {
                 (void)args;
                 NotifyUser(kVersionInfo);
             }});
#undef CHX_ENSURE_ARGEXITS
}

}  // namespace charxed
