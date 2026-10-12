"""T-092 (lots L3, L4, L8): names and identities of the 20 expedition islands in the String Table ST_Islands.

The texts live in docs/narrative/islands.md, in the rows "| `Island.<...>` | FR | EN |". The CSV is generated from them.

Outside the editor (no unreal module), from the root of the repository:
    python scripts/editor/island_names_ue.py --write-csv   # writes docs/narrative/ST_Islands.csv, then checks it
    python scripts/editor/island_names_ue.py --check       # checks only; exit code 1 on a problem
Report: monke_island/Saved/T-092/island-names-check.json

In the editor: scripts/editor/lead_session_t186.py, action "islandnames" (editor lock held, no game running).
  1. The same checks run first: nothing is written when one fails.
  2. /Game/MonkeIsland/UI/Text/ST_Islands (created when missing): when its entries differ from the CSV, the table is
     exported to Saved/T-092/ST_Islands.before.csv, then replaced by the CSV (keys absent from the CSV disappear),
     verified entry by entry and saved.
  3. DA_Island_Raraka_01..20: each FText identity property present on UExpeditionIslandDefinition (DisplayName today;
     Epithet, Tagline, CrownName, Pitch, Rumor once the C++ has them) is pointed to ST_Islands, key
     Island.<FragmentId>.<Field>. Absent properties are skipped and listed. DataCredits naming the real atoll is
     reported (T-139), never changed. DA_Island_Raraka_21 (out of the catalog) and DA_IslandCatalog are only read.
  Idempotent: a second run saves nothing. Report: monke_island/Saved/T-092/island-names.json
  A failed run ends with a RuntimeError that only names the report.
"""
import argparse
import csv
import datetime as dt
import io
import json
import os
import re
import sys
import traceback
from pathlib import Path

try:
    ROOT = Path(__file__).resolve().parents[2]
except NameError:  # run from a string: the default checkout
    ROOT = Path(os.environ.get("MONKE_ROOT", "C:/Users/matth/monk-island"))

DOC = ROOT / "docs/narrative/islands.md"
CSV_PATH = ROOT / "docs/narrative/ST_Islands.csv"
VILLAGE_INDEX = ROOT / "docs/reference/map/raraka/villages/fragments/index.json"
LOCK = ROOT / ".claude/factory/editor.lock.json"
OUT = ROOT / "monke_island/Saved/T-092"
REPORT = OUT / "island-names.json"
CHECK_REPORT = OUT / "island-names-check.json"
BACKUP = OUT / "ST_Islands.before.csv"

ST_FOLDER = "/Game/MonkeIsland/UI/Text"
ST_NAME = "ST_Islands"
ST_PATH = f"{ST_FOLDER}/{ST_NAME}"
ISLANDS_FOLDER = "/Game/MonkeIsland/Data/Islands"
CATALOG = f"{ISLANDS_FOLDER}/DA_IslandCatalog"
OUT_OF_CATALOG = f"{ISLANDS_FOLDER}/DA_Island_Raraka_21"

FRAGMENTS = tuple(f"FRG_Raraka_{n:02d}" for n in range(1, 21))
# maximum length of each field of an island, in characters, French and English (UI12, docs/narrative/islands.md 1.8)
LIMITS = {"Name": 28, "Epithet": 40, "Tagline": 100, "CrownName": 60, "Pitch": 110, "Rumor": 130,
          "Village.Riches": 16, "Village.Wood": 16, "Village.Food": 16}
UI_KEYS = ("Island.UI.CrownNameFormat", "Island.UI.RumorFormat", "Island.UI.VillagesFormat",
           "Island.UI.Destination", "Island.UI.DayEndIsland")
THEME_KEYS = ("Island.Theme.Riches", "Island.Theme.Wood", "Island.Theme.Food")
# theme of a zone village in the generator memory (index.json) -> field of the String Table
THEME_FIELD = {"richesses": "Village.Riches", "bois": "Village.Wood", "vivres": "Village.Food"}
THEME_LABEL = {"Village.Riches": "richesses", "Village.Wood": "bois", "Village.Food": "nourriture"}
# FText identity properties of UExpeditionIslandDefinition (Python names) -> field of the String Table
DEFINITION_TEXTS = (("display_name", "Name"), ("epithet", "Epithet"), ("tagline", "Tagline"),
                    ("crown_name", "CrownName"), ("pitch", "Pitch"), ("rumor", "Rumor"))
