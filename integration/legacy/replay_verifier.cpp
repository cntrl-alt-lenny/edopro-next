#include "replay_verifier.h"

#include "client_field.h"
#include "data_manager.h"
#include "duelclient.h"
#include "game.h"
#include "game_config.h"
#include "materials.h"
#include "replay.h"
#include "sound_manager.h"
#include "utils.h"

#include <irrlicht.h>

#include <cstdio>
#include <iostream>
#include <memory>

namespace edopro_next::legacy_observer {
namespace {

ReplayVerificationStats* g_active_stats = nullptr;

} // namespace

void set_active_verification_stats(ReplayVerificationStats* stats) noexcept {
	g_active_stats = stats;
}

ReplayVerificationStats* get_active_verification_stats() noexcept {
	return g_active_stats;
}

ReplayVerificationStats verify_replay(const std::string& path, bool inject_fault) {
	std::fprintf(stderr, "[debug] Starting verify_replay: %s\n", path.c_str());
	std::fflush(stderr);
	ReplayVerificationStats stats;
	stats.fixture_name = path;

	ygo::Replay replay;
	const auto replay_path = ygo::Utils::ToPathString(path);
	if(!replay.OpenReplay(replay_path)) {
		std::fprintf(stderr, "error: failed to open replay fixture '%s'\n", path.c_str());
		std::fflush(stderr);
		return stats;
	}
	std::fprintf(stderr, "[debug] OpenReplay succeeded: %zu packets\n", replay.packets_stream.size());
	std::fflush(stderr);

	ygo::Game game{};
	ygo::mainGame = &game;
	ygo::DataManager dataManager{};
	ygo::gDataManager = &dataManager;
	ygo::GameConfig config{};
	ygo::gGameConfig = &config;
	ygo::SoundManager soundManager(0.0, 0.0, false, false, ygo::SoundManager::BACKEND::NONE);
	ygo::gSoundManager = &soundManager;

	irr::SIrrlichtCreationParameters params{};
	params.DriverType = irr::video::EDT_NULL;
	params.WindowSize = {1024, 640};
	auto* dev = irr::createDeviceEx(params);
	std::fprintf(stderr, "[debug] createDeviceEx returned: %p\n", static_cast<void*>(dev));
	std::fflush(stderr);
	if(dev != nullptr) {
		ygo::Utils::irrTimer = dev->getTimer();
		game.device = std::shared_ptr<irr::IrrlichtDevice>(dev, [](irr::IrrlichtDevice* d) { d->drop(); });
		game.driver = dev->getVideoDriver();
		game.env = dev->getGUIEnvironment();
		if(game.env != nullptr) {
			game.wCmdMenu = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wPhase = game.env->addStaticText(L"", irr::core::recti(0, 0, 10, 10));
			game.lstLog = game.env->addListBox(irr::core::recti(0, 0, 10, 10));
			game.stHintMsg = game.env->addStaticText(L"", irr::core::recti(0, 0, 10, 10));
			game.stACMessage = game.env->addStaticText(L"", irr::core::recti(0, 0, 10, 10));
			game.stMessage = game.env->addStaticText(L"", irr::core::recti(0, 0, 10, 10));
			game.stQMessage = game.env->addStaticText(L"", irr::core::recti(0, 0, 10, 10));
			game.wACMessage = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wMessage = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wHand = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wPosSelect = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wCardSelect = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wOptions = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wFTSelect = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wQuery = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wANCard = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wANNumber = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wANRace = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.wANAttribute = game.env->addWindow(irr::core::recti(0, 0, 10, 10));
			game.btnDP = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnSP = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnM1 = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnBP = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnM2 = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnEP = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnCancelOrFinish = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnChainAlways = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnChainIgnore = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnChainWhenAvail = game.env->addButton(irr::core::recti(0, 0, 10, 10));
			game.btnShuffle = game.env->addButton(irr::core::recti(0, 0, 10, 10));
		}
	}

	const auto& replay_header = replay.pheader;
	ygo::mainGame->dInfo.isReplay = true;
	ygo::mainGame->dInfo.isFirst = true;
	ygo::mainGame->dInfo.isTeam1 = true;
	ygo::mainGame->dInfo.isRelay = !!(replay.params.duel_flags & DUEL_RELAY);
	ygo::mainGame->dInfo.isSingleMode = !!(replay_header.base.flag & REPLAY_SINGLE_MODE);
	ygo::mainGame->dInfo.isHandTest = !!(replay_header.base.flag & REPLAY_HAND_TEST);
	ygo::mainGame->dInfo.compat_mode = !(replay_header.base.flag & REPLAY_LUA64);
	ygo::mainGame->dInfo.legacy_race_size = GET_CORE_VERSION_MAJOR(replay_header.base.version) < 10;
	ygo::mainGame->dInfo.team1 = replay.GetPlayersCount(0);
	ygo::mainGame->dInfo.team2 = replay.GetPlayersCount(1);
	ygo::mainGame->dInfo.current_player[0] = 0;
	ygo::mainGame->dInfo.current_player[1] = 0;
	if(!ygo::mainGame->dInfo.isRelay)
		ygo::mainGame->dInfo.current_player[1] = ygo::mainGame->dInfo.team2 - 1;
	const auto& names = replay.GetPlayerNames();
	const auto first_oppo = names.begin() + ygo::mainGame->dInfo.team1;
	ygo::mainGame->dInfo.selfnames.assign(names.begin(), first_oppo);
	ygo::mainGame->dInfo.opponames.assign(first_oppo, names.end());
	ygo::mainGame->dInfo.duel_params = replay.params.duel_flags;
	ygo::mainGame->dInfo.duel_field = ygo::mainGame->GetMasterRule(ygo::mainGame->dInfo.duel_params);
	ygo::matManager.SetActiveVertices(ygo::mainGame->dInfo.HasFieldFlag(DUEL_3_COLUMNS_FIELD),
	                                  !ygo::mainGame->dInfo.HasFieldFlag(DUEL_SEPARATE_PZONE));
	ygo::mainGame->dInfo.turn = 0;
	ygo::mainGame->dInfo.isCatchingUp = false;
	ygo::mainGame->dInfo.isInDuel = true;
	ygo::mainGame->dInfo.isStarted = true;

	std::fprintf(stderr, "[debug] Setup complete, starting packet loop...\n");
	std::fflush(stderr);

	set_active_verification_stats(&stats);

	for(std::size_t i = 0; i < replay.packets_stream.size(); ++i) {
		const auto& packet = replay.packets_stream[i];
		std::fprintf(stderr, "[debug] packet %zu: msg=%u len=%zu\n", i, static_cast<unsigned>(packet.message), packet.buff_size());
		std::fflush(stderr);
		ygo::mainGame->dInfo.curMsg = packet.message;
		if(inject_fault && i == 50) {
			// Fault injection: alter legacy LP artificially to prove verifier catches mismatches
			ygo::mainGame->dInfo.lp[0] += 500;
		}
		ygo::DuelClient::ClientAnalyze(packet);
	}
	std::fprintf(stderr, "[debug] Packet loop finished successfully!\n");
	std::fflush(stderr);

	set_active_verification_stats(nullptr);
	ygo::mainGame = nullptr;
	ygo::gDataManager = nullptr;
	ygo::gGameConfig = nullptr;
	ygo::gSoundManager = nullptr;
	stats.completed = true;
	return stats;
}

int verify_replay_cli(const std::string& path, bool inject_fault) {
	const auto stats = verify_replay(path, inject_fault);
	std::cout << "fixture: " << path << "\n";
	std::cout << "packets processed: " << stats.packets_processed << "\n";
	std::cout << "semantic decode failures: " << stats.decode_failures << "\n";
	std::cout << "legacy/semantic mismatches: " << stats.mismatches.size() << "\n";
	if(stats.equivalent()) {
		std::cout << "result: equivalent\n";
		return 0;
	} else {
		std::cout << "result: mismatch\n";
		for(const auto& mismatch : stats.mismatches) {
			std::cerr << mismatch.format() << "\n";
		}
		return 1;
	}
}

} // namespace edopro_next::legacy_observer

extern "C" int edopro_next_verify_replay_cli(const char* path) noexcept {
	if(path == nullptr)
		return 1;
	try {
		return edopro_next::legacy_observer::verify_replay_cli(std::string(path));
	} catch(const std::exception& e) {
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	} catch(...) {
		std::fprintf(stderr, "error: unknown exception\n");
		return 1;
	}
}
