#pragma once

#include "gamerules.h"

// Central mode identification for BRIG's existing CGameRules hierarchy.
// Check Co-op before Deathmatch: CHalfLifeMultiplay::IsDeathmatch() currently
// returns true even when its IsCoOp() reports that the coop cvar is enabled.
enum class BrigGameMode
{
	SinglePlayer,
	CoOp,
	Deathmatch,
	TeamDeathmatch
};

struct BrigGameModeFeatures
{
	bool multiplayer;
	bool playerRespawn;
	bool friendlyPlayerRelationships;
	bool playerVsPlayerDamage;
	bool lockWeaponInputWhileBarnacled;
};

inline BrigGameMode BrigResolveGameMode(const CGameRules& rules)
{
	if (rules.IsCoOp())
	{
		return BrigGameMode::CoOp;
	}

	if (!rules.IsDeathmatch())
	{
		return BrigGameMode::SinglePlayer;
	}

	return rules.IsTeamplay() ? BrigGameMode::TeamDeathmatch : BrigGameMode::Deathmatch;
}

inline BrigGameModeFeatures BrigGetGameModeFeatures(BrigGameMode mode)
{
	switch (mode)
	{
	case BrigGameMode::CoOp:
		return {true, true, true, false, true};

	case BrigGameMode::Deathmatch:
		return {true, true, false, true, false};

	case BrigGameMode::TeamDeathmatch:
		return {true, true, true, true, false};

	case BrigGameMode::SinglePlayer:
	default:
		return {false, true, false, false, false};
	}
}
