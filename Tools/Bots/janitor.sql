-- ============================================================================
-- DEV-ONLY janitor: delete ephemeral test accounts (adm_*) older than 7 days.
-- NEVER run against live VPS. Apply to the DEV database only:
--   docker exec -i mmorpg_prototype_db psql -U postgres -d mmo_prototype \
--     < Tools/Bots/janitor.sql   (from the client repo root via WSL path)
--
-- Safety: prefix adm\_ only + role=0 guard (gm_bot is role=1 and a different
-- prefix; seeded bot_* never match). Characters go via
-- characters_owner_id_fkey ON DELETE CASCADE. No server bounce needed
-- (deleted characters simply fail future logins; game quest cache holds
-- only live sessions).
-- ============================================================================

DELETE FROM public.users
WHERE login LIKE 'adm\_%' AND role = 0
      AND created_at < NOW() - INTERVAL '7 days';

-- Verification (expect the remaining count).
SELECT COUNT(*) AS adm_remaining FROM public.users WHERE login LIKE 'adm\_%';
