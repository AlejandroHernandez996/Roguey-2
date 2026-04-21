# Roguey

## What This Is
A roguelite built in UE5 where combat and progression systems are 
modeled after Old School RuneScape. Player fights through linear 
rooms, loots gear and food, manages a 28-slot inventory, and tries 
to beat a boss. Death restarts the run.
Movement and combat are all based on a grid system just like OSRS.

## Tech
- UE5 source build, C++ primary, Blueprints for UI prototyping
- JetBrains Rider with RiderLink
- Git + LFS

## Core Design Principle
All gameplay runs on a 0.6-second tick system. Combat, eating, 
regen — everything is tick-bound. Do not use UE's native Tick() 
for gameplay logic. Visual interpolation between ticks is fine.

## Combat
All three OSRS combat styles: Melee, Ranged, and Magic. 
The combat triangle applies — each style has strengths and 
weaknesses against the others. Players can switch styles mid-run 
based on loot they find.

## Architecture Guidelines
- Follow standard UE5 conventions — .h and .cpp pairs, UPROPERTY, 
  UFUNCTION, prefix classes with A/U/F/E as appropriate
- Components over inheritance
- Data-driven: items, monsters, and drop tables as UDataAssets
- Items in inventory are data (structs/UObjects), not Actors. 
  Only spawn Actors for ground drops
- Get things working before abstracting. Refactor when patterns emerge


## File Structure
/Source/Roguey/
  Core/       — Tick subsystem, game mode, game instance
  Combat/     — Combat component, hit splats
  Skills/     — Skill component, XP
  Items/      — Data assets, inventory, equipment
  Monsters/   — Data assets, AI
  Dungeon/    — Rooms, floor manager
  UI/         — Widgets

## Current Focus
Week 1 — Project setup, tick system, skill component, basic character