# villages of Lvl_Raraka near the real village (place_villages.py): never reused for a fragment
RESERVED_VILLAGE_WORDS = ("Takara", "Mori", "Kura")
FORBIDDEN = re.compile(
    r"\b(raraka|motutapu|manureva|tuamotu|tiki|veilleurs?|paradis|paradise|sauvages?|savages?|indig[eè]nes?|"
    r"natifs?|natives?|tribus?|tribes?|primitifs?|tapu|mana|atua|ariki|dieux?|gods?|sacrifices?|maudits?|cursed|"
    r"conquise?s?|conqu[eé]rir|conquer(ed)?|chaloupes?|mobs?|pnj|loot)\b", re.IGNORECASE)
FORBIDDEN_CHARS = ("$", "!")
KEY = re.compile(r"^Island\.(UI\.[A-Za-z]+|Theme\.(Riches|Wood|Food)|FRG_Raraka_\d{2}\."
                 r"(Name|Epithet|Tagline|CrownName|Pitch|Rumor|Village\.(Riches|Wood|Food)))$")
ROW = re.compile(r"^\|\s*`(Island\.[A-Za-z0-9_.]+)`\s*\|(.*)\|\s*$")
# summary table of islands.md section 2: "| 01 | Name | Epithet | ..." (a copy of the keys, checked against them)
SUMMARY = re.compile(r"^\|\s*(\d{2})\s*\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|")
PLACEHOLDER = re.compile(r"\{(\w+)\}")
VILLAGE = re.compile(r"^[A-Z][a-z]+-mura$")
NBSP = "\u00a0"

FIELD_NOTES = {
    "Name": "Nom affiché de l'île : carte d'arrivée, chargement, bilan, contrat. Image traduite, jamais un toponyme "
            "réel. 28 caractères au plus (UI12).",
    "Epithet": "Sous-titre factuel de la carte d'arrivée : ce que le joueur va trouver. 40 caractères au plus (UI12).",
    "Tagline": "Phrase d'ambiance de l'écran de chargement, voix du narrateur, sans plaisanterie. 100 caractères au "
               "plus (UI12).",
    "CrownName": "Désignation de la Couronne (satire du Bureau), affichée par Island.UI.CrownNameFormat. 60 caractères "
                 "au plus (UI12).",
    "Pitch": "Accroche du contrat, registre du Bureau : vouvoiement ou impersonnel, sans point d'exclamation. 110 "
             "caractères au plus (UI12).",
    "Rumor": "Rumeur du contrat (Island.UI.RumorFormat) : un bien perdu par la Couronne, jamais un pillage des "
             "habitants. 130 caractères au plus (UI12) ; à valider contre la table de butin.",
    "Village": "Nom du village de la zone « {theme} » : romaji Hepburn, identique dans toutes les langues. 16 caractères "
               "au plus.",
}
UI_NOTES = {
    "Island.UI.CrownNameFormat": "Sous-titre du registre ; {CrownName} = Island.<FRG>.CrownName (60 caractères au plus).",
    "Island.UI.RumorFormat": "Ligne de rumeur du contrat ; {Rumor} = Island.<FRG>.Rumor (130 caractères au plus).",
    "Island.UI.VillagesFormat": "Liste des villages ; {Riches}, {Wood}, {Food} = Island.<FRG>.Village.*.",
    "Island.UI.Destination": "Titre de l'écran de chargement (§16) ; {Name} = Island.<FRG>.Name.",
    "Island.UI.DayEndIsland": "Ligne du bilan de fin de jour ; {Name} = Island.<FRG>.Name. Le mot « Île » porte "
                              "l'accord, quel que soit le nom.",
}
THEME_NOTE = "Libellé du thème de zone (cahier §6.2)."
# the export of a String Table escapes these characters (FString::ReplaceCharWithEscapedChar); undone in this order
ESCAPES = (('\\"', '"'), ("\\'", "'"), ("\\t", "\t"), ("\\r", "\r"), ("\\n", "\n"), ("\\\\", "\\"))


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def rel(path):
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def french(text):
    """Collapses the spaces and puts the French non-breaking spaces (before : ; ? ! », after « and n°)."""
    text = re.sub(r"\s+", " ", text.strip())
    text = re.sub(r" (?=[:;?!»])", NBSP, text)
    return text.replace("« ", "«" + NBSP).replace("n° ", "n°" + NBSP)


