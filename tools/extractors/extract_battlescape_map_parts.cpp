#include "framework/data.h"
#include "framework/framework.h"
#include "framework/palette.h"
#include "game/state/battle/battlemap.h"
#include "game/state/battle/battlemaptileset.h"
#include "library/strings_format.h"
#include "library/voxel.h"
#include "tools/extractors/common/battlemap.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

void InitialGameStateExtractor::readBattleMapParts(GameState &state, TACP &data_t,
                                                   sp<BattleMapTileset> t,
                                                   BattleMapPartType::Type type,
                                                   const UString &idPrefix, const UString &dirName,
                                                   const UString &datName, const UString &pckName,
                                                   const UString &stratPckName)
{
	const UString loftempsFile = "xcom3/tacdata/loftemps.dat";
	const UString loftempsTab = "xcom3/tacdata/loftemps.tab";

	auto datFileName = dirName + "/" + datName + ".dat";
	auto inFile = fw().data->fs.open(datFileName);
	if (!inFile)
	{
		LogError("Failed to open mapunits DAT file at \"%s\"", datFileName.cStr());
		return;
	}
	auto fileSize = inFile.size();
	auto objectCount = fileSize / sizeof(struct BattleMapPartEntry);
	auto firstExitIdx = objectCount - 4;

	auto strategySpriteTabFileName = dirName + "/" + stratPckName + ".tab";
	auto strategySpriteTabFile = fw().data->fs.open(strategySpriteTabFileName);
	if (!strategySpriteTabFile)
	{
		LogError("Failed to open strategy sprite TAB file \"%s\"",
		         strategySpriteTabFileName.cStr());
		return;
	}
	size_t strategySpriteCount = strategySpriteTabFile.size() / 4;

	LogInfo("Loading %zu entries from \"%s\"", objectCount, datFileName.cStr());

	for (size_t i = 0; i < objectCount; i++)
	{

		struct BattleMapPartEntry entry;
		inFile.read((char *)&entry, sizeof(entry));
		if (!inFile)
		{
			LogError("Failed to read entry %zu in \"%s\"", i, datFileName.cStr());
			return;
		}

		UString id = format("%s%u", idPrefix, i);
		auto object = mksp<BattleMapPartType>();
		if (entry.alternative_object_idx != 0)
		{
			object->alternative_map_part = {&state,
			                                format("%s%u", idPrefix, entry.alternative_object_idx)};
		}
		object->type = type;
		object->constitution = entry.constitution;
		object->explosion_power = entry.explosion_power;
		object->explosion_depletion_rate = entry.explosion_depletion_rate;
		object->explosion_type = {&state, data_t.getDModId(entry.explosion_type)};

		object->fire_resist = entry.fire_resist;
		object->fire_burn_time = entry.fire_burn_time;
		object->block_physical = entry.block_physical;
		object->block_gas = entry.block_gas;
		object->block_fire = entry.block_fire;
		object->block_psionic = entry.block_psionic;
		object->size = entry.size;

		object->voxelMapLOF = mksp<VoxelMap>(Vec3<int>{24, 24, 20});
		for (int slice = 0; slice < 20; slice++)
		{
			if ((unsigned int)entry.loftemps_lof[slice] == 0)
				continue;
			auto lofString = format("LOFTEMPS:%s:%s:%u", loftempsFile.cStr(), loftempsTab.cStr(),
			                        (unsigned int)entry.loftemps_lof[slice]);
			object->voxelMapLOF->slices[slice] = fw().data->loadVoxelSlice(lofString);
		}
		object->voxelMapLOS = mksp<VoxelMap>(Vec3<int>{24, 24, 20});
		for (int slice = 0; slice < 20; slice++)
		{
			if ((unsigned int)entry.loftemps_los[slice] == 0)
				continue;
			auto lofString = format("LOFTEMPS:%s:%s:%u", loftempsFile.cStr(), loftempsTab.cStr(),
			                        (unsigned int)entry.loftemps_los[slice]);
			object->voxelMapLOS->slices[slice] = fw().data->loadVoxelSlice(lofString);
		}
		if (entry.damaged_idx)
		{
			object->damaged_map_part = {&state, format("%s%u", idPrefix, entry.damaged_idx)};
		}

		// So far haven't seen an animated object with only 1 frame, but seen objects with 1 in this
		// field that are not actually animated via animated frames, therefore, ignore them
		if (entry.animation_length > 1)
		{
			auto animateTabFileName = dirName + "/" + "animate.tab";
			auto animateTabFile = fw().data->fs.open(animateTabFileName);
			if (!animateTabFile)
			{
				LogError("Failed to open animate sprite TAB file \"%s\"",
				         animateTabFileName.cStr());
				return;
			}
			size_t animateSpriteCount = animateTabFile.size() / 4;

			if (animateSpriteCount < entry.animation_idx + entry.animation_length)
			{
				LogWarning("Bogus animation value, animation frames not present for ID %s",
				           id.cStr());
			}
			else
			{
				for (int j = 0; j < entry.animation_length; j++)
				{
					auto animateString =
					    format("PCK:%s%s.pck:%s%s.tab:%u", dirName.cStr(), "animate",
					           dirName.cStr(), "animate", entry.animation_idx + j);
					object->animation_frames.push_back(fw().data->loadImage(animateString));
				}
			}
		}

		auto imageString = format("PCK:%s%s.pck:%s%s.tab:%u", dirName.cStr(), pckName.cStr(),
		                          dirName.cStr(), pckName.cStr(), i);
		object->sprite = fw().data->loadImage(imageString);
		if (i < strategySpriteCount)
		{
			auto stratImageString =
			    format("PCKSTRAT:%s%s.pck:%s%s.tab:%u", dirName.cStr(), stratPckName.cStr(),
			           dirName.cStr(), stratPckName.cStr(), i);
			object->strategySprite = fw().data->loadImage(stratImageString);
		}
		// It should be {24,34} I guess, since 48/2=24, but 23 gives a little better visual
		// corellation with the sprites
		object->imageOffset = BATTLE_IMAGE_OFFSET;

		object->transparent = entry.transparent == 1;
		object->sfxIndex = entry.sfx - 1;
		object->door = entry.is_door == 1 && entry.is_door_closed == 1;
		object->los_through_terrain = entry.los_through_terrain == 1;
		object->floor = entry.is_floor == 1;
		object->gravlift = entry.is_gravlift == 1;
		object->movement_cost = entry.movement_cost;
		object->height = entry.height;
		object->floating = entry.is_floating;
		object->provides_support = entry.provides_support;
		switch (entry.gets_support_from)
		{
			case 0:
				object->supported_by = BattleMapPartType::SupportedByType::Below;
				break;
			case 1:
				object->supported_by = BattleMapPartType::SupportedByType::North;
				break;
			case 2:
				object->supported_by = BattleMapPartType::SupportedByType::East;
				break;
			case 3:
				object->supported_by = BattleMapPartType::SupportedByType::South;
				break;
			case 4:
				object->supported_by = BattleMapPartType::SupportedByType::West;
				break;
			case 5:
				object->supported_by = BattleMapPartType::SupportedByType::Above;
				break;
			case 7:
				object->supported_by = BattleMapPartType::SupportedByType::Unknown07;
				break;
			case 11:
				object->supported_by = BattleMapPartType::SupportedByType::NorthBelow;
				break;
			case 12:
				object->supported_by = BattleMapPartType::SupportedByType::EastBelow;
				break;
			case 13:
				object->supported_by = BattleMapPartType::SupportedByType::SouthBelow;
				break;
			case 14:
				object->supported_by = BattleMapPartType::SupportedByType::WestBelow;
				break;
			case 20:
				object->supported_by = BattleMapPartType::SupportedByType::Unknown20;
				break;
			case 21:
				object->supported_by = BattleMapPartType::SupportedByType::NorthAbove;
				break;
			case 22:
				object->supported_by = BattleMapPartType::SupportedByType::EastAbove;
				break;
			case 23:
				object->supported_by = BattleMapPartType::SupportedByType::SouthAbove;
				break;
			case 24:
				object->supported_by = BattleMapPartType::SupportedByType::WestAbove;
				break;
			// 30 is a mistake, it should read "1"
			case 30:
				object->supported_by = BattleMapPartType::SupportedByType::North;
				break;
			// 32 is a mistake, it should read "0"
			case 32:
				object->supported_by = BattleMapPartType::SupportedByType::Below;
				break;
			case 36:
				object->supported_by = BattleMapPartType::SupportedByType::WallsNorthWest;
				break;
			case 41:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherNorth;
				break;
			case 42:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherEast;
				break;
			case 43:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherSouth;
				break;
			case 44:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherWest;
				break;
			case 51:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherNorthBelow;
				break;
			case 52:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherEastBelow;
				break;
			case 53:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherSouthBelow;
				break;
			case 54:
				object->supported_by = BattleMapPartType::SupportedByType::AnotherWestBelow;
				break;
			default:
				LogError("Invalid gets_support_from value %d for ID %s", entry.gets_support_from,
				         id.cStr());
				return;
		}
		object->independent_structure = entry.independent_structure;

		if (type == BattleMapPartType::Type::Ground && i >= firstExitIdx)
			object->exit = true;

		t->map_part_types[id] = object;
	}
}

