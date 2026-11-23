# mod-instant-enchants

AzerothCore module that makes selected permanent equipment enhancements instant.

## What It Does

- Removes cast time only from spell IDs listed in the `mod_instant_enchants` world database table.
- Includes 365 spells: enchanting spells and scrolls, weapon chains, the Eternal Belt Buckle,
  leg armor, spellthreads, head/shoulder enhancements and death knight runeforging, including older expansion versions.
- Keeps normal requirements, reagent/item consumption, combat restrictions and cooldowns.
- Supports reloading the list and restoring removed spells without restarting the server.

Death knight runes still require a nearby runeforge (spell focus 1552); only their cast time changes.

The default list excludes poisons, oils, sharpening/weight stones, warlock stones,
combat engineering abilities, glyphs and general crafting. The database list is authoritative:
the module does not automatically select spells by item category, profession or spell effect.

## Requirements

- AzerothCore 3.3.5a (WotLK), with `WorldScript` startup/update/config hooks and `CommandScript`.
- Access to the world database for importing SQL and editing the spell list.

## Installation

1. Place the repository under `modules/mod-instant-enchants` in your AzerothCore source:

   ```bash
   git clone https://github.com/Haeniken/mod-instant-enchants.git modules/mod-instant-enchants
   ```

2. Import `data/sql/db-world/mod_instant_enchants.sql` into the **world** database.
3. Re-run CMake, rebuild and install the core.
4. Copy the installed `etc/modules/mod_instant_enchants.conf.dist` to
   `etc/modules/mod_instant_enchants.conf`.
5. Restart worldserver.

No core patch or client modification is required.

## Configuration

```ini
InstantEnchants.Enable = 1
```

Set this to `0` and run `.reload config` to restore normal cast times.
Set it back to `1` and reload the configuration to apply the database list again.
Changes take effect at the next world update; casts already in progress keep their calculated duration.

The table has two columns:

| Column | Purpose |
|---|---|
| `spell_id` | Spell ID to make instant; primary key. |
| `comment` | Description for administrators; not used by the module. |

For example, add the Eternal Belt Buckle:

```sql
INSERT INTO `mod_instant_enchants` (`spell_id`, `comment`)
VALUES (55655, 'Eternal Belt Buckle')
ON DUPLICATE KEY UPDATE `comment` = 'Eternal Belt Buckle';
```

Remove a spell to give it its original cast time:

```sql
DELETE FROM `mod_instant_enchants` WHERE `spell_id` = 55655;
```

Then reload the table in game:

```text
.reload instant_enchants
```

The command uses the core's `RBAC_PERM_COMMAND_RELOAD` permission and also works from the console
without the leading dot. It queues the reload for the world thread; the result and active spell count
are written to the server log. A failed database query leaves the current overrides unchanged.
An empty table removes all overrides. Unknown spell IDs are logged and skipped.

The installation SQL uses `INSERT IGNORE`: importing it again preserves existing comments and
adds missing default rows, including defaults you previously deleted. Do not re-import it after
customizing the list unless you intend to restore those defaults.

## Notes

- Only server-side cast time is changed. Client tooltips may still display the original DBC cast time.
- Enchanting directly and using a scroll may share the same Spell ID; both become instant.
- The module preserves the original cast-time entry for each overridden spell, including overrides
  already installed by the core before this module starts.
- Reloading is applied in `WorldScript::OnUpdate`, after map update workers have finished.
- The module does no database queries during casts and performs no per-cast spell selection.

## Uninstall

Set `InstantEnchants.Enable = 0` and run `.reload config` to disable the feature immediately.
To remove it completely, remove the module from the source tree, re-run CMake, rebuild/install
and restart worldserver. The `mod_instant_enchants` table can then be dropped if no longer needed.

## Credits

- [Haeniken](https://github.com/Haeniken) and the Elgracia customization team.
- [SymbolixDEV/Core](https://github.com/SymbolixDEV/Core/blob/0f71d17827f7e2aaede115d3108f3af12d775964/src/server/game/Entities/Player/Player.cpp#L8646)
  provided a historical reference for instant item enhancements. This module uses an explicit spell
  list instead of that implementation's item-category filter; its C++ implementation is independent.
- [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk).
