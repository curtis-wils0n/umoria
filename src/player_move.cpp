// Copyright (c) 1981-86 Robert A. Koeneke
// Copyright (c) 1987-94 James E. Wilson
//
// This work is free software released under the GNU General Public License
// version 2.0, and comes with ABSOLUTELY NO WARRANTY.
//
// See LICENSE and AUTHORS for more information.

// Game initialization and maintenance related functions

#include "headers.h"

static void trapOpenPit(Inventory_t const &item, int dam) {
    printMessage("You fell into a pit!");

    if (py.flags.free_fall) {
        printMessage("You gently float down.");
        return;
    }

    obj_desc_t description = {'\0'};
    itemDescription(description, item, true);
    playerTakesHit(dam, description);
}

static void trapArrow(Inventory_t const &item, int dam) {
    if (playerTestBeingHit(125, 0, 0, py.misc.ac + py.misc.magical_ac, CLASS_MISC_HIT)) {
        obj_desc_t description = {'\0'};
        itemDescription(description, item, true);
        playerTakesHit(dam, description);

        printMessage("An arrow hits you.");
        return;
    }

    printMessage("An arrow barely misses you.");
}

static void trapCoveredPit(Inventory_t const &item, int dam, Coord_t coord) {
    printMessage("You fell into a covered pit.");

    if (py.flags.free_fall) {
        printMessage("You gently float down.");
    } else {
        obj_desc_t description = {'\0'};
        itemDescription(description, item, true);
        playerTakesHit(dam, description);
    }

    dungeonSetTrap(coord, 0);
}

static void trapDoor(Inventory_t const &item, int dam) {
    dg.generate_new_level = true;
    dg.current_level++;

    printMessage("You fell through a trap door!");

    if (py.flags.free_fall) {
        printMessage("You gently float down.");
    } else {
        obj_desc_t description = {'\0'};
        itemDescription(description, item, true);
        playerTakesHit(dam, description);
    }

    // Force the messages to display before starting to generate the next level.
    printMessage(CNIL);
}

static void trapSleepingGas() {
    if (py.flags.paralysis != 0) {
        return;
    }

    printMessage("A strange white mist surrounds you!");

    if (py.flags.free_action) {
        printMessage("You are unaffected.");
        return;
    }

    py.flags.paralysis += randomNumber(10) + 4;
    printMessage("You fall asleep.");
}

static void trapHiddenObject(Coord_t coord) {
    (void) dungeonDeleteObject(coord);

    dungeonPlaceRandomObjectAt(coord, false);

    printMessage("Hmmm, there was something under this rock.");
}

static void trapStrengthDart(Inventory_t const &item, int dam) {
    if (playerTestBeingHit(125, 0, 0, py.misc.ac + py.misc.magical_ac, CLASS_MISC_HIT)) {
        if (!py.flags.sustain_str) {
            (void) playerStatRandomDecrease(py_attrs::A_STR);

            obj_desc_t description = {'\0'};
            itemDescription(description, item, true);
            playerTakesHit(dam, description);

            printMessage("A small dart weakens you!");
        } else {
            printMessage("A small dart hits you.");
        }
    } else {
        printMessage("A small dart barely misses you.");
    }
}

static void trapTeleport(Coord_t coord) {
    teleport_player = true;

    printMessage("You hit a teleport trap!");

    // Light up the teleport trap, before we teleport away.
    dungeonMoveCharacterLight(coord, coord);
}

static void trapRockfall(Coord_t coord, int dam) {
    playerTakesHit(dam, "a falling rock");

    (void) dungeonDeleteObject(coord);
    dungeonPlaceRubble(coord);

    printMessage("You are hit by falling rock.");
}

static void trapCorrodeGas() {
    printMessage("A strange red gas surrounds you.");

    damageCorrodingGas("corrosion gas");
}

static void trapSummonMonster(Coord_t coord) {
    // Rune disappears.
    (void) dungeonDeleteObject(coord);

    int num = 2 + randomNumber(3);

    Coord_t location = Coord_t{0, 0};

    for (int i = 0; i < num; i++) {
        location.y = coord.y;
        location.x = coord.x;
        (void) monsterSummon(location, false);
    }
}