sp<BattleMapTileset> InitialGameStateExtractor::extractTileSet(GameState &state,
                                                               const UString &name)
{
	UString tilePrefix = format("%s_", name.cStr());
	UString map_prefix = "xcom3/maps/";
	UString mapunits_suffix = "/mapunits/";
	UString spriteFile;
	UString datFile;
	UString fileName;

	auto t = mksp<BattleMapTileset>();

	// Read ground (tiles)
	{
		readBattleMapParts(state, this->tacp, t, BattleMapPartType::Type::Ground,
		                   BattleMapPartType::getPrefix() + tilePrefix + "GD_",
		                   map_prefix + name + mapunits_suffix, "grounmap", "ground", "sground");
	}

	// Read left walls
	{
		readBattleMapParts(state, this->tacp, t, BattleMapPartType::Type::LeftWall,
		                   BattleMapPartType::getPrefix() + tilePrefix + "LW_",
		                   map_prefix + name + mapunits_suffix, "leftmap", "left", "sleft");
	}

	// Read right walls
	{
		readBattleMapParts(state, this->tacp, t, BattleMapPartType::Type::RightWall,
		                   BattleMapPartType::getPrefix() + tilePrefix + "RW_",
		                   map_prefix + name + mapunits_suffix, "rightmap", "right", "sright");
	}

	// Read feature
	{
		readBattleMapParts(state, this->tacp, t, BattleMapPartType::Type::Feature,
		                   BattleMapPartType::getPrefix() + tilePrefix + "FT_",
		                   map_prefix + name + mapunits_suffix, "featmap", "feature", "sfeature");
	}

	return t;
}

} // namespace OpenApoc
