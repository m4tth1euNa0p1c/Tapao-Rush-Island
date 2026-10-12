# Boutique du marchand : écran ajusté à la vue, défilement, rayons

- **Tâches** : T-177 (marchand et catalogue), T-264 (images des armes et des munitions), T-320 (cette fiche : fenêtre ajustée, défilement, offres rangées), lot L5 ; cahier §7 (seul l'hôte confirme les achats), §19 (boutique : prix, solde, effet avant/après, raison d'un achat impossible, attente du serveur avant toute baisse locale), §18 (pas d'information portée par la seule couleur), recette RUN09 (boutique) et test « achat refusé » (§12).
- **Demande** (propriétaire, 2026-10-11) : « fix ui du marchand pouvoir adapter la vue et défiler vers le bas, organiser la boutique ».
- **Code** (`Source/monke_island/`) :
  - `Shop/ShopTypes.h` : `EShopCategory` (Auto, Firearm, Blade, Explosive, Ammunition, Tool, Consumable : l'ordre des rayons à l'écran), champ `FShopOffer::Category`, règle pure `ShopRules::Arrange` (rayons dans l'ordre fixe, prix croissant, puis ordre du catalogue) ;
  - `Shop/ShopCatalog` : `ResolveCategory` / `GetOfferCategory` (Auto déduit de l'objet vendu), `FShopScreenLayout Screen` (taille de la fenêtre, réglable dans `DA_ShopCatalog`) ;
  - `UI/Tapao/STapaoShop` : `STapaoShopFrame` (fenêtre ajustée au viewport) et `STapaoShopScreen` (l'écran) ; `UI/Tapao/TapaoShopUISubsystem` : ouverture, fermeture, trésorerie suivie par `URunStateComponent::OnRunStateChanged` ; `UI/Tapao/TapaoStyle` : barre de défilement TAPAO (`ScrollBar`, `ScrollBox`).
- **Tests** : `monke_island.Shop.Arrange` (ordre des rayons, tri par prix, Auto, chaque index une fois), `monke_island.Shop.Category` (déduction, catégorie posée à la main), `monke_island.Shop.Screen` (taille de la fenêtre), plus `Judge` et `Ammo` (inchangés).
- **Script éditeur** : `scripts/editor/shop_categories_ue.py` écrit la catégorie déduite dans les offres de `DA_ShopCatalog` restées sur Auto (idempotent, rapport `monke_island/Saved/T-320/shop-categories.json`).

## Écran
```
              jeu visible derrière un voile
   +---------------------------------------------------------+
   |  Le marchand du galion                                  |  fixe
   |  (o) Trésorerie de l'équipage : 600 couronnes           |
   |  -------------------------------------------------------|
   |  Armes à feu (7) ------------------------------------  ||  défile
   |  |[img] Pistolet            250 c   [ Acheter ]       | ||  (molette,
   |   [img] Mousquet            400 c   [ Acheter ]         ||  barre or
   |   [img] Mitrailleuse       2500 c   [Trop cher]         ||  sur piste
   |         ...              il manque 1900 c               ||  sombre)
   |  Armes blanches (1) ---------------------------------   |
   |  Explosifs (1) / Munitions (9) / Outils et objets (1)   |
   |  -------------------------------------------------------|
   |  Pistolet, 250 c : la trésorerie passera de 600 à 350.  |  fixe
   |  Pistolet acheté : il est dans ton inventaire.          |
   |  [Molette] défiler [↑ ↓] choisir [Pg↑ Pg↓] rayon        |
   |  [Entrée] acheter                    [Fermer (Échap)]   |
   +---------------------------------------------------------+
```
- **Taille** : la fenêtre fait `Screen.Width` (900 unités Slate) de large, au plus `MaxWidthFraction` (92 %) de la largeur de l'écran, et la hauteur de son contenu, au plus `MaxHeightFraction` (86 %) de la hauteur : au-delà, seule la liste défile. Les unités Slate suivent l'échelle DPI du viewport (règle du côté le plus court, 1080 = 1) : mêmes proportions de 1280 × 720 à 2560 × 1440.
- **Rayons** : un titre par catégorie (avec le nombre d'offres), dans l'ordre fixe Armes à feu, Armes blanches, Explosifs, Munitions, Outils et objets, Consommables ; un rayon vide n'apparaît pas. Dans un rayon, prix croissant.
- **Catégorie d'une offre** : champ `Category` de l'offre ; sur Auto, déduite de ce qu'elle donne : paquet de munitions → Munitions ; grenade (`AGrenadeWeapon`) → Explosifs ; arme à feu (`AFirearmWeapon`, `AMusketWeapon`) → Armes à feu ; sabre (`ACutlassWeapon`) → Armes blanches ; classe d'objet nommée *Consumable* → Consommables ; sinon Outils et objets. Un consommable d'inventaire d'une autre classe doit recevoir `Category = Consumable` dans le catalogue.
- **Ligne d'offre** : image (icône de l'objet ou du paquet, sinon nom court), nom, ligne de description (retour à la ligne), prix ; bouton « Acheter », ou « Trop cher » grisé avec « il manque N c » écrit sous le prix.
- **Offre choisie** : contour or plus un trait or à gauche (forme, pas seulement couleur) ; la ligne fixe sous la liste donne la trésorerie avant / après, ou ce qui manque.
- **Achat** : le bouton (ou Entrée / A) envoie l'index du catalogue, jamais la position à l'écran ; le serveur juge (`AShopkeeperActor::Sell`). L'écran affiche « Achat en cours… » puis la réponse ; la trésorerie affichée est celle répliquée, rafraîchie à la réponse et à chaque changement de l'état de partie.

## Commandes
| Entrée | Effet |
|---|---|
| Molette (n'importe où sur l'écran) | Fait défiler la liste. |
| Souris qui bouge sur une ligne / clic gauche | Choisit cette offre. Le bouton droit maintenu fait glisser la liste. |
| ↑ ↓, croix ou stick gauche de la manette, Tab / Maj + Tab | Offre précédente / suivante ; la liste défile pour la garder visible (avec une marge `NavigationPadding` qui laisse voir le titre du rayon). |
| Pg↑ / Pg↓, gâchettes hautes LB / RB | Début du rayon, rayon précédent / suivant (placé en haut de la liste). |
| Début / Fin | Première / dernière offre. |
| Entrée, A de la manette | Achète l'offre choisie, un achat par appui (pas de répétition). Trop chère : affiche le refus sans rien demander au serveur. La barre d'espace n'achète pas. |
| Échap, F, B de la manette, bouton « Fermer » | Ferme. |

## Réglages (`DA_ShopCatalog` → Screen)
| Champ | Défaut | Rôle |
|---|---|---|
| `Width` | 900 | Largeur de la fenêtre quand l'écran a la place. |
| `MaxWidthFraction` | 0,92 | Part maximale de la largeur de l'écran. |
| `MaxHeightFraction` | 0,86 | Part maximale de la hauteur ; au-delà la liste défile. |
| `NavigationPadding` | 64 | Marge gardée autour de l'offre choisie quand le clavier fait défiler. |
| `WheelScrollMultiplier` | 2 | Vitesse de la molette (1 = 32 unités par cran). |

## Vérification (à faire en PIE, éditeur)
1. Galion, F sur le marchand à 1920 × 1080 : rayons dans l'ordre, prix croissants, fenêtre centrée, liste qui défile à la molette jusqu'au bout, en-tête et pied immobiles.
2. Fenêtre PIE en 1280 × 720 puis plus petite (ou `r.SetRes 1024x600w`) : la fenêtre reste dans l'écran, rien ne déborde en bas.
3. Flèches et Pg↑ / Pg↓ : la sélection reste visible ; Entrée achète l'offre choisie ; trésorerie et ligne avant / après mises à jour à la réponse.
4. Offre trop chère : « Trop cher », « il manque N c », Entrée affiche « Pas assez de couronnes dans la trésorerie. ».
5. Hôte + client : un achat du client n'apparaît qu'après la réponse de l'hôte ; la trésorerie suit sur les deux machines.
