/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Haeniken
 */

#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "RBAC.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldScript.h"
#include <atomic>
#include <unordered_map>
#include <vector>

namespace
{
    class InstantEnchants
    {
    public:
        static InstantEnchants& Instance()
        {
            static InstantEnchants instance;
            return instance;
        }

        void RequestReload()
        {
            _reloadRequested.store(true);
        }

        void Update()
        {
            if (_reloadRequested.exchange(false))
                Reload();
        }

        void Reload()
        {
            bool const enabled = sConfigMgr->GetOption<bool>("InstantEnchants.Enable", true);
            std::vector<uint32> spellIds;
            SpellCastTimesEntry const* instant = nullptr;

            if (enabled)
            {
                instant = sSpellCastTimesStore.LookupEntry(1);
                if (!instant)
                {
                    LOG_ERROR("module",
                        "mod-instant-enchants: instant cast time entry is missing; keeping current rules.");
                    return;
                }

                // The sentinel distinguishes an empty table from a failed query.
                QueryResult result = WorldDatabase.Query(
                    "SELECT `spell_id` FROM `mod_instant_enchants` UNION ALL SELECT 0");
                if (!result)
                {
                    LOG_ERROR("module", "mod-instant-enchants: cannot read the table; keeping current rules.");
                    return;
                }

                do
                {
                    uint32 const spellId = result->Fetch()[0].Get<uint32>();
                    if (spellId)
                        spellIds.push_back(spellId);
                } while (result->NextRow());
            }

            // Only this module's previous overrides are restored, including null entries.
            for (auto const& [spellId, original] : _originalCastTimes)
                if (SpellInfo* spell = GetSpell(spellId))
                    spell->CastTimeEntry = original;

            _originalCastTimes.clear();

            for (uint32 spellId : spellIds)
            {
                SpellInfo* spell = GetSpell(spellId);
                if (!spell)
                {
                    LOG_ERROR("module", "mod-instant-enchants: unknown spell {} in the table; skipped.", spellId);
                    continue;
                }

                _originalCastTimes.emplace(spellId, spell->CastTimeEntry);
                spell->CastTimeEntry = instant;
            }

            LOG_INFO("module", "mod-instant-enchants: {} spell overrides active (enabled: {}).",
                _originalCastTimes.size(), enabled);
        }

    private:
        static SpellInfo* GetSpell(uint32 spellId)
        {
            return const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(spellId));
        }

        std::atomic<bool> _reloadRequested{ false };
        std::unordered_map<uint32, SpellCastTimesEntry const*> _originalCastTimes;
    };

    class InstantEnchantsWorldScript final : public WorldScript
    {
    public:
        InstantEnchantsWorldScript() : WorldScript("InstantEnchantsWorldScript",
            { WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

        void OnStartup() override
        {
            InstantEnchants::Instance().Reload();
        }

        void OnAfterConfigLoad(bool reload) override
        {
            if (reload)
                InstantEnchants::Instance().RequestReload();
        }

        void OnUpdate(uint32 /*diff*/) override
        {
            // World updates run after MapMgr has waited for all map update workers.
            InstantEnchants::Instance().Update();
        }
    };

    class InstantEnchantsCommandScript final : public CommandScript
    {
    public:
        InstantEnchantsCommandScript() : CommandScript("InstantEnchantsCommandScript") { }

        Acore::ChatCommands::ChatCommandTable GetCommands() const override
        {
            using namespace Acore::ChatCommands;

            static ChatCommandTable const commands =
            {
                { "reload instant_enchants", HandleReload, rbac::RBAC_PERM_COMMAND_RELOAD, Console::Yes }
            };
            return commands;
        }

    private:
        static bool HandleReload(ChatHandler* handler)
        {
            InstantEnchants::Instance().RequestReload();
            handler->SendSysMessage("Instant enchants reload queued. See the server log for the result.");
            return true;
        }
    };
}

void AddInstantEnchantsScripts()
{
    new InstantEnchantsWorldScript();
    new InstantEnchantsCommandScript();
}