static void trapFire(int dam) {
    printMessage("You are enveloped in flames!");

    damageFire(dam, "a fire trap");
}

static void trapAcid(int dam) {
    printMessage("You are splashed with acid!");

    damageAcid(dam, "an acid trap");
}

static void trapPoisonGas(int dam) {
    printMessage("A pungent green gas surrounds you!");

    damagePoisonedGas(dam, "a poison gas trap");
}

static void trapBlindGas() {
    printMessage("A black gas surrounds you!");

    py.flags.blind += randomNumber(50) + 50;
}

static void trapConfuseGas() {
    printMessage("A gas of scintillating colors surrounds you!");

    py.flags.confused += randomNumber(15) + 15;
}

static void trapSlowDart(Inventory_t const &item, int dam) {
    if (playerTestBeingHit(125, 0, 0, py.misc.ac + py.misc.magical_ac, CLASS_MISC_HIT)) {
        obj_desc_t description = {'\0'};
        itemDescription(description, item, true);
        playerTakesHit(dam, description);

        printMessage("A small dart hits you!");

        if (py.flags.free_action) {
            printMessage("You are unaffected.");
        } else {
            py.flags.slow += randomNumber(20) + 10;
        }
    } else {
        printMessage("A small dart barely misses you.");
    }
}

static void trapConstitutionDart(Inventory_t const &item, int dam) {
    if (playerTestBeingHit(125, 0, 0, py.misc.ac + py.misc.magical_ac, CLASS_MISC_HIT)) {
        if (!py.flags.sustain_con) {
            (void) playerStatRandomDecrease(py_attrs::A_CON);

            obj_desc_t description = {'\0'};
            itemDescription(description, item, true);
            playerTakesHit(dam, description);

            printMessage("A small dart saps your health!");
        } else {
            printMessage("A small dart hits you.");
        }
    } else {
        printMessage("A small dart barely misses you.");
    }
}

enum class TrapTypes {
    open_pit = 1,
    arrow_pit,
    covered_pit,
    trap_door,
    sleeping_gas,
    hidden_object,
    dart_of_str,
    teleport,
    rockfall,
    corroding_gas,
    summon_monster,
    fire_trap,
    acid_trap,
    poison_gas,
    blinding_gas,
    confuse_gas,
    slow_dart,
    dart_of_con,
    secret_door,
    scare_monster = 99,
    general_store = 101,
    armory,
    weaponsmith,
    temple,
    alchemist,
    magic_shop,
};

// Player hit a trap.  (Chuckle) -RAK-
static void playerStepsOnTrap(Coord_t coord) {
    playerEndRunning();
    trapChangeVisibility(coord);

    Inventory_t const &item = treasure_list[dg.floor[coord.y][coord.x].treasure_id];

    int damage = diceRoll(item.damage);

    switch ((TrapTypes) item.sub_category_id) {
        case TrapTypes::open_pit:
            trapOpenPit(item, damage);
            break;
        case TrapTypes::arrow_pit:
            trapArrow(item, damage);
            break;
        case TrapTypes::covered_pit:
            trapCoveredPit(item, damage, coord);
            break;
        case TrapTypes::trap_door:
            trapDoor(item, damage);
            break;
        case TrapTypes::sleeping_gas:
            trapSleepingGas();
            break;
        case TrapTypes::hidden_object:
            trapHiddenObject(coord);
            break;
        case TrapTypes::dart_of_str:
            trapStrengthDart(item, damage);
            break;
        case TrapTypes::teleport:
            trapTeleport(coord);
            break;
        case TrapTypes::rockfall:
            trapRockfall(coord, damage);
            break;
        case TrapTypes::corroding_gas:
            trapCorrodeGas();
            break;
        case TrapTypes::summon_monster:
            trapSummonMonster(coord);
            break;
        case TrapTypes::fire_trap:
            trapFire(damage);
            break;
        case TrapTypes::acid_trap:
            trapAcid(damage);
            break;
        case TrapTypes::poison_gas:
            trapPoisonGas(damage);
            break;
        case TrapTypes::blinding_gas:
            trapBlindGas();
            break;
        case TrapTypes::confuse_gas:
            trapConfuseGas();
            break;
        case TrapTypes::slow_dart:
            trapSlowDart(item, damage);
            break;
        case TrapTypes::dart_of_con:
            trapConstitutionDart(item, damage);
            break;
        case TrapTypes::secret_door:
        case TrapTypes::scare_monster:
            break;

            // Town level traps are special, the stores.
        case TrapTypes::general_store:
            storeEnter(0);
            break;
        case TrapTypes::armory:
            storeEnter(1);
            break;
        case TrapTypes::weaponsmith:
            storeEnter(2);
            break;
        case TrapTypes::temple:
            storeEnter(3);
            break;
        case TrapTypes::alchemist:
            storeEnter(4);
            break;
        case TrapTypes::magic_shop:
            storeEnter(5);
            break;

        default:
            printMessage("Unknown trap value.");
            break;
    }
}

