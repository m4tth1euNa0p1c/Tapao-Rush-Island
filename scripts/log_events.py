"""Exporte uniquement les événements reconnus des logs locaux UE ; aucun envoi réseau.

Les champs libres, noms d'acteurs, chemins et en-têtes User/Computer sont ignorés.
La durée mesure le début -> dernière fin observée (submersion ou équipage perdu),
pas la durée de session. Une expédition sans fin demeure incomplète.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
from collections import Counter
from pathlib import Path

FIELDS = ("event", "expedition_id", "t", "cause", "phase", "alive", "entity", "build", "rules_hash")
GUID = r"(?:[0-9A-Fa-f]{32}|[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12})"
NUMBER = r"[0-9]{1,12}(?:\.[0-9]{1,6})?"
EXPEDITION = re.compile(rf"\bExpedition (?P<id>{GUID}|none): (?P<body>.*)")
START = re.compile(rf"started at (?P<t>{NUMBER}) s, flood at {NUMBER} s")
DEATH = re.compile(rf"died schema=1 cause=(unknown|fall|drowning|coconut|tiki|shark) t=({NUMBER}) phase=(?:EExpeditionPhase::)?(Preparing|Exploring|Submerging|Resolved|unknown) alive=([0-9]{{1,4}}) entity=(player|creature)(?:\s|$)")
END = re.compile(rf"(?:phase \S+ -> (?:EExpeditionPhase::)?Resolved at|every player is dead at) ({NUMBER}) s")
STAMP = re.compile(r"\[[0-9.:-]{15,30}\]\[\s*[0-9]+\]")


def parse_line(line: str) -> dict | None:
    """Allowlist stricte : ne renvoie jamais un fragment de ligne libre."""
    if len(line) > 65536 or re.search(r"\b(?:User|Computer):", line, re.I):
        return None
    match = EXPEDITION.search(line)
    if not match:
        # Un ancien log ne permet pas de connaître la cause ni le type du mort.
        if " died (killer:" in line:
            return dict(event="legacy_death", cause="unknown", entity="unknown")
        return None
    expedition_id = match["id"].replace("-", "").lower()
    if expedition_id == "0" * 32:
        expedition_id = "none"
    body = match["body"]
    event = dict(expedition_id=expedition_id)
    start = START.match(body)
    if start:
        event.update(event="start", t=float(start["t"]))
        build = re.search(r"\bbuild=([a-zA-Z0-9._+\-]{1,96})(?=\s|$)", body)
        fingerprint = re.search(r"\brules_hash=([a-fA-F0-9]{32})(?=\s|$)", body)
        event.update(build=build[1] if build else "unversioned", rules_hash=fingerprint[1].lower() if fingerprint else "")
        return event
    death = DEATH.fullmatch(body.strip())
    if death:
        event.update(event="death", cause=death[1], t=float(death[2]), phase=death[3], alive=int(death[4]), entity=death[5])
        return event
    end = END.match(body)
    if end:
        event.update(event="end", t=float(end[1]), phase="TeamWiped" if body.startswith("every") else "Resolved")
        return event
    return None


def read_events(paths):
    """Déduplique les lignes horodatées identiques recopiées dans plusieurs logs.

    Deux lignes identiques dans le même fichier restent deux événements (deux
    morts au même instant sont possibles). Ni noms ni chemins sources en sortie.
    """
    prior_files = Counter()
    for path in paths:
        current_file = Counter()
        with path.open(encoding="utf-8-sig", errors="replace") as stream:
            for line in stream:
                event = parse_line(line)
                if event is None:
                    continue
                stamp = STAMP.search(line)
                if stamp:
                    key = (stamp[0], json.dumps(event, sort_keys=True))
                    current_file[key] += 1
                    if current_file[key] <= prior_files[key]:
                        continue
                yield event
        prior_files |= current_file


def summarize(events):
    starts, ends = {}, {}
    deaths = Counter()
    legacy = 0
    for event in events:
        expedition_id = event.get("expedition_id", "none")
        if event["event"] == "start" and expedition_id != "none":
            starts.setdefault(expedition_id, event["t"])
        elif event["event"] == "end" and expedition_id != "none":
            ends[expedition_id] = max(event["t"], ends.get(expedition_id, 0))
        elif event["event"] == "death" and event["entity"] == "player":
            deaths[event["cause"]] += 1
        elif event["event"] == "legacy_death":
            legacy += 1
    durations = [round(ends[key] - start, 2) for key, start in starts.items()
                 if key in ends and ends[key] >= start and math.isfinite(ends[key] - start)]
    return {
        "expeditions": len(starts), "completed_observed": len(durations),
        "incomplete": len(starts) - len(durations), "durations_seconds": durations,
        "mean_duration_seconds": round(sum(durations) / len(durations), 2) if durations else None,
        "player_deaths_by_cause": dict(sorted(deaths.items())),
        "legacy_deaths_unclassified": legacy,
    }


def main(argv=None):
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--logs", type=Path, default=root / "monke_island/Saved/Logs")
    parser.add_argument("--csv", type=Path, default=root / "monke_island/Saved/Telemetry/events.csv")
    parser.add_argument("--summary", type=Path, default=root / "monke_island/Saved/Telemetry/summary.json")
    args = parser.parse_args(argv)
    paths = sorted(args.logs.glob("*.log"))
    if not paths:
        parser.exit(2, "Aucun journal .log trouvé.\n")
    outputs = (args.csv.resolve(), args.summary.resolve())
    if outputs[0] == outputs[1] or any(p.suffix.lower() == ".log" for p in outputs) or any(p in outputs for p in (x.resolve() for x in paths)):
        parser.exit(2, "Les sorties doivent être distinctes des journaux et l'une de l'autre.\n")
    try:
        events = list(read_events(paths))
        for path in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(events)
        summary = summarize(events)
        args.summary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(summary, ensure_ascii=False, indent=2))
    except OSError:
        parser.exit(2, "Lecture ou écriture impossible (chemin personnel masqué).\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
