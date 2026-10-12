"""T-233/T-234: render the user's 138 audio requirements with separate proof levels.

This is a cue sheet and review template, not another production backlog. Task
status remains in chantier.py. An import report never turns listening green.
"""
from __future__ import annotations

import html
import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs/audio"

# label | source | prepared sound id | implementation evidence | remaining work
# Existing = seen in current source/assets, not newly verified in Unreal.
GROUPS = [
("Environnement maritime", """
Vagues légères contre la coque du galion|waves|HullWater|Code T-234 : présence à bord|PIE et dosage dans la cabine
Vagues contre la barque|waves|HullWater|Existant gardé : SW_Boat_Wake, boucle synthétique de 8 s qui suit la vitesse (HullWater, 2,8 s, bouclerait)|Tester vitesse et arrêt sur sable
Rames entrant dans l'eau|water|Splash_01|Existant gardé : SW_Boat_OarSplash_01, synthèse sur mesure cohérente avec la goutte|Écoute des coups de rame ; Splash_01 reste une alternative
Rames sortant de l'eau|existing_boat||Existant : OarDripSound|Écoute des deux rames
Grincement des rames contre le bois|rowing|OarCreak|Fichier prêt ; geste à raccorder|Ajouter un déclenchement au pivot de rame
Éclaboussures pendant la navigation|water|Splash_02|Existant : sillage ; variantes prêtes|Séparer vagues et gerbes de navigation
Vent léger, fort et tempête|wind|WindStrong|Code T-234 : état de WindStormActor|Tester les fondus et les abris
Tonnerre et éclairs|foley2|Thunder|Code T-234 : tempête finale|Le son n'est pas encore synchronisé à un éclair visuel
Pluie sur le pont et sur l'eau|water|Rain|Code T-234 : tempête finale|Ajouter deux textures et l'occlusion sous le pont
Mouettes et oiseaux marins|gulls||Source trouvée ; non importée|Choisir noddis/sternes/frégates pour Raraka ; mouettes génériques en référence
Bruit sous-marin étouffé|water|Underwater|Code T-234 : caméra sous l'eau|Lit filtré prêt ; filtrage du mix complet restant
Ressac sur la plage et récifs|waves|ShoreSurf|Existant gardé : SW_Shore_Surf (48 s) et SW_Shore_Sea (32 s) ; candidats CC0 de 2 s trop courts|Écoute plage/lagon ; monter une boucle CC0 longue avant tout remplacement
"""),
("Galion et navigation", """
Grincement du bois du galion|wood_metal|WoodCreak_01|Code T-234 : sons espacés à bord|Varier selon roulis ; audition dans la cale
Voiles qui claquent au vent|sails||Source CC0 trouvée ; non téléchargée|Isoler les claquements et les lier au vent
Cordages sous tension|rope||Source CC0 trouvée ; non téléchargée|Nettoyer les oiseaux de fond ; déclencher sous tension
Gouvernail qui tourne|wood_metal|OarCreak|Foley prêt ; déclencheur absent|Boucler uniquement pendant la rotation réelle
Pas sur le pont en bois|existing_steps||Existant : FootstepSubsystem / Wood|Tester les surfaces de tous les ponts
Cloche du galion|foley|Bell|Fichier prêt ; interaction à raccorder|Placement 3D et portée de la cloche
Ancre jetée et remontée|rpg|Chain_01|Foley prêt ; déclencheurs absents|Composer chaîne + eau ; montée/descente distinctes
Chaînes métalliques|rpg|Chain_02|Trois variations préparées|Atténuation et animation de manipulation
Canons : chargement, tir et impact|battle|CannonFire|Six sons prêts ; canon à raccorder|Le fichier audio ne crée pas une mécanique de canon
Coque frappée par de grosses vagues|water|WaterObject|Fichier prêt ; déclencheur absent|Impact selon amplitude des vagues et proximité
Bruit de collision|battle|ShipCollision|Fichier prêt ; déclencheur absent|Seuil d'impulsion et délai anti-répétition
Ambiance de la cabine du capitaine|wood_metal|WoodCreak_02|Code T-234 : ambiance du galion|Créer une zone intérieure filtrée et réverbérée
Départ et arrivée du galion|foley|Bell|Fichier prêt ; transitions à raccorder|Deux signatures sans doubler les alertes
Musique de victoire après un quota atteint|jingles|QuotaMet|Code T-234 : transition du quota|Audition musicale et priorité sur les autres jingles
"""),
("Joueur et mouvements", """
Pas sur sable sec et humide|sand|SandStep_01|Import prévu : 4 pas secs à la place du substitut neige ; humide synthétique gardé (4 variantes)|Tester sec/humide, cadence marche et course
Pas dans l'eau peu profonde|existing_steps||Existant : surface Water|Vérifier profondeur et cadence
Pas sur bois, pierre et végétation|existing_steps||Existant : Wood / Stone / Litter|Vérifier les matériaux des nouvelles maquettes
Course et sprint|existing_steps||Existant : cadence FootstepSubsystem|Écoute marche/course et plusieurs joueurs
Saut et réception|existing_steps||Existant : réception ; saut à compléter|Ajouter effort vocal sans doubler la réception native
Respiration normale et essoufflée|voice||À enregistrer|Deux boucles raccordées à l'endurance
Cris de douleur|pain||Code T-234 : OnHealthChanged|Voix existante provisoire ; remplacer par prise humaine dédiée
Chute et mort du joueur|existing_creatures||Code T-234 : OnDeath|Voix existante provisoire ; valider la couleur du bean
Nage et immersion|sand|SwimEnter|Code T-234 : changement du mode de déplacement|Entrée/sortie prêtes ; mouvements de nage continus à compléter
Ramassage d'un objet|rpg|Gem_01|Code T-234 : augmentation d'inventaire|Tester objet isolé, pile et achat sans doublon
Objet lourd porté, traîné et lâché|wood_metal|WoodHit_01|Code T-234 : prise d'objet|Traînage et impact au sol restent à raccorder
Chute d'un objet dans l'eau|water|WaterObject|Fichier prêt ; entrée d'objet à raccorder|Ne pas déclencher au simple lâcher
Interactions avec portes, coffres et outils|foley|ChestOpen|Fichiers prêts ; interactions à raccorder|Varier ouverture/fermeture/outil
"""),
("Sabres et armes", """
Sabre retiré du fourreau|rpg|BladeDraw|Import prévu : DA_CutlassRules.DrawSound|Écoute du geste à la première et troisième personne
Sabre remis dans le fourreau|rpg|BladeSheathe|Import prévu : DA_CutlassRules.SheatheSound|Écoute du geste complet
Coup de sabre dans l'air|existing_weapons||Existant : SwingSound|Écoute en combat
Impact du sabre sur un ennemi|existing_weapons||Existant : HitSound|Vérifier matériau et absence de doublon
Choc de deux sabres|wood_metal|BladeParry|Import prévu : ParrySound|Tester vraie parade à deux joueurs
Sabre heurtant le bois, la pierre et le métal|wood_metal|MetalHit_01|Banque prête ; routage matériaux à compléter|Trois matériaux distincts
Parade et blocage|wood_metal|BladeParry|Import prévu : ParrySound|Varier blocage et parade réussie
Mousquet : chargement de poudre|existing_weapons||Existant : ReloadSoundCues|Vérifier le timing du rechargement
Mousquet : armement et tir|existing_weapons||Existant : Cock / Lock / FireSound|Écoute proche et distante
Détonation et écho du mousquet|existing_weapons||Existant : FireSound|Ajouter une queue d'écho adaptée à l'espace
Impact de balle selon le matériau|battle|CannonStone|Références d'impacts ; routage à compléter|Les impacts de canon sont trop lourds pour une balle sans montage
Rechargement et manipulation des munitions|existing_weapons||Existant : ReloadSoundCues|Contrôler poudre/papier/balle/baguette
"""),
("Mobs et créatures", """
Crabe : déplacements sur le sable|sand|SandStep_02|Existant : pas de pattes T-261 (StepCue, HeavyStepCue) ; foley T-234 non lié|Écoute T-261 ; DA_GiantCrabRules_Sand encore muet
Crabe : claquement des pinces|wood_metal|CrabClack|Existant : ClackSound / ClackCue T-261 ; foley T-234 non lié|DA_GiantCrabRules_Sand : lui donner les sons T-261
Crabe : attaque et blessure|wood_metal|CrabSlam|Existant : HitSound / SlamSound T-261 ; foley T-234 non lié|La blessure vocale reste à raccorder
Crabe : mort|rpg|CrabHurt|Existant : DeathCue T-261 ; CrabHurt reste une référence|Écoute de la mort du boss (T-261)
Créature hostile : respiration et grognement|existing_creatures||Existant : FTikiVoice.Idle|Écoute et distances
Créature : détection d'un joueur|existing_creatures||Existant : FTikiVoice.Aggro|Détection réelle, pas à chaque tick
Créature : poursuite et attaque|existing_creatures||Existant : FTikiVoice.Attack|Variations de poursuite à compléter
Créature : cri de mort|existing_creatures||Existant : FTikiVoice.Death|Écoute avec les nouvelles animations
Bruits de créatures dans la végétation|rpg|CreatureRare|Référence prête ; émetteurs à placer|Position 3D et espacement
Bruit mystérieux au loin|rpg|ArtifactCurse|Fichier prêt ; événement à raccorder|Pas de répétition rapprochée
Apparition d'une créature rare|rpg|CreatureRare|Fichier prêt ; spawn à raccorder|Une seule signature par apparition
Sons surnaturels liés aux artefacts|rpg|ArtifactPulse|Fichiers prêts ; artefacts à raccorder|Rayon d'écoute et pulsations
"""),
("PNJ, marchand et soldats français", """
PNJ : discussions ambiantes|voice||À enregistrer|Groupes discrets, textes originaux
PNJ : salutations et réactions|voice||À enregistrer|Variantes brèves et espacées
PNJ : cris de peur|existing_creatures||Existant : voix Villager_Flee|Écoute du résultat et contexte
PNJ : alerte lorsqu'un joueur vole un objet|voice||À enregistrer et raccorder|Après confirmation du vol par le serveur
PNJ : fuite et appels à l'aide|voice||Voix Flee existante ; phrases à enregistrer|Séparer cri et appel intelligible
PNJ : bruits des activités quotidiennes|foley|Ceramic_01|Foley disponible ; activités à raccorder|Pas de machines modernes
Marchand : salutation|voice||À enregistrer|Déclencher une fois à l'ouverture du commerce
Marchand : rire et remarques humoristiques|voice||À enregistrer|Plusieurs phrases sans spam
Marchand : bruit de pièces d'or|rpg|Coin_01|Code T-234 : achat confirmé|Tester le solde et l'absence de doublon sonore
Marchand : achat et vente validés|rpg|Coin_02|Code T-234 : achat ; vente à raccorder|Ne sonner qu'après le résultat serveur
Marchand : refus d'une transaction|jingles|QuotaFailed|Code T-234 : résultat de refus|Différencier manque d'argent / inventaire plein
Marchand : réaction à un objet rare|voice||À enregistrer et raccorder|Seuil de rareté réel
Marchand : réaction à un objet sans valeur|voice||À enregistrer et raccorder|Éviter les répétitions
Marchand : réaction si le quota n'est pas atteint|voice||À enregistrer et raccorder|Après le verdict du contrat
Soldats : pas de patrouille|existing_steps||Existant : FootstepSubsystem|Vérifier les soldats et leurs surfaces
Soldats : bruits des équipements militaires|rpg|Chain_03|Foley prêt ; animation à raccorder|Bretelles/bois/métal plutôt que grosse chaîne seule
Soldats : ordres militaires en français|voice||À enregistrer|Textes français originaux
Soldats : détection du joueur|voice||À enregistrer et raccorder|Une alerte par acquisition de cible
Soldats : cris d'alerte|voice||À enregistrer et raccorder|Concurrence entre les soldats proches
Soldats : poursuite et combat|existing_weapons||Existant : SwingSound / HitSound|Compléter les voix françaises
Soldats : tir au mousquet|existing_weapons||Existant : FortSoldierRules.ShotSound|Vérifier la référence et la portée en PIE
Soldats : blessure et mort|voice||À enregistrer et raccorder|Variantes de souffle et douleur sans phrase anglaise
Soldats : recherche d'un joueur caché|voice||À enregistrer et raccorder|Lier au véritable état de recherche
"""),
("Îles, jungle et villages", """
Vent dans les palmiers|wind|WindLight|Code T-234 : vent ambiant|Ajouter le froissement localisé des palmes
Feuilles et branches qui bougent|existing_steps||Référence Litter ; montage à compléter|Pas de confusion entre pas et vent
Oiseaux tropicaux|birds||Source CC0 générique trouvée|Identifier les espèces avant de revendiquer une ambiance de Raraka
Insectes de jour et de nuit|crickets|Crickets|Code T-234 : nuit sur l'île|Ambiance générique de nuit ; couche diurne à compléter
Feu de camp et bois qui craque|fire|Campfire|Boucle prête ; feux à équiper|Actif seulement près d'un feu allumé
Bruits de pêcheurs|voice||À enregistrer et composer|Voix discrètes + matériel de pêche
Filets de pêche manipulés|rope||Référence cordage ; foley à enregistrer|Filet froissé et posé, sans oiseaux de fond
Cabane : porte et plancher|foley|DoorOpen|Pas existants ; porte prête|Raccorder la porte mobile, pas le décor fixe
Pierres déplacées dans les marae|rpg|StoneScrape_01|Fichier prêt ; interaction à raccorder|Déclencher seulement si pierre déplacée
Ambiance des ruines abandonnées|wind|WindLight|Sources disponibles ; zone à créer|Vent filtré et craquements espacés
Bruits nocturnes inquiétants|rpg|ArtifactCurse|Fichier prêt ; événements à raccorder|Sons rares, pas de boucle intrusive
Silence soudain lors d'un événement dangereux|mix||Mixage à implémenter|Atténuer faune et ambiance avec un fondu, puis restaurer
"""),
("Loot et artefacts", """
Ramassage d'un objet commun|rpg|Gem_01|Code T-234 : gain d'inventaire|Distinguer objet de quête et pile
Ramassage d'un objet précieux|rpg|Gem_02|Source prête ; catégorie à raccorder|Ne pas jouer la même valeur pour tout le loot
Tintement de l'or|rpg|Coin_03|Fichier prêt ; achat raccordé en code|Ajouter les pièces physiques et ventes
Bruit des perles et bijoux|rpg|Gem_03|Fichier prêt ; objets à raccorder|Écoute pour différencier nacre et métal
Céramique et poterie|foley|Ceramic_02|Deux fichiers prêts|Choc léger différent de casse
Ouverture et fermeture de coffre|foley|ChestOpen|Deux fichiers prêts|Animation et verrouillage à raccorder
Destruction d'un objet fragile|foley|FragileBreak_01|Fichier prêt ; ValuableBroken à raccorder|Le verre sert de référence ; céramique à différencier
Artefact légendaire découvert|jingles|Legendary|Jingle prêt ; découverte à raccorder|Une seule annonce par découverte
Activation d'une malédiction|rpg|ArtifactCurse|Fichier prêt ; événement à raccorder|Synchroniser avec le véritable effet
Pulsations surnaturelles|rpg|ArtifactPulse|Fichier prêt ; pulsation à programmer|Concurrence et atténuation
Vent et grondements déclenchés par un artefact|wind|WindStorm|Sources prêtes ; artefact à raccorder|Ne pas déclencher une vraie tempête par effet secondaire audio
Dépôt des objets sur le galion|wood_metal|WoodHit_02|Fichier prêt ; dépôt confirmé à raccorder|Son au contact / arrimage, pas au lâcher en l'air
Vente des objets au marchand|rpg|Coin_01|Fichier prêt ; vente à raccorder|Déclenchement après crédit effectif
"""),
("Gameplay, quota et événements", """
Début de la journée|jingles|DayStart|Code T-234 : début d'expédition|Pas de jingle de départ lors d'une connexion tardive
Signal de début des 8 minutes d'expédition|jingles|DayStart|Code T-234 : horloge réelle|Ne modifie pas la durée configurée du gameplay
Avertissement à mi-parcours|foley|Bell|Code T-234 : franchissement du milieu|Une seule fois malgré les bonus de temps
Alerte à 60 secondes restantes|existing_ui||Existant : HUD 60/30/10|Tester seuils et messages visuels
Compte à rebours final|existing_ui||Code T-234 : secondes 9 à 1|Pas de doublon avec l'alerte native à 10 secondes
Signal de retour à la barque|existing_ui||Existant : EvacuationSound|Tester au début de la submersion
Joueur abandonné sur l'île|voice||À composer et raccorder|Après confirmation d'abandon, pas à proximité du rivage
Quota atteint|jingles|QuotaMet|Code T-234 : transition de IsQuotaMet|Ne pas rejouer à chaque réplication
Quota non atteint|jingles|QuotaFailed|Fichier prêt ; verdict à raccorder|Défaite de campagne déjà observée par T-234
Passage au jour suivant|jingles|DayStart|Code T-234 : nouveau monde / expédition|Tester voyage et arrêt des anciennes boucles
Augmentation de la difficulté|jingles|Danger|Jingle prêt ; période à raccorder|Ne pas déclencher à chaque variation du HUD
Apparition d'une tempête|foley2|Thunder|Code T-234 : passage à la tempête finale|Tester son, météo et sécurité du mix
Augmentation de la dangerosité après un vol|jingles|Danger|Jingle prêt ; vol confirmé à raccorder|Un seul signal par changement réel
Fin de partie et écran de résultats|jingles|Defeat|Code T-234 : outcome Victory / Defeat|Tester persistance à travers l'écran de résultats
"""),
("Musique et ambiance dynamique", """
Musique du menu principal|pirate_music|PirateTheme|Alternative CC0 prête ; musique existante conservée|Origine des trois musiques OneDrive à confirmer avant publication
Ambiance calme du lobby sur le galion|solace||Piste candidate CC BY 4.0|Télécharger, créditer et décider quand jouer au galion
Musique d'arrivée sur l'île|helios||Piste candidate CC BY 4.0|Montage court ; musique actuelle déjà gérée à l'arrivée
Exploration paisible|chasing_daylight||Piste candidate CC BY 4.0|Ambiance contemplative, pas une musique de poursuite
Tension lorsqu'un ennemi approche|old_ones||Piste candidate CC BY 4.0|Montage en couches et détection d'ennemi à raccorder
Musique de poursuite|catalyst||Piste candidate CC BY 4.0|Audition, découpe et état de poursuite
Musique des 60 dernières secondes|pirate_music|PirateTheme|Référence CC0 ; arrangement à créer|Version rythmique urgente, sans changement de tempo brutal
Retour triomphal au galion|helios||Piste candidate CC BY 4.0|Stinger d'arrivée après extraction confirmée
Musique de marchand humoristique|jingles|Merchant|Code T-234 : ouverture du commerce|Jingle court ; Sneaky Snitch en alternative longue créditée
Musique de découverte légendaire|jingles|Legendary|Jingle prêt ; découverte à raccorder|Priorité sur ambiance et autres jingles
Ambiance nocturne et surnaturelle|old_ones||Musique Night existante ; candidate CC BY 4.0|Origine de Night à confirmer ; candidate à écouter
Musique de défaite|jingles|Defeat|Code T-234 : défaite de campagne|Audition du jingle ; Hiraeth en alternative longue
Musique de victoire|jingles|Victory|Code T-234 : victoire de campagne|Audition du jingle et ducking de la musique de fond
""")]

