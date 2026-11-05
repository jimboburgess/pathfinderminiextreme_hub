#pragma once
#include "charactercreation.h"

enum CharacterCreationState
{
    CCS_CLASS_SELECT,
    CCS_WEAPON_STYLE_SELECT,
    CCS_VIEW_STATS,
    CCS_MENU,
    CCS_NAME_ENTRY
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
    void beginNameEntry() {
        if(!showingPreview)return;
        clearCharacterName(preview);selectedMenu=MENU_ACCEPT;
        creationState=CCS_NAME_ENTRY;
    }
    void rotateNameCharacter(bool clockwise) {
        static const char characters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
        const uint8_t count=sizeof(characters)-1;
        const uint8_t index=static_cast<uint8_t>(selectedMenu);
        selectedMenu=static_cast<CharacterCreationMenuOption>((index+(clockwise?1:count-1))%count);
    }
    char currentNameCharacter() const {
        static const char characters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
        return characters[static_cast<uint8_t>(selectedMenu)];
    }
    bool appendNameCharacter(char character) {
        if(creationState!=CCS_NAME_ENTRY || !isSupportedCharacterNameCharacter(character))return false;
        const char* current=hasCharacterName(preview)?getCharacterName(preview):"";
        uint8_t length=0;while(current[length]!='\0'&&length+1<CHARACTER_NAME_SIZE)length++;
        if(length+1>=CHARACTER_NAME_SIZE)return false;
        char updated[CHARACTER_NAME_SIZE]={};
        for(uint8_t index=0;index<length;index++)updated[index]=current[index];
        updated[length]=character;setCharacterName(preview,updated);return true;
    }
    bool appendSelectedNameCharacter() {return appendNameCharacter(currentNameCharacter());}
    bool backspaceName() {
        if(creationState!=CCS_NAME_ENTRY || !hasCharacterName(preview))return false;
        const char* current=getCharacterName(preview);uint8_t length=0;
        while(current[length]!='\0'&&length+1<CHARACTER_NAME_SIZE)length++;
        if(length==0)return false;char updated[CHARACTER_NAME_SIZE]={};
        for(uint8_t index=0;index+1<length;index++)updated[index]=current[index];
        if(length==1)clearCharacterName(preview);else setCharacterName(preview,updated);return true;
    }
    void cancelNameEntry() {if(creationState==CCS_NAME_ENTRY){creationState=CCS_MENU;selectedMenu=MENU_ACCEPT;}}
    bool accept(Character& destination) {
        if(!showingPreview)return false;
        destination=preview;setCharacterName(destination,
            hasCharacterName(preview)?getCharacterName(preview):nullptr);
        showingPreview=false;return true;
    }
};