static bool playerRandomMovement(int dir) {
    // Never random if sitting
    if (dir == 5) {
        return false;
    }

    // 75% random movement
    bool playerRandomMove = randomNumber(4) > 1;

    bool playerIsConfused = py.flags.confused > 0;

    return playerIsConfused && playerRandomMove;
}

// Player is on an object. Many things can happen based -RAK-
// on the TVAL of the object. Traps are set off, money and most objects
// are picked up. Some objects, such as open doors, just sit there.
static void carry(Coord_t coord, bool pickup) {
    Inventory_t &item = treasure_list[dg.floor[coord.y][coord.x].treasure_id];

    int tileFlags = treasure_list[dg.floor[coord.y][coord.x].treasure_id].category_id;

    if (tileFlags > TV_MAX_PICK_UP) {
        if (tileFlags == TV_INVIS_TRAP || tileFlags == TV_VIS_TRAP || tileFlags == TV_STORE_DOOR) {
            // OOPS!
            playerStepsOnTrap(coord);
        }
        return;
    }

    obj_desc_t description = {'\0'};
    obj_desc_t msg = {'\0'};

    playerEndRunning();

    // There's GOLD in them thar hills!
    if (tileFlags == TV_GOLD) {
        py.misc.au += item.cost;

        itemDescription(description, item, true);
        (void) sprintf(msg, "You have found %d gold pieces worth of %s", item.cost, description);

        printCharacterGoldValue();
        (void) dungeonDeleteObject(coord);

        printMessage(msg);

        return;
    }

    // Too many objects?
    if (inventoryCanCarryItemCount(item)) {
        // Okay,  pick it up
        if (pickup && config::options::prompt_to_pickup) {
            itemDescription(description, item, true);

            // change the period to a question mark
            description[strlen(description) - 1] = '?';
            pickup = getInputConfirmation("Pick up " + std::string(description));
        }

        // Check to see if it will change the players speed.
        if (pickup && !inventoryCanCarryItem(item)) {
            itemDescription(description, item, true);

            // change the period to a question mark
            description[strlen(description) - 1] = '?';
            pickup = getInputConfirmation("Exceed your weight limit to pick up " + std::string(description));
        }

        // Attempt to pick up an object.
        if (pickup) {
            int locn = inventoryCarryItem(item);

            itemDescription(description, inventory[locn], true);
            (void) sprintf(msg, "You have %s (%c)", description, locn + 'a');
            printMessage(msg);
            (void) dungeonDeleteObject(coord);
        }
    } else {
        itemDescription(description, item, true);
        (void) sprintf(msg, "You can't carry %s", description);
        printMessage(msg);
    }
}

