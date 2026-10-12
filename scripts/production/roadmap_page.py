"""Roadmap page of the project (owner 2026-10-11: « affiche-moi vraiment toute la roadmap »).

Reads docs/production/board.md (every T-NNN line: title, agent, status, feature, date) and writes one self-contained HTML
page: the milestones with their estimated dates, the delivery lots of the cahier (§11) with the state of their tickets,
what waits for the owner, and every ticket with filters. Published as an Artifact by the lead; run again after each wave
to refresh it.

usage: python scripts/production/roadmap_page.py <out.html>
"""
import html
import json
import re
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "docs/production/board.md"

STATUS = {
    "fait": "Validé",
    "review": "À vérifier",
    "en-cours": "En cours",
    "a-faire": "À faire",
    "bloque": "Bloqué",
}
STATUS_ORDER = ["fait", "review", "en-cours", "a-faire", "bloque"]

# the delivery lots of the cahier des charges (§11, §20, §24.10), in their order
LOTS = [
    ("L0", "Projet et socle", "Projet, version moteur, architecture, map grise, commandes de build.",
     "Le projet ouvre et compile ; dépendances et limites documentées."),
    ("L1", "Déplacement et objets", "Déplacement, interaction, prise et dépôt.",
     "Deux joueurs prennent des objets distincts ; un même objet n'est jamais dupliqué."),
    ("L2", "Barque", "Barque, sièges, arrimage, capacité, repositionnement entre mouillages.",
     "Stock partagé sans duplication ; cargaison conservée ; nouveau point de retour visible."),
    ("L3", "Expédition", "PvE, zones pacifiables, horloge de submersion, bonus, extraction ; galion au mouillage, départ à la barre.",
     "Bonus unique partagé, l'eau ne baisse jamais, boucle solo puis hôte + client."),
    ("L4", "Hub et économie", "Hub, quotas, taxe, achats, sauvegarde ; zone sûre, vente, manches, boutique, lobby à bord.",
     "Exemple économique exact ; une relance conserve la progression ; le bilan n'est pas rejoué."),
    ("L5", "Équipage et dangers", "4 joueurs, endurance, nuit, lanterne, requin ; HUD, paramètres et pause.",
     "Partie à 4 terminable, danger lisible, états cohérents sur les clients."),
    ("L6", "Build V1", "Build Windows et intégration des personnages ; navigation et packaging vérifiés.",
     "Build joué hors éditeur, procédure d'installation et rapport de recette."),
    ("L7", "En ligne", "Sessions par Internet (héberger, rejoindre en privé) et chat vocal.",
     "Deux machines sur des réseaux distincts se rejoignent et se parlent ; mute et départ propres."),
    ("L8", "Extensions", "Îles variées, événements, factions, catastrophes, améliorations avancées, destruction.",
     "Chaque extension est proposée à part, avec sa recette et son coût, avant d'être intégrée."),
    ("E1", "Éditeur d'îles", "Éditeur local d'îles à partir d'éléments préfabriqués (solo), après L6.",
     "Créer, enregistrer et rouvrir un projet d'île ; aucune progression de campagne touchée."),
]

# milestones: dates estimated by the lead on 2026-10-11 (to be revised as the work goes)
MILESTONES = [
    ("2026-10-11", "done", "v2.0 jouable", "Campagne en jours sur Raraka, forts, crabe géant, boutique du galion, extraction ; solo et coop en réseau local. Packagée sur le Bureau."),
    ("2026-10-12", "next", "v2.2", "Nouvelles armes (tireur d'élite à verrou, mitraillette, mousquet à baïonnette), cage et menottes, trompette sur A, fort corrigé, armes fluides, Veilleurs plus résistants, munitions infinies sur la carte de test."),
    ("2026-10-15", "planned", "Recette du noyau", "104 tickets à vérifier en 13 sessions de test en jeu (~11 h), corrections des bugs trouvés."),
    ("2026-10-30", "planned", "V1 complète (L3 à L6)", "La boucle entière sans trou : galion-lobby, départ, sauvegarde et « Continuer », horloge du jour, 20 fragments en vraies cartes, recette signée."),
    ("2026-11-15", "planned", "En ligne (L7)", "Héberger et rejoindre par Internet (Steam), chat vocal ; après un test sur deux PC."),
    (None, "later", "Éditeur d'îles (E1)", "Après la V1, indépendant du jeu en ligne. Non planifié."),
    (None, "later", "Extensions (L8+)", "Destruction façon Teardown (T-339) et autres chantiers : chiffrés et proposés séparément."),
]

