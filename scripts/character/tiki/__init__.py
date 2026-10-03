"""Tiki creatures: island factions of Crownfall Expeditions (sprite/mob-*.png, pnj-1.png, sniper-1.png).

Entry point: scripts/character/tiki_creatures.py
  kit       geometry: egg bodies, mask plates, strokes, tubes, blobs, ropes, leaves
  mats      materials: CC0 texture sets (art/textures/characters) gradient-mapped to each palette
  creatures the five characters (dimensions, palettes, builders)
  rig       skeleton, skinning, FK / IK pose solver
  anims     provisional animations (idle, walk, run, attack / aim / shoot / reload / cower / talk, hit, death)
  stage     presentation scene (line-up with the bean, the boat and a palm), renders, NLA showcase
  export    FBX for Unreal (skeletal meshes, weapons, animations)
"""