// Moves player from one space to another. -RAK-
void playerMove(int direction, bool do_pickup) {
    if (playerRandomMovement(direction)) {
        direction = randomNumber(9);
        playerEndRunning();
    }

    Coord_t coord = py.pos;

    // Legal move?
    if (!playerMovePosition(direction, coord)) {
        return;
    }

    Tile_t const &tile = dg.floor[coord.y][coord.x];
    Monster_t const &monster = monsters[tile.creature_id];

    // if there is no creature, or an unlit creature in the walls then...
    // disallow attacks against unlit creatures in walls because moving into
    // a wall is a free turn normally, hence don't give player free turns
    // attacking each wall in an attempt to locate the invisible creature,
    // instead force player to tunnel into walls which always takes a turn
    if (tile.creature_id < 2 || (!monster.lit && tile.feature_id >= MIN_CLOSED_SPACE)) {
        // Open floor spot
        if (tile.feature_id <= MAX_OPEN_SPACE) {
            // Make final assignments of char coords
            Coord_t old_coord = py.pos;

            py.pos.y = coord.y;
            py.pos.x = coord.x;

            // Move character record (-1)
            dungeonMoveCreatureRecord(old_coord, py.pos);

            // Check for new panel
            if (coordOutsidePanel(py.pos, false)) {
                drawDungeonPanel();
            }

            // Check to see if they should stop
            if (py.running_tracker != 0) {
                playerAreaAffect(direction, py.pos);
            }

            // Check to see if they've noticed something
            // fos may be negative if have good rings of searching
            if (py.misc.fos <= 1 || randomNumber(py.misc.fos) == 1 || ((py.flags.status & config::player::status::PY_SEARCH) != 0u)) {
                playerSearch(py.pos, py.misc.chance_in_search);
            }

            if (tile.feature_id == TILE_LIGHT_FLOOR) {
                // A room of light should be lit.

                if (!tile.permanent_light && (py.flags.blind == 0)) {
                    dungeonLightRoom(py.pos);
                }
            } else if (tile.perma_lit_room && py.flags.blind < 1) {
                // In doorway of light-room?

                for (int row = (py.pos.y - 1); row <= (py.pos.y + 1); row++) {
                    for (int col = (py.pos.x - 1); col <= (py.pos.x + 1); col++) {
                        if (dg.floor[row][col].feature_id == TILE_LIGHT_FLOOR && !dg.floor[row][col].permanent_light) {
                            dungeonLightRoom(Coord_t{row, col});
                        }
                    }
                }
            }

            // Move the light source
            dungeonMoveCharacterLight(old_coord, py.pos);

            // An object is beneath them.
            if (tile.treasure_id != 0) {
                carry(py.pos, do_pickup);

                // if stepped on falling rock trap, and space contains
                // rubble, then step back into a clear area
                if (treasure_list[tile.treasure_id].category_id == TV_RUBBLE) {
                    dungeonMoveCreatureRecord(py.pos, old_coord);
                    dungeonMoveCharacterLight(py.pos, old_coord);

                    py.pos.y = old_coord.y;
                    py.pos.x = old_coord.x;

                    // check to see if we have stepped back onto another trap, if so, set it off
                    uint8_t id = dg.floor[py.pos.y][py.pos.x].treasure_id;
                    if (id != 0) {
                        int val = treasure_list[id].category_id;
                        if (val == TV_INVIS_TRAP || val == TV_VIS_TRAP || val == TV_STORE_DOOR) {
                            playerStepsOnTrap(py.pos);
                        }
                    }
                }
            }
        } else {
            // Can't move onto floor space

            if ((py.running_tracker == 0) && tile.treasure_id != 0) {
                if (treasure_list[tile.treasure_id].category_id == TV_RUBBLE) {
                    printMessage("There is rubble blocking your way.");
                } else if (treasure_list[tile.treasure_id].category_id == TV_CLOSED_DOOR) {
                    printMessage("There is a closed door blocking your way.");
                }
            } else {
                playerEndRunning();
            }
            game.player_free_turn = true;
        }
    } else {
        // Attacking a creature!

        int old_find_flag = py.running_tracker;

        playerEndRunning();

        // if player can see monster, and was in find mode, then nothing
        if (monster.lit && (old_find_flag != 0)) {
            // did not do anything this turn
            game.player_free_turn = true;
        } else {
            playerAttackPosition(coord);
        }
    }
}