def field_of(key):
    return key.split(".", 2)[2] if key.startswith("Island.FRG_") else None


def fragment_of(key):
    return key.split(".")[1] if key.startswith("Island.FRG_") else None


def dev_notes(key, english):
    field = field_of(key)
    if key.startswith("Island.UI."):
        note = UI_NOTES.get(key, "Format d'interface.")
    elif key.startswith("Island.Theme."):
        note = THEME_NOTE
    elif field and field.startswith("Village."):
        note, english = FIELD_NOTES["Village"].format(theme=THEME_LABEL[field]), ""
    else:
        note = FIELD_NOTES.get(field, "")
    if english:
        note += f" EN proposé : {english}"
    return french(note)


def group_of(row):
    key = row["key"]
    return 0 if key.startswith("Island.UI.") else 1 if key.startswith("Island.Theme.") else 2


def read_doc():
    """Rows "| `Island.<...>` | FR | EN |" of islands.md, in document order."""
    rows, problems = [], []
    for number, line in enumerate(DOC.read_text(encoding="utf-8").splitlines(), 1):
        match = ROW.match(line)
        if not match:
            continue
        cells = [cell.strip() for cell in match.group(2).split("|")]
        if len(cells) != 2:
            problems.append(f"islands.md:{number}: {match.group(1)} has {len(cells) + 1} cells, 3 expected")
            continue
        rows.append({"key": match.group(1), "fr": cells[0], "en": cells[1], "line": number})
    return rows, problems


def expected_entries(doc_rows):
    return {row["key"]: (french(row["fr"]), dev_notes(row["key"], row["en"])) for row in doc_rows}


def csv_text(doc_rows):
    buffer = io.StringIO()
    writer = csv.writer(buffer, quoting=csv.QUOTE_ALL, lineterminator="\r\n")
    writer.writerow(["Key", "SourceString", "DevNotes"])
    for row in sorted(doc_rows, key=group_of):
        writer.writerow([row["key"], french(row["fr"]), dev_notes(row["key"], row["en"])])
    return buffer.getvalue()


def read_csv():
    """Entries of the CSV (key -> source, notes) and its problems."""
    problems, entries = [], {}
    if not CSV_PATH.exists():
        return entries, [f"{rel(CSV_PATH)} is missing: run --write-csv"]
    with CSV_PATH.open(encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.reader(handle))
    if not rows or rows[0][:2] != ["Key", "SourceString"]:
        return entries, [f"{rel(CSV_PATH)}: header must start with Key, SourceString"]
    notes = rows[0].index("DevNotes") if "DevNotes" in rows[0] else None
    for number, row in enumerate(rows[1:], 2):
        if len(row) < 2 or not row[0].strip() or not row[1].strip():
            problems.append(f"{rel(CSV_PATH)}:{number}: empty key or text")
            continue
        if row[0] in entries:
            problems.append(f"{rel(CSV_PATH)}:{number}: key {row[0]} twice")
        entries[row[0]] = (row[1], row[notes] if notes is not None and notes < len(row) else "")
    return entries, problems


def generator_villages():
    """Zone villages (field -> zone id, name) and hamlet names of each fragment, from the generator memory."""
    data = json.loads(VILLAGE_INDEX.read_text(encoding="utf-8"))
    fragments = data.get("fragments", [])
    if isinstance(fragments, dict):
        fragments = [dict(value, id=key) for key, value in fragments.items()]
    result = {}
    for fragment in fragments:
        zones, hamlets = {}, []
        for village in fragment.get("villages", []):
            if village.get("kind", "zone") == "zone" and village.get("theme") in THEME_FIELD:
                zones[THEME_FIELD[village["theme"]]] = (village["id"], village["name"])
            else:
                hamlets.append(village["name"])
        result[fragment.get("id") or fragment.get("fragment")] = {"zones": zones, "hamlets": hamlets}
    return result


