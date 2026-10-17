# TAPAO Rush Island — Steamworks publication and SteamPipe (T-368)

Status: OPERATING CHECKLIST ONLY, NOT PUBLISHED. Updated 2026-10-17.
This repository has no verified Steamworks AppID, page, or shipping-ready online multiplayer. Steam Direct onboarding and Valve review require an authorized publisher account. Do not infer launch approval from any checked-in file.

## Mandatory release decisions

- [ ] Obtain explicit publisher authorization for public distribution and a cleared title and legal entity.
- [ ] Audit rights to every shipped texture, soundtrack, reference map and dataset (T-130); remove anything licensed only for research or education. Verify real Raraka/cultural/sacred-site references and localization (T-139).
- [ ] Finish Steam Direct onboarding, bank/tax identity, price/countries, appropriate content and AI disclosure forms, accessibility and privacy policy; collect the REAL AppID and Windows content DepotID, do not commit Steam credentials.
- [ ] Create Store Presence: capsules, graphical assets, five real-build gameplay screenshots, trailer, supported-language and system-requirement evidence. Do not depict unshipped features as implemented.
- [ ] Have an actual Windows Shipping build that starts on a clean install (T-298); no PDB, Saved, editor-only plugins, secrets or private imagery in the package.
- [ ] Pass LAN on two separate PCs (T-133), then internet multiplayer (T-367), then Steam invites/relay (T-134) before claiming "1–4 player online co-op" on the public page.

## Store copy (draft; do not post until verified)

EN: Take a tiny royal crew to a doomed tropical island. Row ashore, grab fragile loot and race the rising sea back to your galleon in a chaotic extraction adventure.

FR: Débarquez sur une île tropicale condamnée, récupérez un butin fragile et regagnez votre galion avant la montée des eaux, dans une aventure d'extraction chaotique.

Avoid the real island/place names and culturally sensitive terms until the responsible review is complete. Candidate tags (verify with actual release): Action, Adventure, Co-op, Online Co-Op, Multiplayer, Sailing, Exploration, Dark Humor, Indie. Do not claim unverified controller support, voice chat, Steam friend invites or 20 playable maps.

## Steamworks release sequence

1. Complete Steam Direct account onboarding and app setup. Note any fee, waiting period and eligibility that Steamworks reports for this application.
2. Configure app Windows platform, launch executable, DepotID, installation directory, installable packages and supported language(s) in Steamworks. Save and publish INTERNAL Steamworks configuration changes, not the public store listing.
3. Submit the Store Presence checklist for Valve review. After approval, explicitly publish Coming Soon when the publisher authorizes it.
4. Steam normally requires Coming Soon to be public for at least TWO WEEKS before release; onboarding may require additional waiting (check actual Steamworks state).
5. Produce a signed-off Windows Shipping package; on a clean Windows account verify boot, input, gameplay loop and claims. Archive checksums and symbols outside the package, private.
6. Generate local SteamPipe manifests with actual AppID and DepotID; the generator validates obvious debug/secret files and Unreal cooked content. Example PowerShell commands (replace IDs and paths):

       python scripts/steam/make_steampipe_vdf.py --appid REAL_APP_ID --depotid REAL_DEPOT_ID --content-root "C:\Temp\TapaoShipping" --output-dir "C:\Temp\TapaoSteamPipe" --executable monke_island.exe --version candidate
       python -m unittest discover -s scripts/steam -p "test_*.py"

   This tool does NOT authenticate, upload, publish or activate Steam builds. The VDF intentionally omits SetLive.
7. Install the official Steamworks SDK ContentBuilder in a secure environment, put the VDF files in its scripts directory or reference their absolute paths. Log in with a restricted SteamPipe build account + Steam Guard, not an account password in source. On the builder, upload ONLY after signing off this operation:

       steamcmd +login YOUR_BUILD_ACCOUNT +run_app_build C:\Temp\TapaoSteamPipe\app_REAL_APP_ID.vdf +quit

   Check the generated BuildID in Steamworks. An uploaded build is NOT released. First make it available only on a private Steam beta branch for full reinstall/update/rollback QA.
8. Store Presence review must be complete before the Game Build is submitted for Valve review. Address Valve feedback. Verify two Steam accounts on different networks, intended networking backend, install/download from Steam, saves, post-journey reconnect and crash reporting.
9. Only the authorized publisher performs the final RELEASE APP action in Steamworks after the GO/NO-GO checklist; this operation is intentionally not automated.
10. A public demo, Steam Playtest or Next Fest registration is separate; recheck eligibility and event calendar. Never advertise untested co-op or publish a broken demo.

## Evidence and status

| Gate | Proof | Current state |
|---|---|---|
| Rights and commercial name | T-130 / T-139 and owner approval | OPEN |
| Steamworks account and AppID | Steamworks UI | NOT VERIFIED |
| Valve-approved Coming Soon | Store review status | NOT SUBMITTED |
| Two PCs / four-player multiplayer | T-133 and T-367 session logs | NOT VALIDATED |
| Steam invites/relay | T-134 two-account test | NOT IMPLEMENTED |
| Windows Shipping | T-298 and clean install | NOT REVALIDATED |
| SteamPipe build upload and private beta | Steam BuildID and testers | NOT UPLOADED |
| Valve build approval | Steamworks review result | NOT SUBMITTED |
| Manual public release | authorized publisher | NOT RELEASED |

## Primary references

- https://partner.steamgames.com/doc/store/releasing
- https://partner.steamgames.com/doc/sdk/uploading
- https://partner.steamgames.com/doc/features/playtest
- https://partner.steamgames.com/steamdirect
- https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine
