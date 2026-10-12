"""Veilleurs « Outlast » (T-274) : Gardien Crabe, Veilleur des Cendres, Guerrier Coco réalistes et inquiétants.

Point d'entrée : scripts/character/tiki_horror.py
  base         chargement de tiki, constructeur de graphes NB, matériaux de base       (rapatrié du 2e PC)
  real_mats    matériaux réalistes de la maquette mob-coco-realiste                     (rapatrié du 2e PC)
  real_geo     base RealCoco et Guerrier Coco réaliste                                  (rapatrié du 2e PC)
  real_rig     budget de polygones et skinning                                          (rapatrié du 2e PC)
  real_gore    démembrement en modules fermés                                           (rapatrié du 2e PC)
  real_ue      UV, cuisson, LOD, FBX                                                    (rapatrié du 2e PC)
  horror_mats  matériaux « Outlast » et couches de coupe du crabe et du Veilleur
  horror_geo   géométrie des trois Veilleurs
  build        chaîne complète par mob, manifeste pour le réimport en place
  render       rendus de contrôle (matériaux cuits)
"""