def run_checks(doc_rows, doc_problems):
    """Every rule of docs/narrative/islands.md section 1 that a script can check, plus CSV == document."""
    errors, warnings = list(doc_problems), []
    by_key = {}
    for row in doc_rows:
        if row["key"] in by_key:
            errors.append(f"islands.md:{row['line']}: key {row['key']} already on line {by_key[row['key']]['line']}")
        by_key[row["key"]] = row

    # keys: format and coverage of the 20 fragments
    for key, row in by_key.items():
        if not KEY.match(key):
            errors.append(f"islands.md:{row['line']}: unexpected key {key}")
        elif fragment_of(key) and fragment_of(key) not in FRAGMENTS:
            errors.append(f"islands.md:{row['line']}: {fragment_of(key)} is not in the catalog")
    for key in UI_KEYS + THEME_KEYS + tuple(f"Island.{fid}.{field}" for fid in FRAGMENTS for field in LIMITS):
        if key not in by_key:
            errors.append(f"islands.md: key {key} missing")

    # texts: lengths, forbidden words, punctuation, placeholders
    longest = {}
    for key, row in by_key.items():
        field, fr, en = field_of(key), french(row["fr"]), row["en"]
        where = f"islands.md:{row['line']} {key}"
        if not fr or not en:
            errors.append(f"{where}: empty FR or EN text")
            continue
        for language, text in (("FR", fr), ("EN", en)):
            found = FORBIDDEN.search(text)
            if found:
                errors.append(f"{where}: {language} uses the forbidden word '{found.group(0)}'")
            for char in FORBIDDEN_CHARS:
                if char in text:
                    errors.append(f"{where}: {language} uses '{char}'")
            if field in LIMITS and len(text) > LIMITS[field]:
                errors.append(f"{where}: {language} has {len(text)} characters, {LIMITS[field]} at most")
            if field in LIMITS:
                best = longest.setdefault(f"{field} {language}", {"length": 0})
                if len(text) > best["length"]:
                    longest[f"{field} {language}"] = {"length": len(text), "key": key, "text": text}
        if '"' in fr:
            errors.append(f"{where}: FR uses straight double quotes, use « »")
        if "«" in en or "»" in en:
            errors.append(f"{where}: EN uses « »")
        if set(PLACEHOLDER.findall(fr)) != set(PLACEHOLDER.findall(en)):
            errors.append(f"{where}: FR and EN placeholders differ")
        if field and ("{" in fr or "}" in fr):
            errors.append(f"{where}: an island text has no placeholder")

    # one text per island and per field: no two islands share a name, an epithet...
    for field in LIMITS:
        for language in ("fr", "en"):
            seen = {}
            for fid in FRAGMENTS:
                row = by_key.get(f"Island.{fid}.{field}")
                if not row:
                    continue
                text = row[language].casefold()
                if text in seen:
                    errors.append(f"{field} {language.upper()} '{row[language]}' used by {seen[text]} and {fid}")
                seen[text] = fid

    # villages: form, reserved words, hamlets, generator memory
    alignment = []
    try:
        generator = generator_villages()
    except (OSError, ValueError, KeyError) as error:
        generator = {}
        warnings.append(f"{rel(VILLAGE_INDEX)} unreadable ({error}): villages not compared with the generator")
    hamlet_words = {name.split("-")[0] for fragment in generator.values() for name in fragment["hamlets"]}
    villages = {}
    for fid in FRAGMENTS:
        for field in ("Village.Riches", "Village.Wood", "Village.Food"):
            row = by_key.get(f"Island.{fid}.{field}")
            if not row:
                continue
            name = row["fr"]
            word = name.split("-")[0]
            if word.casefold() in villages:
                errors.append(f"islands.md:{row['line']}: village word '{word}' already used by {villages[word.casefold()]}")
            villages[word.casefold()] = f"{fid}.{field}"
            if not VILLAGE.match(name):
                errors.append(f"islands.md:{row['line']}: village '{name}' is not <Word>-mura")
            if row["en"] != name:
                errors.append(f"islands.md:{row['line']}: village '{name}' must be the same in EN")
            if word in RESERVED_VILLAGE_WORDS:
                errors.append(f"islands.md:{row['line']}: '{name}' is a village of Lvl_Raraka near the real village")
            if word in hamlet_words:
                errors.append(f"islands.md:{row['line']}: '{word}' is already a hamlet (-sato) of the generator")
            zone = generator.get(fid, {}).get("zones", {}).get(field)
            if generator and not zone:
                warnings.append(f"{fid}: the generator has no {THEME_LABEL[field]} zone")
            elif zone and zone[1] != name:
                alignment.append({"fragment": fid, "zone": zone[0], "theme": THEME_LABEL[field], "generator": zone[1],
                                  "string_table": name})

    # the summary table of section 2 repeats the names and epithets
    for number, line in enumerate(DOC.read_text(encoding="utf-8").splitlines(), 1):
        match = SUMMARY.match(line)
        if not match or f"FRG_Raraka_{match.group(1)}" not in FRAGMENTS:
            continue
        fid = f"FRG_Raraka_{match.group(1)}"
        for field, text in (("Name", match.group(2)), ("Epithet", match.group(3))):
            row = by_key.get(f"Island.{fid}.{field}")
            if row and row["fr"] != text:
                errors.append(f"islands.md:{number}: summary {field} of {fid} is '{text}', key says '{row['fr']}'")

    # the CSV is exactly what islands.md generates
    entries, csv_problems = read_csv()
    errors += csv_problems
    expected = expected_entries(doc_rows)
    if entries and entries != expected:
        missing = sorted(set(expected) - set(entries))
        extra = sorted(set(entries) - set(expected))
        changed = sorted(key for key in set(expected) & set(entries) if expected[key] != entries[key])
        errors.append(f"{rel(CSV_PATH)} differs from islands.md (missing {len(missing)}, extra {len(extra)}, "
                      f"changed {len(changed)}, e.g. {(missing + extra + changed)[:5]}): run --write-csv")
    return {"errors": errors, "warnings": warnings, "keys": len(expected), "csv_keys": len(entries),
            "align_generator": alignment, "longest": longest}