QUESTIONS = [
    ("Commits", "Enregistrer dans git, par domaine, le travail du 6 au 11 octobre (T-164). main n'a pas bougé depuis le 6 ; cela débloque 6 tickets."),
    ("Tickets remplacés", "Clore les 13 tickets que des tickets plus récents remplacent (recette, §3.6)."),
    ("Priorités", "Nouveautés avant la recette du noyau ? Geler le vocal (L7) et la destruction (L8) jusqu'à la validation du noyau ?"),
    ("Test sur deux PC", "Date du test réseau avec le 2e PC (T-133, T-257, T-311) ; un micro pour tester le chat vocal (T-263)."),
]


def parse_board():
    tickets = []
    for line in BOARD.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| T-"):
            continue
        cells = [c.strip() for c in line.split("|")]
        if len(cells) < 8:
            continue
        tid, title, agent, status, feature, when, note = cells[1:8]
        tags = re.findall(r"\[([^\]]+)\]", title)
        lot = "—"
        for tag in tags:
            match = re.match(r"(L\d)", tag)
            if match:
                lot = match.group(1)
                break
            if tag.startswith("E1"):
                lot = "E1"
                break
        if lot == "L9":
            lot = "L8"
        clean = re.sub(r"^(\[[^\]]+\])+\s*", "", title)
        clean = re.sub(r"\s*-\s*owner\s+\d{4}-\d{2}-\d{2}\s*$", "", clean)
        kinds = [t for t in tags if not re.match(r"(L\d|E1|REC|INT)", t)]
        tickets.append({"id": tid, "title": clean, "lot": lot, "status": status if status in STATUS else "a-faire",
                        "agent": agent, "feature": feature, "date": when[:10], "kinds": kinds[:3],
                        "owner": "owner" in title})
    return tickets


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build/roadmap.html"
    tickets = parse_board()
    totals = Counter(t["status"] for t in tickets)
    per_lot = {code: Counter(t["status"] for t in tickets if t["lot"] == code) for code, *_ in LOTS}
    other = Counter(t["status"] for t in tickets if t["lot"] not in {code for code, *_ in LOTS})
    data = {
        "generated": date.today().isoformat(),
        "tickets": tickets,
        "totals": {s: totals.get(s, 0) for s in STATUS_ORDER},
        "lots": [{"code": c, "name": n, "scope": s, "gate": g, "counts": {k: per_lot[c].get(k, 0) for k in STATUS_ORDER}}
                 for c, n, s, g in LOTS],
        "other": {k: other.get(k, 0) for k in STATUS_ORDER},
        "milestones": [{"date": d, "state": st, "name": n, "text": tx} for d, st, n, tx in MILESTONES],
        "questions": [{"title": a, "text": b} for a, b in QUESTIONS],
        "status": STATUS,
        "order": STATUS_ORDER,
    }
    template = (Path(__file__).with_name("roadmap_template.html")).read_text(encoding="utf-8")
    page = template.replace("/*__DATA__*/null", json.dumps(data, ensure_ascii=False).replace("</", "<\\/"))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(page, encoding="utf-8")
    print(f"{out} — {len(tickets)} tickets, " + ", ".join(f"{STATUS[s]} {totals.get(s, 0)}" for s in STATUS_ORDER))


if __name__ == "__main__":
    main()