EXTRA = {
    "rowing": ("Rowing boat from inside — Fenodyrie", "CC0-1.0", "https://freesound.org/people/Fenodyrie/sounds/588307/"),
    "gulls": ("sea gulls.wav — Snapper4298", "CC0-1.0", "https://freesound.org/people/Snapper4298/sounds/166703/"),
    "sails": ("flag.wav — GeorgeHopkins", "CC0-1.0", "https://freesound.org/people/GeorgeHopkins/sounds/448975/"),
    "rope": ("Hang Man's Rope — jergonda", "CC0-1.0", "https://freesound.org/people/jergonda/sounds/254735/"),
    "pain": ("Ouch.wav — ajanhallinta", "CC0-1.0", "https://freesound.org/people/ajanhallinta/sounds/649543/"),
    "birds": ("Ambient Bird Sounds — isaiah658", "CC0-1.0", "https://opengameart.org/content/ambient-bird-sounds"),
    "existing_steps": ("Pas existants : Kenney + synthèse du projet", "CC0 / original", "https://kenney.nl/assets/impact-sounds"),
    "existing_creatures": ("Voix existantes : rubberduck", "CC0-1.0", "https://opengameart.org/content/80-cc0-creature-sfx"),
    "existing_boat": ("Synthèse originale du projet", "Original du projet", "../../scripts/props/boat_sounds.py"),
    "existing_weapons": ("Synthèse originale du projet", "Original du projet", "../../scripts/props/musket_sounds.py"),
    "existing_ui": ("Alertes originales du projet", "Original du projet", "../../scripts/props/hud_timer_sounds.py"),
    "voice": ("Prises originales en français à enregistrer", "Droits à documenter avec les interprètes", ""),
    "mix": ("Règle de mixage, aucun fichier nécessaire", "Sans objet", ""),
    "solace": ("Solace — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/solace/"),
    "helios": ("Helios — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/helios/"),
    "chasing_daylight": ("Chasing Daylight — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/chasing-daylight/"),
    "old_ones": ("The Old Ones — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/the-old-ones/"),
    "catalyst": ("Catalyst — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/catalyst/"),
    "hiraeth": ("Hiraeth — Scott Buckley", "CC BY 4.0", "https://www.scottbuckley.com.au/library/hiraeth/"),
    "sneaky_snitch": ("Sneaky Snitch — Kevin MacLeod", "CC BY 4.0", "https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100772"),
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / "art/audio/expedition/manifest.json").read_text("utf-8"))
    prepared = {e["id"]: e for e in manifest["sounds"]}
    selected = json.loads((ROOT / "scripts/audio/expedition_sources.json").read_text("utf-8"))["sources"]
    sources = {s["id"]: {"title":s["title"], "license":s["license"], "url":s["page"]} for s in selected}
    sources.update({key:{"title":v[0], "license":v[1], "url":v[2]} for key,v in EXTRA.items()})
    import_path = ROOT / "monke_island/Saved/T234/import-report.json"
    imports = json.loads(import_path.read_text("utf-8")) if import_path.exists() else {}
    imported = {s["id"]:s for s in imports.get("imported", [])}
    build_path = ROOT / "docs/audio/expedition-code-validation.json"
    build = json.loads(build_path.read_text("utf-8")) if build_path.exists() else {}
    events = []
    for group_index, (category, block) in enumerate(GROUPS, 1):
        for row_index, line in enumerate(block.strip().splitlines(), 1):
            label, source_id, sound_id, implementation, remaining = line.split("|")
            if source_id not in sources or sound_id and sound_id not in prepared:
                raise ValueError(line)
            sound = prepared.get(sound_id)
            import_ok = bool(sound and sound_id in imported and imported[sound_id]["wav_sha256"] == sound["sha256"])
            stage = "record" if source_id == "voice" else "code" if implementation.startswith("Code") else "existing" if implementation.startswith("Existant") else "prepared" if sound else "source"
            events.append({"id":f"AUD-{group_index:02}-{row_index:02}", "category":category, "event":label,
                           "source":sources[source_id], "sound_id":sound_id, "implementation":implementation,
                           "remaining":remaining, "stage":stage,
                           "technical_ready":bool(sound and sound["technical_validated"]), "import_verified":import_ok,
                           "gameplay_validated":False, "audio":"../../"+sound["file"] if sound else "",
                           "audio_source":{"title":sound["author"], "url":sound["source_url"], "license":sound["license"]} if sound else None,
                           "unreal_asset":sound["asset"] if sound else "", "seconds":sound["seconds"] if sound else None})
    assert len(events) == 138, len(events)
    assert len({e["id"] for e in events}) == 138
    data = {"schema":1, "generated_at":datetime.now(timezone.utc).isoformat(), "tasks":["T-233", "T-234"],
            "events":events, "sounds":len(prepared), "banks_downloaded":len(selected), "sources":sources,
            "notice":"Préparation technique vérifiée. Import, écoute PIE, build et multijoueur sont des preuves distinctes. Aucun événement n'est déclaré validé en jeu par ce catalogue.",
            "import_success":bool(imports.get("success")), "gameplay_validated":False,
            "game_target_compiled":bool(build.get("success")), "editor_target_compiled":bool(build.get("editor_target_compiled"))}
    (OUT / "expedition-audio-catalogue.json").write_text(json.dumps(data, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    template = Path(__file__).with_name("expedition_template.html").read_text("utf-8")
    payload = json.dumps(data, ensure_ascii=False).replace("<", "\\u003c")
    (OUT / "expedition-audio.html").write_text(template.replace("/*CATALOGUE_DATA*/null", payload), encoding="utf-8")
    print(f"138 events / 10 themes; {len(prepared)} prepared WAV; {sum(e['import_verified'] for e in events)} rows with import evidence; 0 PIE validation claims")
    print(json.dumps(dict(Counter(e['category'] for e in events)), ensure_ascii=False))


if __name__ == "__main__":
    main()
