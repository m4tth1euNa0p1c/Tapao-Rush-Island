# TAPAO Rush Island — Dedicated server on Google Cloud (T-367)

**Status: engineering runbook, NOT deployed or validated.** Last checked 2026-10-17.
Use the existing direct-IP multiplayer protocol first. This does **not** implement matchmaking, Google sign-in, relay NAT, Steam invites or a cloud autoscaler.

## Release gates — in order

1. Pass [T-133](https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/issues/133): **two real Windows PCs on a LAN**, through create/join, boat, item pickup, death and seamless travel. Preserve a gameplay recording and BOTH logs.
2. Install a **source build** of Unreal Engine 5.8 on a build machine (Epic's dedicated server target requires an engine built from source). Regenerate project files, compile `Development Server` for `monke_islandServer`. Dedicated builds also need correctly cooked assets for server and client; test locally before uploading anything.
3. Boot locally into `/Game/MonkeIsland/Maps/L_Boot` (set as ServerDefaultMap), UDP port 7777. Confirm that **an empty server waits for the first player** (T-367) and that a remote player joining starts the join grace period. The session is still direct-IP, not Steam or GCP-backed discovery.
4. Run a second client on another machine, then repeat from another internet connection. Check joining, 4-player cap, sessions that reject late joins, island travel, items, mobs, death, host/network failures, crash recovery. Only then provision cloud testing.
5. Archive build version, commit SHA, server/client logs, memory and CPU, packet loss and latency. Release gate: 30-minute 4-player run on two different networks without blocking desync.

## Minimal first cloud topology

```text
Windows PC A ---- UDP 7777 ---> Compute Engine VM (single dedicated UE server)
Windows PC B ---- UDP 7777 ------^    private build, no game client running here
                                      |
                                Cloud Logging / journalctl
```

Choose **one** Compute Engine Linux VM only after LinuxServer compilation and local Linux smoke testing. A Windows VM with a successfully built Windows Server artifact is an alternative; do not assume a Linux server can execute Windows packaged assets. Avoid GKE/Agones until there is proven player demand or elastic fleet requirements.

GCP prerequisites: billing-enabled project, explicit budget + alerts, compute quota and region, provisioner identity, private artifact bucket or controlled SSH upload, firewall permissions, minimal service account. No GitHub commits of credentials. Default-deny ingress, allow **UDP 7777 only from testers' public IP CIDRs**; use IAP or a limited SSH route for administration.

**Illustrative operator commands only — replace placeholders and check permissions/cost. Do NOT run before the local build gate.**

```bash
gcloud config set project PROJECT_ID

# Restrict external traffic to testers. Do not accidentally publish 7777 to 0.0.0.0/0.
gcloud compute firewall-rules create tapao-test-game-udp   --network=default --direction=INGRESS --action=ALLOW   --rules=udp:7777 --source-ranges=TESTER_PUBLIC_IP/32   --target-tags=tapao-test-game

# Instance image and type are EXAMPLES, not validated capacity recommendations.
gcloud compute instances create tapao-test-01   --zone=europe-west9-b --machine-type=e2-standard-4   --image-family=debian-12 --image-project=debian-cloud   --tags=tapao-test-game   --service-account=LEAST_PRIVILEGE_SA@PROJECT_ID.iam.gserviceaccount.com   --scopes=https://www.googleapis.com/auth/cloud-platform
```

Choose a suitable supported zone and measure egress/cost before long-running tests. The example has not been executed; do not interpret the instance type as sized for 4 players. Copy the **built and cooked** Linux server files through a private bucket with object-read permission or secure SSH, pin SHA-256, then run under a non-root Unix service with restart-on-failure and rate-limited logs. A typical run command from the packaged server directory is:

```bash
./monke_islandServer /Game/MonkeIsland/Maps/L_Boot -log -port=7777
```

Adjust executable name/path to the actual packaged output. Use a `systemd` unit with `WorkingDirectory` set to the package directory; set a CPU/RAM cap after measurement and rotate logs. Monitor `journalctl` and the Unreal `Saved/Logs` path. Client's **Join** screen takes the VM's public IP (or an assigned static IP) and UDP port. No separate TCP app server is required for existing direct-IP gameplay.

**Stop/delete** the test VM and its unused public IP after sessions. Review Cloud Billing and object storage charges. No GCP environment was created by this commit.

## Critical implementation notes

- Dedicated executable target: `monke_island/Source/monke_islandServer.Target.cs`.
- `ServerDefaultMap`: `L_Boot`, not `/Engine/Maps/Entry`.
- `AExpeditionBootGameMode`: zero-player dedicated host waits in lobby; first player restarts lobby grace timer, travel waits for clients currently loading. Validate seamless travel and that the desired session joins actually land in this GameMode.
- Online subsystem is still **Null** (direct IP). The separate [T-134](https://github.com/m4tth1euNa0p1c/Tapao-Rush-Island/issues/134) is Steam networking/invites; do not mislabel GCP hosting as Steam multiplayer.
- A server accessible over public UDP needs pre-release abuse tests (oversized packets, RPC spam, server-side interaction/damage validation), a crash strategy, no user IP or microphone audio in public logs, and a maintenance strategy.
- Source checkout requires Git LFS (~8.5 GiB; compare consumption/quotas before CI). Do not bake IPs, service account keys or Steam IDs into files.

## Acceptance evidence

| Test | Required evidence | State at writing |
|---|---|---|
| `monke_islandServer` compiles using UE 5.8 source | build log + SHA | NOT TESTED |
| Server stays in L_Boot while empty | VM/server log | NOT TESTED |
| First remote join triggers grace and map travel | server + client logs | NOT TESTED |
| Two physical PCs on two networks | captures + timestamps | NOT TESTED |
| Four-player 30-minute play session | latency, net stats, CPU, RAM, gameplay log | NOT TESTED |
| Linux cloud start/restart/stop | deployment log, cost check | NOT DEPLOYED |

References:
- Unreal Engine 5.8 dedicated server setup: https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine
- Compute Engine: https://cloud.google.com/compute/docs/instances/create-start-instance
- Google Cloud firewall: https://cloud.google.com/firewall/docs/using-firewalls
- Least-privilege service accounts: https://cloud.google.com/iam/docs/best-practices-for-managing-service-account-keys
