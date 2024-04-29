#include "hi_ll_plugin/HiLLPlugin.h"

#include <memory>

#include "hi_ll_plugin/Config.h"

#include "ll/api/plugin/NativePlugin.h"
#include "ll/api/plugin/RegisterHelper.h"

#include "ll/api/Config.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/data/KeyValueDB.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/form/ModalForm.h"
#include "mc/entity/utilities/ActorType.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/registry/ItemStack.h"
#include "mc/world/level/Command.h"

#include <string>
/*
#include <fmt/format.h>
#include <functional>
#include <ll/api/command/Command.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/plugin/PluginManagerRegistry.h>
#include <ll/api/service/Bedrock.h>
#include <stdexcept>
*/

namespace {

hi_ll_plugin::Config config;

std::unique_ptr<ll::data::KeyValueDB> playerDb;

ll::event::ListenerPtr playerJoinEventListener;
ll::event::ListenerPtr playerUseItemEventListener;

} // namespace

namespace hi_ll_plugin {

static std::unique_ptr<HiLLPlugin> instance;

HiLLPlugin& HiLLPlugin::getInstance() { return *instance; }

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

    // Initialize databases;
    const auto& playerDbPath = getSelf().getDataDir() / "players";
    playerDb                 = std::make_unique<ll::data::KeyValueDB>(playerDbPath);

    return true;
}

bool HiLLPlugin::enable() {
    const auto& logger = getSelf().getLogger();
    getSelf().getLogger().info("Enabling...");
    // Code for enabling the plugin goes here.

    // suicide command
    auto& suicideCommand = ll::command::CommandRegistrar::getInstance()
                               .getOrCreateCommand("suicide", "Commits suicide", CommandPermissionLevel::Any);
    suicideCommand.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (entity == nullptr || !entity->isType(ActorType::Player)) {
            output.error("Only players can commit suicide");
            return;
        }

        auto* player = static_cast<Player*>(entity);
        player->kill();

        hi_ll_plugin::HiLLPlugin::getInstance().getSelf().getLogger().info(
            "{} killed themselves",
            player->getRealName()
        );
    });

    // hello command
    enum HelloAction : int { hello, hi };
    struct HelloParam {
        HelloAction action;
        std::string name;
    };
    auto& helloCommand = ll::command::CommandRegistrar::getInstance()
                             .getOrCreateCommand("hello", "say hello", CommandPermissionLevel::Any);
    helloCommand.overload<HelloParam>().required("action").optional("name").execute(
        [](CommandOrigin const& origin, CommandOutput& output, HelloParam const& param) {
            auto* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player)) {
                output.error("Only players can say hello");
                return;
            }

            auto* player = static_cast<Player*>(entity);

            switch (param.action) {
            case hello:
                if (param.name != "") player->sendMessage("hello! " + param.name);
                else player->sendMessage("hello! player");
                break;
            case hi:
                if (param.name != "") player->sendMessage("hi! " + param.name);
                else player->sendMessage("hi! player");
                break;
            }
            return;
        }
    );

    // player join event
    auto& eventBus          = ll::event::EventBus::getInstance();
    playerJoinEventListener = eventBus.emplaceListener<ll::event::player::PlayerJoinEvent>(
        [doGiveClockOnFirstJoin = config.doGiveClockOnFirstJoin,
         &logger,
         &playerDb = playerDb](ll::event::player::PlayerJoinEvent& event) {
            if (doGiveClockOnFirstJoin) {
                auto&       player = event.self();
                const auto& uuid   = player.getUuid();

                // check if the player has joined before
                if (!playerDb->get(uuid.asString())) {

                    ItemStack itemStack("clock", 1);
                    player.add(itemStack);

                    // must refresh inventory to see the clock
                    player.refreshInventory();

                    // mark the player as joined
                    if (!playerDb->set(uuid.asString(), "true")) {
                        logger.error("Cannot mark {} as joined in database", player.getRealName());
                    }

                    logger.info("First join of {}! Giving them a clock", player.getRealName());
                }
            }
        }
    );

    // player use item event
    playerUseItemEventListener =
        eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([enableClockMenu = config.enableClockMenu,
                                                                 &logger](ll::event::PlayerUseItemEvent& event) {
            if (enableClockMenu) {
                auto& player    = event.self();
                auto& itemStack = event.item();

                if (itemStack.getRawNameId() == "clock") {
                    ll::form::ModalForm form("Warning", "Are you sure you want to kill yourself?", "Yes", "No");

                    form.sendTo(player, [&logger](Player& player, auto button, auto reason) {
                        (void)reason;
                        if (button == ll::form::ModalFormSelectedButton::Upper) {
                            player.kill();
                            logger.info("{} killed themselves", player.getRealName());
                        }
                    });
                }
            }
        });
    return true;
}

bool HiLLPlugin::disable() {
    getSelf().getLogger().info("Disabling...");
    // Code for disabling the plugin goes here.

    auto& eventBus = ll::event::EventBus::getInstance();
    eventBus.removeListener(playerJoinEventListener);
    eventBus.removeListener(playerUseItemEventListener);
    return true;
}

} // namespace hi_ll_plugin

LL_REGISTER_PLUGIN(hi_ll_plugin::HiLLPlugin, hi_ll_plugin::instance);
