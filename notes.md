# 一步步构建自己的LeviLamina插件

LL新手的学习 ~~（踩坑）~~ 笔记

本日志基于 `LeviLamina 0.12.1` 。

## 0. 不要害怕

刚接触新东西的时候总会紧张的，一步步来，相信自己没问题的。

## 1. 设置工作区

建议使用 **VsCode** 开发。

安装VS2022的时候别忘记勾选安装Windows SDK。

如何配置自动补全？我们需要安装clangd插件，然后在项目根目录运行 `xmake project -k compile_commands` 。重启VsCode生效。

## 2. 创建插件仓库

遵循文档操作，直至结束修改README步骤。

接下来就与原文档有所出入了。在最新 *(1ecd534)* 版本的模板中，操作应如下：

1. 按照命名惯例修改 `src/plugin` 文件夹的名字，例如我改成了 `hi_ll_plugin` 。然后进入该文件夹。
2. 貌似 `LeviLamina` 在 `0.11.x` 版本中改动了内存管理相关，故文件夹下会多出 `MemoryOperators.cpp` 文件，不要动它。
3. 重命名 `MyPlugin.h/cpp` ，例如我改成了 `HiLLPlugin.h/cpp` 。
4. 编辑 `HiLLPlugin.h` ，按照惯例，重命名 `my_plugin` 命名空间为之前目录的名字，重命名 `MyPlugin` 类为之前文件的名字。记住类里面的几个声明也要改一下。
5. 编辑 `HiLLPlugin.cpp` ，修改第一行以匹配之前重命名的文件。然后仿照步骤4对文件内容进行修改。注意不要遗漏一些细枝末节。

## 3. 构建你的插件

直接照做就行，没啥坑。

当上游LeviLamina更新时，需要执行 `xmake repo -u` 来进行依赖库更新。若更新失败，可先使用 `xmake c -a` 清除全部缓存。

## 4. 注册指令

首先认识下 `load/unload` 、 `enable/disable` 函数：

+ `load` 在dll插件加载时执行
+ `enable` 在dll插件启用时执行
+ `disable` 在dll插件禁用时执行
+ `unload` 在dll插件卸载时执行

所以正常的流程是： `load -> enable -> (server stop) -> dlsable`，即 `加载 -> 启用 -> 禁用` 。

会发现默认地， `HiLLPlugin.h` 里的 `unload` 函数被注释掉了。不允许手动卸载的插件则不写 `unload` 函数。

手动卸载时，先禁用插件，后执行卸载，故流程为 `load -> enable -> (unload plugin) -> disable -> unload` 。

+ *OEOTYAN：额 （unload里）就反正各种异步的东西全停掉就差不多了*
+ *建议：不会写unload就别写*

我们应在插件启用时注册指令，故内容应放在 `enable` 函数中。

注册指令以及操作Player用到的头文件：

```cpp
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "mc/entity/utilities/ActorType.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Command.h"
```

注册指令时，不需要 `getCommandRegistry()` 操作。故该步骤省略。

lambda函数中，如果要调用控制台输出，应将原版文档中的 `getSelfPluginInstance().getLogger().info("{} killed themselves", player->getRealName());` 改写成 `hi_ll_plugin::HiLLPlugin::getInstance().getSelf().getLogger().info("{} killed themselves", player->getRealName());` 。

以下是一个完整的示例：

```cpp
auto& suicideCommand = ll::command::CommandRegistrar::getInstance().getOrCreateCommand("suicide", "Commits suicide", CommandPermissionLevel::Any);
suicideCommand.overload().execute([](CommandOrigin const& origin, CommandOutput& output){
    auto* entity = origin.getEntity();
    if (entity == nullptr || !entity->isType(ActorType::Player)) {
        output.error("Only players can commit suicide");
        return;
    }

    auto* player = static_cast<Player*>(entity);
    player->kill();

    hi_ll_plugin::HiLLPlugin::getInstance().getSelf().getLogger().info("{} killed themselves", player->getRealName());
});
```

其实很好理解：第一步，获取或创建指令对象（此处行为是创建）。第二步，设置指令重载并设置相应的回调。

LeviLamina不提供手动解注册命令的方式。

按照教程给出的带参数指令示例，我们可以尝试写出一个带参数指令：

```cpp
enum HelloAction: int {hello, hi};
struct HelloParam {
    HelloAction action;
    std::string name;
};
auto& helloCommand = ll::command::CommandRegistrar::getInstance().getOrCreateCommand("hello", "say hello", CommandPermissionLevel::Any);
helloCommand.overload<HelloParam>().required("action").optional("name").execute([](CommandOrigin const& origin, CommandOutput& output, HelloParam const& param){
    auto* entity = origin.getEntity();
    if (entity == nullptr || !entity->isType(ActorType::Player)) {
        output.error("Only players can say hello");
        return;
    }

    auto* player = static_cast<Player*>(entity);

    switch (param.action) {
    case hello:
        if (param.name != "") player->sendMessage("hello! "+param.name);
        else player->sendMessage("hello! player");
        break;
    case hi:
        if (param.name != "") player->sendMessage("hi! "+param.name);
        else player->sendMessage("hi! player");
        break;
    }
    return;
});
```