def write_json(path, data):
    OUT.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def offline(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write-csv", action="store_true", help="regenerate the CSV from islands.md, then check")
    mode.add_argument("--check", action="store_true", help="check only (default)")
    args = parser.parse_args(argv)
    doc_rows, doc_problems = read_doc()
    written = False
    if args.write_csv:
        if doc_problems:
            print("\n".join(doc_problems))
            return 1
        CSV_PATH.write_text(csv_text(doc_rows), encoding="utf-8-sig", newline="")
        written = True
    result = run_checks(doc_rows, doc_problems)
    report = {"task": "T-092", "mode": "write-csv" if written else "check", "doc": rel(DOC), "csv": rel(CSV_PATH),
              **result, "finished_at": now()}
    write_json(CHECK_REPORT, report)
    sys.stdout.reconfigure(encoding="utf-8")
    print(f"{report['mode']}: {result['keys']} keys in islands.md, {result['csv_keys']} in the CSV, "
          f"{len(result['errors'])} error(s), {len(result['warnings'])} warning(s), "
          f"{len(result['align_generator'])} village name(s) differ from the generator")
    for line in result["errors"] + result["warnings"]:
        print(" -", line)
    print(f"report: {rel(CHECK_REPORT)}")
    return 1 if result["errors"] else 0


# ---------------------------------------------------------------------------------------------------------------- editor


def first_value(result):
    """Out value of a bool UFUNCTION seen from Python: the value, (value,), (True, value) or None."""
    if isinstance(result, tuple):
        if result and isinstance(result[0], bool):
            if not result[0]:
                return None
            result = result[1:]
        return result[0] if result else None
    return result


def unescape(text):
    for escaped, char in ESCAPES:
        text = text.replace(escaped, char)
    return text


def table_entries(ue, table_id):
    """Entries of a String Table (key -> source, notes), read through its CSV export."""
    raw = first_value(ue.StringTableLibrary.export_table_to_csv_string(table_id))
    rows = list(csv.reader(io.StringIO(raw))) if raw else []
    if not rows:
        return {}
    header = rows[0]
    key, source = header.index("Key"), header.index("SourceString")
    notes = header.index("DevNotes") if "DevNotes" in header else None
    entries = {}
    for row in rows[1:]:
        if len(row) > max(key, source):
            entries[unescape(row[key])] = (unescape(row[source]),
                                           unescape(row[notes]) if notes is not None and notes < len(row) else "")
    return entries


def text_reference(ue, text):
    """(table id, key) of an FText taken from a String Table, else None."""
    try:
        result = ue.TextLibrary.string_table_id_and_key_from_text(text)
    except Exception:
        return None
    if isinstance(result, tuple) and result and isinstance(result[0], bool):
        result = result[1:] if result[0] else None
    if isinstance(result, tuple) and len(result) >= 2:
        return str(result[0]), str(result[1])
    return None


def check_editor(ue):
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER")
    if not owner or lock.get("owner") != owner:
        raise RuntimeError(f"editor lock held by {lock.get('owner')}, not by {owner}")
    if dt.datetime.fromisoformat(lock["expires"]) <= dt.datetime.now().astimezone():
        raise RuntimeError("editor lock expired")
    if ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("a game is running in the editor")


def update_string_table(ue, wanted, report):
    eal = ue.EditorAssetLibrary
    created = not eal.does_asset_exist(ST_PATH)
    if created:
        table = ue.AssetToolsHelpers.get_asset_tools().create_asset(ST_NAME, ST_FOLDER, ue.StringTable,
                                                                     ue.StringTableFactory())
    else:
        table = eal.load_asset(ST_PATH)
    if not isinstance(table, ue.StringTable):
        raise RuntimeError(f"{ST_PATH} is not a String Table")
    table_id = ue.StringTableLibrary.get_table_id(table)
    before = table_entries(ue, table_id)
    added = sorted(set(wanted) - set(before))
    removed = sorted(set(before) - set(wanted))
    common = set(wanted) & set(before)
    texts = sorted(key for key in common if wanted[key][0] != before[key][0])
    notes_only = sorted(key for key in common if wanted[key][0] == before[key][0] and wanted[key][1] != before[key][1])
    state = {"path": ST_PATH, "table_id": str(table_id), "created": created,
             "namespace": ue.StringTableLibrary.get_table_namespace(table_id), "keys_before": len(before),
             "added": added, "removed": removed, "changed_text": texts, "changed_notes_only": notes_only}
    report["string_table"] = state
    if not (created or added or removed or texts or notes_only):
        state.update(result="unchanged", keys_after=len(before), saved=False)
        return table_id
    if before:
        state["backup"] = rel(BACKUP) if ue.StringTableLibrary.export_table_to_csv_file(table_id, str(BACKUP)) else None
    try:
        table.modify()
    except Exception:
        pass  # the save below does not need the dirty flag
    state["replaced"] = bool(ue.StringTableLibrary.import_table_from_csv_file(table_id, str(CSV_PATH)))
    after = table_entries(ue, table_id)
    state["keys_after"] = len(after)
    wrong = sorted(key for key in set(wanted) | set(after) if wanted.get(key) != after.get(key))
    if wrong:
        state.update(result="mismatch after replacement, not saved", mismatched=wrong[:20])
        report["errors"].append(f"ST_Islands differs from the CSV after replacement ({len(wrong)} entries), not saved")
        return table_id
    state["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False))
    state["result"] = "updated" if state["saved"] else "updated in memory, save failed"
    if not state["saved"]:
        report["errors"].append(f"{ST_PATH}: save failed")
    return table_id


def update_definitions(ue, table_id, wanted, report):
    eal = ue.EditorAssetLibrary
    table = str(table_id)
    results, missing, credits = [], set(), []
    for fid in FRAGMENTS:
        path = f"{ISLANDS_FOLDER}/DA_Island_Raraka_{fid[-2:]}"
        entry = {"fragment": fid, "asset": path, "fields": {}}
        results.append(entry)
        if not eal.does_asset_exist(path):
            entry["error"] = "asset missing"
            report["errors"].append(f"{path}: asset missing")
            continue
        asset = eal.load_asset(path)
        actual = str(asset.get_editor_property("fragment_id"))
        if actual != fid:
            entry["error"] = f"FragmentId is {actual}"
            report["errors"].append(f"{path}: FragmentId is {actual}, {fid} expected")
            continue
        changed = False
        for prop, field in DEFINITION_TEXTS:
            key = f"Island.{fid}.{field}"
            try:
                current = asset.get_editor_property(prop)
            except Exception:
                missing.add(prop)
                continue
            reference = text_reference(ue, current)
            if reference == (table, key):
                entry["fields"][prop] = {"state": "unchanged", "text": str(current)}
                continue
            if key not in wanted:
                entry["fields"][prop] = {"state": "no key in ST_Islands", "text": str(current)}
                report["errors"].append(f"{path}: {key} missing from ST_Islands")
                continue
            asset.set_editor_property(prop, ue.TextLibrary.text_from_string_table(table_id, key))
            after = asset.get_editor_property(prop)
            ok = text_reference(ue, after) == (table, key)
            entry["fields"][prop] = {"state": "set" if ok else "failed", "before": str(current),
                                     "before_reference": list(reference) if reference else None, "text": str(after)}
            if ok:
                changed = True
            else:
                report["errors"].append(f"{path}: {prop} does not point to {key} after the change")
        if changed:
            entry["saved"] = bool(eal.save_loaded_asset(asset, only_if_is_dirty=False))
            if not entry["saved"]:
                report["errors"].append(f"{path}: save failed")
        try:
            text = str(asset.get_editor_property("data_credits"))
            if re.search(r"raraka|tuamotu|motutapu", text, re.IGNORECASE):
                credits.append(fid)
        except Exception:
            pass
    report["definitions"] = results
    report["definitions_saved"] = [entry["fragment"] for entry in results if entry.get("saved")]
    report["properties_absent"] = sorted(missing)
    if missing:
        report["notes"].append("UExpeditionIslandDefinition has no " + ", ".join(sorted(missing)) +
                               ": skipped (proposal in docs/narrative/islands.md 3.6); run again once the C++ has them")
    if credits:
        report["t139"]["data_credits_name_the_real_atoll"] = credits


def read_catalog(ue, report):
    eal = ue.EditorAssetLibrary
    if eal.does_asset_exist(CATALOG):
        ids = [str(item.get_editor_property("primary_asset_name"))
               for item in eal.load_asset(CATALOG).get_editor_property("fragments")]
        report["catalog"] = {"count": len(ids), "missing": [fid for fid in FRAGMENTS if fid not in ids],
                             "extra": [fid for fid in ids if fid not in FRAGMENTS]}
    if eal.does_asset_exist(OUT_OF_CATALOG):
        asset = eal.load_asset(OUT_OF_CATALOG)
        name = asset.get_editor_property("display_name")
        reference = text_reference(ue, name)
        report["out_of_catalog"] = {"asset": OUT_OF_CATALOG, "display_name": str(name),
                                    "reference": list(reference) if reference else None, "changed": False}


def run_editor():
    import unreal as ue
    report = {"task": "T-092", "mode": "editor", "started_at": now(), "errors": [], "notes": [], "t139": {},
              "csv": rel(CSV_PATH), "doc": rel(DOC)}
    OUT.mkdir(parents=True, exist_ok=True)
    table_id, wanted = None, None
    for step in ("checks", "string_table", "definitions", "catalog"):
        try:
            if step == "checks":
                check_editor(ue)
                doc_rows, doc_problems = read_doc()
                result = run_checks(doc_rows, doc_problems)
                report["check"] = {"errors": result["errors"], "warnings": result["warnings"], "keys": result["keys"],
                                   "align_generator": len(result["align_generator"])}
                report["errors"] += result["errors"]
                if result["errors"]:
                    break
                wanted = expected_entries(doc_rows)
            elif step == "string_table":
                table_id = update_string_table(ue, wanted, report)
                if report["errors"]:
                    break
            elif step == "definitions":
                update_definitions(ue, table_id, wanted, report)
            else:
                read_catalog(ue, report)
        except Exception:
            report["errors"].append(f"{step}: {traceback.format_exc()}")
            break
    report["finished_at"] = now()
    write_json(REPORT, report)
    if report["errors"]:
        ue.log_error(f"T092_ISLAND_NAMES_FAILED: {len(report['errors'])} problem(s), see {rel(REPORT)}")
    else:
        ue.log(f"T092_ISLAND_NAMES_SUCCESS: see {rel(REPORT)}")
    return len(report["errors"])


def main(argv=None):
    try:
        import unreal  # noqa: F401  (present only inside the editor)
    except ModuleNotFoundError:
        return offline(argv)
    problems = run_editor()
    if problems:
        raise RuntimeError(f"island names: {problems} problem(s), see {rel(REPORT)}") from None
    return None


if __name__ == "__main__":
    code = main()
    if code:  # outside the editor only: never leave the editor's Python
        sys.exit(code)
