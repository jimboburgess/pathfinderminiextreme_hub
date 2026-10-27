#pragma once
#include "charactercreation.h"

enum CharacterCreationState
{
    CCS_CLASS_SELECT,
    CCS_WEAPON_STYLE_SELECT,
    CCS_VIEW_STATS,
    CCS_MENU
};

enum CharacterCreationMenuOption
{
    MENU_ACCEPT,
    MENU_REROLL,
    MENU_CHANGE_CLASS,
    MENU_CANCEL
};

// Production creation rules with caller-owned state. Rendering and transitions
// into the adventure are adapters; creating another character never resets a world.
struct CharacterCreationModel
{
    Character preview{};
    CharacterClass selectedClass = CLASS_FIGHTER;
    WeaponGroup selectedWeaponStyle = WEAPON_GROUP_BLADES;
    bool showingPreview = false;
    CharacterCreationState creationState = CCS_CLASS_SELECT;
    CharacterCreationMenuOption selectedMenu = MENU_ACCEPT;

    void enter() {
        selectedClass=CLASS_FIGHTER; selectedWeaponStyle=WEAPON_GROUP_BLADES;
        showingPreview=false; creationState=CCS_CLASS_SELECT;
    }
    void rotateClass(bool clockwise) {
        if(showingPreview)return;
        selectedClass=static_cast<CharacterClass>((selectedClass+(clockwise?1:3))%4);
    }
    void rotateStyle(bool clockwise) {
        if(creationState!=CCS_WEAPON_STYLE_SELECT)return;
        const WeaponGroup styles[]={WEAPON_GROUP_BLADES,WEAPON_GROUP_AXES,WEAPON_GROUP_BLUDGEONS};
        unsigned index=0;while(index<3&&styles[index]!=selectedWeaponStyle)++index;
        selectedWeaponStyle=styles[index<3?(index+(clockwise?1:2))%3:0];
    }
    bool createPreview() {
        if(selectedClass==CLASS_FIGHTER&&creationState==CCS_CLASS_SELECT) {
            selectedWeaponStyle=WEAPON_GROUP_BLADES; creationState=CCS_WEAPON_STYLE_SELECT; return false;
        }
        createCharacter(preview,selectedClass,selectedWeaponStyle);
        showingPreview=true; creationState=CCS_VIEW_STATS;return true;
    }
    void reroll() {if(showingPreview)createCharacter(preview,selectedClass,selectedWeaponStyle);}
    bool accept(Character& destination) {
        if(!showingPreview)return false;
        destination=preview;showingPreview=false;return true;
    }
};