`struct HelloParam` 中不止能写enum和string，还可以写其他的[参数类型](https://zh.minecraft.wiki/w/%E5%8F%82%E6%95%B0%E7%B1%BB%E5%9E%8B)。

如果你写过脚本插件，会发现理解这些并不困难。首先定义枚举量与命令参数结构体，然后注册命令hello，最后设置指令重载和回调。在这里，我们指定那个枚举量为必选参数，指定字符串为可选参数，并在回调里面分别case。

在这里，我遇到了vscode报错 `In included file: variable 'boost::pfr::detail::fake_object<HelloParam>' is used but not defined in this translation unit, and cannot be defined in any other translation unit because its type does not have linkage` ，但不影响编译。

其他的内容还请参阅开发教程。

## 5. 读取配置文件

按照教程所言，创建 `Config.h` ，并补充头文件：

```cpp
#include "hi_ll_plugin/Config.h"
#include "ll/api/Config.h"
```

`Config.h` 中，struct要写在namespace里，全文如下：

```cpp
namespace hi_ll_plugin {

struct Config {
    int  version          = 1;
    bool doGiveClockOnFirstJoin = true;
    bool enableClockMenu = true;
};

} // namespace hi_ll_plugin
```

然后在 `HiLLPlugin.cpp` 里面新建匿名namespace，并新增成员变量 `config` ，代码如下：

```cpp
namespace {

hi_ll_plugin::Config config;

}
```

然后编辑 `load` 函数，读取配置文件并将配置信息保存到成员变量中：

```cpp
bool HiLLPlugin::load() {
    const auto& logger = getSelf().getLogger();
    logger.info("Loading...");
    // Code for loading the plugin goes here.

    // Load or initialize configurations
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        logger.warn("Cannot load configurations from {}", configFilePath);
        logger.info("Saving default configurations");

        if (!ll::config::saveConfig(config, configFilePath)) {
            logger.error("Cannot save default configurations to {}", configFilePath);
        }
    }
    return true;
}
```

## 6. 将玩家进服信息持久化保存在数据库中

首先补充数据库相关头文件：

```cpp
#include "ll/api/data/KeyValueDB.h"
```

然后在之前写 `config` 的那个匿名namespace里面增加一个成员变量，用于保存数据库实例。

```cpp
std::unique_ptr<ll::data::KeyValueDB> playerDb;
```

注意阅读文档的tips。

在 `load` 函数中新增初始化数据库实例的代码：

```cpp
// Initialize databases;
const auto& playerDbPath = getSelf().getDataDir() / "players";
playerDb                 = std::make_unique<ll::data::KeyValueDB>(playerDbPath);
```

## 7. 玩家首次进服时，给予一个钟

补充event监听头文件：

```cpp
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
```

以及接下来用到的事件的头文件：

```cpp
#include "ll/api/event/player/PlayerJoinEvent.h"
```

以及操作玩家物品栏的头文件：

```cpp
#include "mc/world/item/registry/ItemStack.h"
```

然后在之前写 `config` 的那个匿名namespace里面再增加一个事件监听器指针：

```cpp
ll::event::ListenerPtr playerJoinEventListener;
```

在 `enable()` 中注册这个事件监听器，并在 `disable()` 中取消注册。这一段没什么坑，可以完全照着教程写（除了 `logger` 用 `const auto& logger = getSelf().getLogger();` 定义一下）。建议仔细阅读文档，理解监听和数据库的相关操作。

## 8. 使用钟的时候，弹出确认自杀的提示

补充监听事件与表单的头文件：

```cpp
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/form/ModalForm.h"
```

然后大部分内容按照教程学习就行了，和上一个章节的内容很像。唯一要注意的就是表单的构建，与教程中的不一样。

首先， `ll::form::ModalForm` 接受的参数变成了4个，分别是表单标题，表单内容，第一个（Upper）按钮，第二个（Lower）按钮。

那么如何设置callback呢？答案是在调用 `sendTo` 时。代码如下：

```cpp
// player use item event
playerUseItemEventListener =
    eventBus.emplaceListener<ll::event::PlayerUseItemEvent>(
        [enableClockMenu = config.enableClockMenu,
        &logger](ll::event::PlayerUseItemEvent& event) {
        if (enableClockMenu) {
            auto& player    = event.self();
            auto& itemStack = event.item();

            if (itemStack.getRawNameId() == "clock") {
                ll::form::ModalForm form(
                    "Warning",
                    "Are you sure you want to kill yourself?",
                    "Yes",
                    "No"
                );

                form.sendTo(player, [&logger](Player& player, auto button, auto reason){
                    (void)reason;
                    if (button == ll::form::ModalFormSelectedButton::Upper) {
                        player.kill();
                        logger.info("{} killed themselves", player.getRealName());
                    }
                });
            }
        }
    });
```

这里， `(void)reason;` 用于消除reason参数未使用的警告。

## 9. 构建插件

参阅原教程“构建你的插件”章节。

版本号会自动获取当前分支的 `git tag` （例如 `v0.0.1` ）。获取失败时，会使用默认的 `v0.0.0` 。

## 10. 结语

本文记录了一个萌新入门的脚步。在此感谢LL交流群中做出帮助的各位大佬们。祝愿LL社区越来越好！
