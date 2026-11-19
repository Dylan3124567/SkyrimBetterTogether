ScriptName QF_MQ101_0003372B Extends Quest hidden

; Shared Helgen: host keeps the vanilla execution role; guests follow and watch.
Bool sharedGuestStarted = False

;-- Variables ---------------------------------------

;-- Properties --------------------------------------
ObjectReference Property AlduinAttackEnableMarker1 Auto
{ enable marker for first part of Alduin attack }
Scene Property AlduinAttackScene Auto
Weather Property AlduinAttackWeather Auto
ReferenceAlias Property Alias_Alduin Auto
ReferenceAlias Property Alias_BarracksRoomSoldier01 Auto
ReferenceAlias Property Alias_BarracksRoomSoldier01StartMarker Auto
ReferenceAlias Property Alias_BarracksRoomSoldier02 Auto
ReferenceAlias Property Alias_BarracksRoomSoldier02StartMarker Auto
ReferenceAlias Property Alias_Bear Auto
ReferenceAlias Property Alias_Cart1 Auto
ReferenceAlias Property Alias_Cart2 Auto
ReferenceAlias Property Alias_CartHorse1 Auto
ReferenceAlias Property Alias_CartHorse2 Auto
ReferenceAlias Property Alias_CivilianGunnar Auto
ReferenceAlias Property Alias_CivilianIngrid Auto
ReferenceAlias Property Alias_CivilianMatlara Auto
ReferenceAlias Property Alias_CivilianTorolf Auto
ReferenceAlias Property Alias_CivilianTorri Auto
ReferenceAlias Property Alias_CivilianVilod Auto
ReferenceAlias Property Alias_DeadStormcloakHeadtrackMarker Auto
ReferenceAlias Property Alias_DragonLookMarker2 Auto
ReferenceAlias Property Alias_DragonLookMarker3 Auto
ReferenceAlias Property Alias_Elenwen Auto
ReferenceAlias Property Alias_ElenwenStartMarker Auto
ReferenceAlias Property Alias_GeneralTullius Auto
ReferenceAlias Property Alias_GunjarCorpse Auto
ReferenceAlias Property Alias_Hadvar Auto
ReferenceAlias Property Alias_HadvarHorse Auto
ReferenceAlias Property Alias_HadvarStartMarker Auto
ReferenceAlias Property Alias_HallwayStormcloak01 Auto
ReferenceAlias Property Alias_HallwayStormcloak02 Auto
ReferenceAlias Property Alias_HallwayStormcloak03 Auto
ReferenceAlias Property Alias_HallwayStormcloakStartMarker1 Auto
ReferenceAlias Property Alias_HallwayStormcloakStartMarker2 Auto
ReferenceAlias Property Alias_HallwayStormcloakStartMarker3 Auto
ReferenceAlias Property Alias_Headsman Auto
ReferenceAlias Property Alias_HeadsmanStartMarker Auto
LocationAlias Property Alias_Helgen Auto
ReferenceAlias Property Alias_HelgenArcher01 Auto
ReferenceAlias Property Alias_HelgenArcher02 Auto
ReferenceAlias Property Alias_HelgenArcher03 Auto
ReferenceAlias Property Alias_HelgenArcher04 Auto
ReferenceAlias Property Alias_ImperialSoldier01 Auto
ReferenceAlias Property Alias_ImperialSoldier02 Auto
ReferenceAlias Property Alias_ImperialSoldierFort01 Auto
ReferenceAlias Property Alias_ImperialSoldierHelgen01 Auto
ReferenceAlias Property Alias_ImperialSoldierHelgen02 Auto
ReferenceAlias Property Alias_Justiciar01 Auto
ReferenceAlias Property Alias_Justiciar02 Auto
ReferenceAlias Property Alias_JusticiarMarker1 Auto
ReferenceAlias Property Alias_JusticiarMarker2 Auto
ReferenceAlias Property Alias_KeepDoorInteriorImperials Auto
ReferenceAlias Property Alias_KeepDoorInteriorStormcloaks Auto
ReferenceAlias Property Alias_KeepLever1 Auto
ReferenceAlias Property Alias_KeepLever2 Auto
ReferenceAlias Property Alias_KeepLockedCageDoor1 Auto
ReferenceAlias Property Alias_KeepLockedDoor1 Auto
ReferenceAlias Property Alias_LockedCage Auto
ReferenceAlias Property Alias_ObjectiveAlleyway01 Auto
ReferenceAlias Property Alias_ObjectiveInn01 Auto
ReferenceAlias Property Alias_ObjectiveKeepCourtyard Auto
ReferenceAlias Property Alias_ObjectiveKeepDoorImperial Auto
ReferenceAlias Property Alias_ObjectiveKeepDoorStormcloaks Auto
ReferenceAlias Property Alias_ObjectiveKeepEscape1 Auto
ReferenceAlias Property Alias_ObjectiveKeepEscape2 Auto
ReferenceAlias Property Alias_ObjectiveKeepEscape3 Auto
ReferenceAlias Property Alias_ObjectiveKeepEscape4 Auto
ReferenceAlias Property Alias_ObjectiveKeepEscape5 Auto
ReferenceAlias Property Alias_ObjectiveKeepEscapeExit Auto
ReferenceAlias Property Alias_Player Auto
ReferenceAlias Property Alias_PlayerFriend Auto
ReferenceAlias Property Alias_PlayerStartMarker Auto
ReferenceAlias Property Alias_PoolRoomEnemy1StartMarker Auto
ReferenceAlias Property Alias_PoolRoomEnemy2StartMarker Auto
ReferenceAlias Property Alias_PoolRoomEnemy3StartMarker Auto
ReferenceAlias Property Alias_PoolRoomEnemy4StartMarker Auto
ReferenceAlias Property Alias_PoolRoomEnemy5StartMarker Auto
ReferenceAlias Property Alias_PoolRoomImperial1 Auto
ReferenceAlias Property Alias_PoolRoomImperial2 Auto
ReferenceAlias Property Alias_PoolRoomImperial3 Auto
ReferenceAlias Property Alias_PoolRoomImperial4 Auto
ReferenceAlias Property Alias_PoolRoomImperial5 Auto
ReferenceAlias Property Alias_PoolRoomStormcloak1 Auto
ReferenceAlias Property Alias_PoolRoomStormcloak2 Auto
ReferenceAlias Property Alias_PoolRoomStormcloak3 Auto
ReferenceAlias Property Alias_PoolRoomStormcloak4 Auto
ReferenceAlias Property Alias_PoolRoomStormcloak5 Auto
ReferenceAlias Property Alias_Priest Auto
ReferenceAlias Property Alias_PriestStartMarker Auto
ReferenceAlias Property Alias_Prisoner01 Auto
ReferenceAlias Property Alias_PrisonerMarker1 Auto
ReferenceAlias Property Alias_Ralof Auto
ReferenceAlias Property Alias_RalofStartMarker Auto
ReferenceAlias Property Alias_SoldierMarker1 Auto
ReferenceAlias Property Alias_SoldierMarker2 Auto
ReferenceAlias Property Alias_SoldierMarkerFort1 Auto
ReferenceAlias Property Alias_SoldierMarkerHelgen1 Auto
ReferenceAlias Property Alias_SoldierMarkerHelgen2 Auto
ReferenceAlias Property Alias_SoldierMarkerInsideFort1 Auto
ReferenceAlias Property Alias_Spider1 Auto
ReferenceAlias Property Alias_Spider2 Auto
ReferenceAlias Property Alias_Spider3 Auto
ReferenceAlias Property Alias_Spider4 Auto
ReferenceAlias Property Alias_Spider5 Auto
ReferenceAlias Property Alias_StoreoomBarrel Auto
ReferenceAlias Property Alias_StoreroomImperial1 Auto
ReferenceAlias Property Alias_StoreroomImperial2 Auto
ReferenceAlias Property Alias_StoreroomStormcloak1 Auto
ReferenceAlias Property Alias_StoreroomStormcloak2 Auto
ReferenceAlias Property Alias_StormcloakPrisoner01 Auto
ReferenceAlias Property Alias_StormcloakPrisoner02 Auto
ReferenceAlias Property Alias_StormcloakPrisoner03 Auto
ReferenceAlias Property Alias_StormcloakPrisoner04 Auto
ReferenceAlias Property Alias_StormcloakPrisoner1StartMarker Auto
ReferenceAlias Property Alias_StormcloakPrisoner2StartMarker Auto
ReferenceAlias Property Alias_StormcloakPrisoner3StartMarker Auto
ReferenceAlias Property Alias_StormcloakPrisoner4StartMarker Auto
ReferenceAlias Property Alias_TortureRoomImperialMarker1 Auto
ReferenceAlias Property Alias_TortureRoomImperialMarker2 Auto
ReferenceAlias Property Alias_TortureRoomImperialMarker3 Auto
ReferenceAlias Property Alias_TortureRoomImperialSoldier1 Auto
ReferenceAlias Property Alias_TortureRoomImperialSoldier2 Auto
ReferenceAlias Property Alias_TortureRoomImperialSoldier3 Auto
ReferenceAlias Property Alias_TortureRoomStormcloak1 Auto
ReferenceAlias Property Alias_TortureRoomStormcloak1StartMarker Auto
ReferenceAlias Property Alias_TortureRoomStormcloak2 Auto
ReferenceAlias Property Alias_TortureRoomStormcloak2StartMarker Auto
ReferenceAlias Property Alias_TortureRoomStormcloak3 Auto
ReferenceAlias Property Alias_TortureRoomStormcloak3StartMarker Auto
ReferenceAlias Property Alias_Torturer Auto
ReferenceAlias Property Alias_TrophyRoomPrisoner01 Auto
ReferenceAlias Property Alias_TrophyRoomPrisoner01StartMarker Auto
ReferenceAlias Property Alias_TrophyRoomPrisoner02 Auto
ReferenceAlias Property Alias_TrophyRoomPrisoner02StartMarker Auto
ReferenceAlias Property Alias_TulliusHorse Auto
ReferenceAlias Property Alias_TulliusStartMarker Auto
ReferenceAlias Property Alias_Ulfric Auto
ReferenceAlias Property Alias_UlfricStartMarker Auto
ReferenceAlias Property Alias_WardenContainer Auto
Armor Property ArmorGag Auto
ImageSpaceModifier Property BlackoutImod Auto
{ when dragon attacks, apply this for transition to player movement }
ObjectReference Property BlockGateCollisionMarker Auto
Idle Property BoundStandingCut Auto
helgenhallcollapsescript Property BridgeCollapseTrigger Auto
{ possibly temp, holds script to trigger dust, stagger, etc. for bridge collapse }
ObjectReference Property BridgeDebris Auto
ObjectReference Property BridgeDragonRoarMarker Auto
ObjectReference Property BridgeOriginal Auto
Faction Property CaptiveFaction Auto
{ faction for caged prisoners in keep }
ObjectReference Property Cart1Marker4 Auto
ObjectReference Property Cart1Marker5 Auto
ObjectReference Property Cart2Marker4 Auto
ObjectReference Property Cart2Marker5 Auto
ObjectReference Property CartHorse1Marker4 Auto
ObjectReference Property CartHorse1Marker5 Auto
ObjectReference Property CartHorse2Marker4 Auto
ObjectReference Property CartHorse2Marker5 Auto
ObjectReference Property CartPathAmbientMarker Auto
Sound Property CartSound Auto
{ moving cart sound }
Message Property CharGenActivateBody Auto
Message Property CharGenActivateOpen Auto
Message Property CharGenAttack Auto
Message Property CharGenAttackwithPower Auto
Message Property CharGenBloc Auto
Message Property CharGenBlock Auto
Message Property CharGenBlock02 Auto
Message Property CharGenLockpick Auto
Message Property CharGenLook Auto
Message Property CharGenLookPC Auto
Message Property CharGenMove Auto
Message Property CharGenMovePC Auto
Message Property CharGenOpenInventory Auto
Message Property CharGenPowerAttack Auto
Message Property CharGenReadyWeapon Auto
Message Property CharGenSneak Auto
ObjectReference Property CiviliansOutsideHelgenMarker Auto
Armor Property ClothesPrisoner Auto
Armor Property ClothesPrisonerShoes Auto
ObjectReference Property CollapsingBridge Auto
{ bridge that separates player and friend from other allies }
ObjectReference Property CollapsingBridgeAnim Auto
Sound Property DragonRoarDistant01 Auto
Sound Property DragonRoarDistant02 Auto
ObjectReference Property EastGate Auto
{ east gate of Helgen (close second) }
MusicType Property ExitMusic Auto
ObjectReference Property ExplosionTriggerA01 Auto
ObjectReference Property ExtHelgenAttackASREF Auto
ImageSpaceModifier Property FadeUpFromBlack Auto
ObjectReference Property FortGuard1Marker5 Auto
{ town square start marker }
ObjectReference Property FortGuard1Marker8 Auto
ObjectReference Property FortNeugradEnableMarker Auto
{ marker controlling Fort Neugrad actors }
GlobalVariable Property GameHour Auto
ObjectReference Property Guard1KeepMarker1 Auto
ObjectReference Property Guard1Marker4 Auto
ObjectReference Property Guard1Marker5 Auto
ObjectReference Property Guard1Marker8 Auto
ObjectReference Property Guard2KeepMarker1 Auto
ObjectReference Property Guard2Marker4 Auto
ObjectReference Property Guard2Marker5 Auto
ObjectReference Property Guard2Marker8 Auto
ObjectReference Property HadvarHorseMarker4 Auto
ObjectReference Property HadvarKeepMarker1 Auto
ObjectReference Property HadvarMarker4 Auto
ObjectReference Property HadvarMarker5 Auto
ObjectReference Property HadvarMarker8 Auto
Outfit Property HadvarOutfit Auto
{ outfit Hadvar should wear by the end of chargen }
ObjectReference Property HadvarRiverwoodMarker Auto
{ where is Hadvar moved at start of game (TEMP) }
ObjectReference Property HelgenCrowdWallaMarker1 Auto
ObjectReference Property HelgenCrowdWallaMarker2 Auto
ObjectReference Property HelgenEndMarker Auto
{ temp marker for starting player at end of Helgen keep }
ObjectReference Property HelgenFriendMarker Auto
{ temp marker to quickstart Hadvar/Ralof outside Helgen keep exit }
LocationAlias Property HelgenReserved Auto
Idle Property IdleCartDriverSway Auto
Idle Property IdleCartPrisonerASway Auto
Idle Property IdleCartPrisonerBSway Auto
Idle Property IdleCartPrisonerCIdle Auto
Idle Property IdleCartPrisonerCSway Auto
Idle Property IdleCartPrisonerDSway Auto
Idle Property IdleCaryPrisonerDSway Auto
Idle Property IdleExecutionerAlduinReaction Auto
Idle Property IdleExecutionerAlduinReactionDeath Auto
Weapon Property ImperialBow Auto
Weapon Property ImperialSword Auto
Scene Property IntoTorriScene Auto
MusicType Property IntroMusic Auto
Ammo Property IronArrow Auto
ObjectReference Property KeepGate1 Auto
{ first gate from Imperial barracks room }
Scene Property KeepIntroSceneA Auto
{ Imperial intro scene }
Scene Property KeepIntroSceneB Auto
{ Stormcloak intro scene }
Scene Property KeepScene2A Auto
Scene Property KeepScene2B Auto
Scene Property KeepScene4A Auto
{ Imperial version }
Scene Property KeepScene4B Auto
Scene Property KeepScene5A Auto
{ Imperial version }
Scene Property KeepScene5B Auto
MiscObject Property Lockpick Auto
Weapon Property Longbow Auto
ObjectReference Property MQ101AlduinTowerAttackTowerREF Auto
ObjectReference Property MQ101CrowdOhREF Auto
ObjectReference Property MQ101CrowdWalla1REF Auto
ObjectReference Property MQ101CrowdWalla2REF Auto
ObjectReference Property MQ101CrowdWallaMarker1 Auto
Quest Property MQ101DragonAttack Auto
ObjectReference Property MQ101DragonShoutsMarkerREF Auto
ObjectReference Property MQ101DragonStormcallMarker Auto
Faction Property MQ101ImperialFaction Auto
Scene Property MQ101KeepEnemyScene1A Auto
Scene Property MQ101KeepEnemyScene1B Auto
Scene Property MQ101KeepEnemyScene2A Auto
Scene Property MQ101KeepEnemyScene2B Auto
Faction Property MQ101LokirEnemyFaction Auto
ObjectReference Property MQ101OutsideClutterMarker Auto
Faction Property MQ101StormcloakFaction Auto
Quest Property MQ102 Auto
Quest Property MQ102A Auto
Quest Property MQ102B Auto
ObjectReference Property MQ102ChargenFriendExitMarker Auto
MusicType Property MUSCombatBossChargen Auto
MusicType Property MUSDungeonChargen Auto
Sound Property NPCDragonFlightRoar Auto
Sound Property NPCDragonFlightRoarObstructed Auto
ObjectReference Property NorthGate Auto
{ north gate of town }
Idle Property OffsetBoundStandingStart Auto
Idle Property OffsetStop Auto
ObjectReference Property Phase2RespawnMarker1 Auto
ObjectReference Property Phase2RespawnMarker2 Auto
ObjectReference Property Phase3RespawnMarker1 Auto
ObjectReference Property Phase3RespawnMarker2 Auto
ObjectReference Property Phase4RespawnMarker1 Auto
ObjectReference Property Phase4RespawnMarker2 Auto
Faction Property PlayerFaction Auto
ObjectReference Property PlayerImperialKeepMarker1 Auto
ObjectReference Property PlayerInnMoveMarker Auto
{ temp - move player into inn from tower }
ObjectReference Property PlayerKeepStartMarker Auto
ObjectReference Property PlayerMarker4 Auto
ObjectReference Property PlayerMarker5 Auto
ObjectReference Property PlayerMarker8 Auto
ObjectReference Property PlayerSonKeepMarker1 Auto
ObjectReference Property PlayerStartMarker Auto
{ where should the player start for New Game? }
ObjectReference Property PostMQ101KeepMarker Auto
Armor Property PrisonerCuffs Auto
Armor Property PrisonerCuffsPlayer Auto
ObjectReference Property PrisonerMarker4 Auto
ObjectReference Property PrisonerMarker5 Auto
Outfit Property PrisonerOutfit Auto
ObjectReference Property RalofKeepExteriorMarker Auto
ObjectReference Property RalofKeepMarker1 Auto
ObjectReference Property RalofMarker4 Auto
ObjectReference Property RalofMarker5 Auto
ObjectReference Property RalofMarker8 Auto
Outfit Property RalofOutfit Auto
{ outfit for Ralof to change into at the end of chargen }
Scene Property Scene1 Auto
Scene Property Scene2 Auto
{ Elenwen scene to town square }
Scene Property Scene3 Auto
{ town square through Alduin attacking }
Scene Property Scene4 Auto
GlobalVariable Property SittingAngleLimit Auto
Weather Property SkyrimClear Auto
ObjectReference Property Son1KeepMarker1 Auto
ObjectReference Property Son1Marker4 Auto
ObjectReference Property Son1Marker5 Auto
ObjectReference Property Son2KeepMarker1 Auto
ObjectReference Property Son2Marker4 Auto
ObjectReference Property Son2Marker5 Auto
ObjectReference Property Son3Marker4 Auto
ObjectReference Property Son3Marker5 Auto
ObjectReference Property Son4Marker4 Auto
ObjectReference Property Son4Marker5 Auto
Weapon Property SteelDagger Auto
ObjectReference Property StormcloakPrisoner1Marker8 Auto
ObjectReference Property StormcloakPrisoner2Marker8 Auto
ObjectReference Property StormcloakPrisoner3Marker8 Auto
ObjectReference Property TEMPCartRiders Auto
ObjectReference Property TEMPEndGate Auto
{ temporary gate at end }
Message Property TempChooseSidesMessage Auto
Message Property TempChooseSidesMessage2 Auto
Message Property TempMessage1 Auto
{ cart stops message }
Message Property TempMessage2 Auto
{ cart stops message }
Faction Property TemporaryFriendsFaction Auto
{ makes everyone in chargen friends at start }
ObjectReference Property TorolfMarker8 Auto
ObjectReference Property TorriMarker8 Auto
Book Property TorturerSpellTome Auto
ObjectReference Property TowerDoorExterior Auto
ObjectReference Property TulliusKeepMarker1 Auto
ObjectReference Property TulliusMarker4 Auto
ObjectReference Property TulliusMarker5 Auto
ObjectReference Property TulliusMarker8 Auto
ObjectReference Property UlfricKeepMarker1 Auto
ObjectReference Property UlfricMarker4 Auto
ObjectReference Property UlfricMarker5 Auto
ObjectReference Property UlfricMarker8 Auto
Outfit Property UlfricOutfit Auto
WordOfPower Property WordFus Auto
WordOfPower Property WordRo Auto
Spell Property dunCGDragonVoiceStormCall Auto

;-- Functions ---------------------------------------

Function Fragment_103()
  ; Stage 265 also reaches delayed guests that missed the first keep chain activation.
  default2stateactivator gate = KeepGate1 as default2stateactivator
  gate.SetOpen(True)
  ; This gate is doOnce in Skyrim.esm; a late activation must not close it again.
  gate.GotoState("done")
EndFunction

Function Fragment_122()
  ; Empty function
EndFunction

Function Fragment_128()
  ; Empty function
EndFunction

Function Fragment_129()
  ; Empty function
EndFunction

Function Fragment_136()
  ; Empty function
EndFunction

Function Fragment_144()
  ; Empty function
EndFunction

Function Fragment_169()
  ; Empty function
EndFunction

Function Fragment_173()
  ; The lever stage is synced even when its activation-parent event is not.
  dunCGdefault2StateActivator bridge = CollapsingBridge as dunCGdefault2StateActivator
  If bridge != None
    bridge.SetOpen(False)
    ; Preserve this bridge's vanilla doOnce state before a delayed activation can toggle it.
    bridge.GotoState("done")
  EndIf
EndFunction

Function Fragment_185()
  ; Empty function
EndFunction

Function Fragment_195()
  ; Empty function
EndFunction

Function Fragment_202()
  ; Empty function
EndFunction

Function Fragment_206()
  ; Empty function
EndFunction

Function Fragment_210()
  ; Empty function
EndFunction

Function Fragment_230()
  ; Empty function
EndFunction

Function Fragment_240()
  ; Empty function
EndFunction

Function Fragment_249()
  ; Empty function
EndFunction

Function Fragment_253()
  ; Empty function
EndFunction

Function Fragment_259()
  ; Empty function
EndFunction

Function Fragment_280()
  ; Empty function
EndFunction

Function Fragment_282()
  ; Empty function
EndFunction

Function Fragment_283()
  ; Empty function
EndFunction

Function Fragment_284()
  ; Empty function
EndFunction

Function Fragment_285()
  ; Empty function
EndFunction

Function Fragment_286()
  ; Empty function
EndFunction

Function Fragment_290()
  ; Empty function
EndFunction

Function Fragment_291()
  ; Empty function
EndFunction

Function Fragment_313()
  ; Empty function
EndFunction

Function Fragment_320()
  ; Empty function
EndFunction

Function Fragment_321()
  ; Empty function
EndFunction

Function Fragment_323()
  ; Empty function
EndFunction

Function Fragment_33()
  ; Empty function
EndFunction

Function Fragment_50()
  ; Empty function
EndFunction

Function Fragment_52()
  ; Empty function
EndFunction

Function Fragment_69()
  ; Empty function
EndFunction

Function Fragment_72()
  ; Empty function
EndFunction

Function Fragment_87()
  ; Empty function
EndFunction

Function Fragment_99()
  ; Empty function
EndFunction

; Skipped compiler generated GetState

; Skipped compiler generated GotoState

Function Fragment_146()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:688
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:689
  Self.SetStage(365) ; #DEBUG_LINE_NO:693
EndFunction

Function Fragment_197()
  Game.ShowFirstPersonGeometry(False) ; #DEBUG_LINE_NO:703
  Alias_CartHorse1.GetRef().Enable(False) ; #DEBUG_LINE_NO:705
  Game.SetPlayerAIDriven(True) ; #DEBUG_LINE_NO:707
  Game.GetPlayer().RemoveAllItems(None, False, False) ; #DEBUG_LINE_NO:709
  Game.GetPlayer().EquipItem(ClothesPrisoner as Form, False, False) ; #DEBUG_LINE_NO:711
  Game.GetPlayer().EquipItem(ClothesPrisonerShoes as Form, False, False) ; #DEBUG_LINE_NO:712
EndFunction

Function Fragment_48()
  KeepIntroSceneB.Start() ; #DEBUG_LINE_NO:721
  MQ101StormcloakFaction.SetAlly(PlayerFaction, False, False) ; #DEBUG_LINE_NO:723
  Alias_PlayerFriend.ForceRefTo(Alias_Ralof.GetRef()) ; #DEBUG_LINE_NO:725
  Alias_BarracksRoomSoldier01.TryToEnable() ; #DEBUG_LINE_NO:727
  Alias_BarracksRoomSoldier02.TryToEnable() ; #DEBUG_LINE_NO:728
  Alias_Hadvar.TryToDisable() ; #DEBUG_LINE_NO:730
EndFunction

Function Fragment_179()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:738
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:739
  kmyQuest.ExitCart(Alias_StormcloakPrisoner04, 1) ; #DEBUG_LINE_NO:744
  kmyQuest.ExitCart(Alias_StormcloakPrisoner02, 2) ; #DEBUG_LINE_NO:745
  kmyQuest.ExitCart(Alias_StormcloakPrisoner03, 3) ; #DEBUG_LINE_NO:746
  kmyQuest.ExitCart(Alias_StormcloakPrisoner01, 4) ; #DEBUG_LINE_NO:747
EndFunction

Function Fragment_61()
  Alias_KeepLockedDoor1.GetRef().Lock(False, False) ; #DEBUG_LINE_NO:756
EndFunction

Function Fragment_59()
  Self.SetStage(0) ; #DEBUG_LINE_NO:773
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:774
  Self.SetStage(10) ; #DEBUG_LINE_NO:775
  Alias_Hadvar.TryToMoveTo(HadvarKeepMarker1) ; #DEBUG_LINE_NO:781
  Alias_Ralof.TryToMoveTo(RalofKeepMarker1) ; #DEBUG_LINE_NO:782
  Alias_Ralof.GetActorRef().RemoveItem(PrisonerCuffs as Form, 1, False, None) ; #DEBUG_LINE_NO:783
  Alias_BarracksRoomSoldier01.TryToEnable() ; #DEBUG_LINE_NO:784
  Alias_BarracksRoomSoldier02.TryToEnable() ; #DEBUG_LINE_NO:785
  Scene1.Stop() ; #DEBUG_LINE_NO:797
  Scene2.Stop() ; #DEBUG_LINE_NO:798
  Scene3.Stop() ; #DEBUG_LINE_NO:799
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:800
  Self.SetStage(180) ; #DEBUG_LINE_NO:801
  Self.SetStage(190) ; #DEBUG_LINE_NO:802
  Self.SetStage(195) ; #DEBUG_LINE_NO:803
  Game.GetPlayer().moveto(PlayerSonKeepMarker1, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:805
  Game.DisablePlayerControls(True, True, True, False, False, True, True, False, 0) ; #DEBUG_LINE_NO:808
  Game.EnablePlayerControls(True, False, False, True, True, True, False, True, 0) ; #DEBUG_LINE_NO:809
EndFunction

Function Fragment_21()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:825
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:826
  kmyQuest.FactionPath = 1 ; #DEBUG_LINE_NO:830
  Self.SetStage(200) ; #DEBUG_LINE_NO:831
EndFunction

Function Fragment_301()
  Self.SetObjectiveDisplayed(75, 1 as Bool, False) ; #DEBUG_LINE_NO:840
  Game.GetPlayer().AddItem(Lockpick as Form, 12, False) ; #DEBUG_LINE_NO:841
EndFunction

Function Fragment_245()
  Self.SetObjectiveCompleted(100, True) ; #DEBUG_LINE_NO:851
  Game.AddAchievement(1) ; #DEBUG_LINE_NO:853
  ExitMusic.Add() ; #DEBUG_LINE_NO:855
  Self.SetStage(500) ; #DEBUG_LINE_NO:858
  Self.SetStage(1000) ; #DEBUG_LINE_NO:860
EndFunction

Function Fragment_212()
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:871
  Alias_ImperialSoldierFort01.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:872
EndFunction

Function Fragment_295()
  If !SkyrimTogetherUtils.IsHelgenGuest()
    Quest __temp = Self as Quest ; #DEBUG_LINE_NO:880
    mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:881
    Game.SetPlayerAIDriven(False) ; #DEBUG_LINE_NO:885
    Game.GetPlayer().PlayIdle(IdleExecutionerAlduinReactionDeath) ; #DEBUG_LINE_NO:889
    kmyQuest.PlayerImodAnimation() ; #DEBUG_LINE_NO:893
    kmyQuest.PlayerFurnitureAnimation() ; #DEBUG_LINE_NO:895
  EndIf
EndFunction

Function Fragment_255()
  If Self.GetStageDone(257) == 0 as Bool ; #DEBUG_LINE_NO:905
    Self.SetObjectiveDisplayed(60, False, False) ; #DEBUG_LINE_NO:906
  EndIf
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:909
EndFunction

Function Fragment_235()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:917
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:918
  Alias_Cart2.GetRef().SetMotionType(Alias_Cart1.GetRef().Motion_Keyframed, True) ; #DEBUG_LINE_NO:922
  Sound.StopInstance(kmyQuest.CartSoundInstanceID) ; #DEBUG_LINE_NO:924
  kmyQuest.ExitCart(Alias_ImperialSoldier01, 5) ; #DEBUG_LINE_NO:926
EndFunction

Function Fragment_287()
  Game.DisablePlayerControls(True, True, False, True, False, True, True, False, 0) ; #DEBUG_LINE_NO:936
  If sharedGuestStarted
    Game.GetPlayer().ClearKeepOffsetFromActor()
    Game.SetPlayerAIDriven(False)
    Game.ForceFirstPerson()
    Game.EnablePlayerControls(False, False, False, True, False, False, False, False, 0)
  EndIf
  If !SkyrimTogetherUtils.IsHelgenGuest()
    Game.GetPlayer().EvaluatePackage() ; #DEBUG_LINE_NO:937
  EndIf
EndFunction

Function Fragment_156()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:945
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:946
  Game.SetInChargen(False, False, False) ; #DEBUG_LINE_NO:951
  Game.DisablePlayerControls(True, True, False, False, False, True, True, False, 0) ; #DEBUG_LINE_NO:954
  Game.GetPlayer().RemoveAllItems(None, False, False) ; #DEBUG_LINE_NO:956
  Game.GetPlayer().EquipItem(ClothesPrisoner as Form, False, False) ; #DEBUG_LINE_NO:958
  Game.GetPlayer().EquipItem(ClothesPrisonerShoes as Form, False, False) ; #DEBUG_LINE_NO:959
  Game.GetPlayer().moveto(HelgenEndMarker, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:960
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:961
  Game.ShowRaceMenu() ; #DEBUG_LINE_NO:963
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:964
  If TempChooseSidesMessage2.Show(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0) == 0 ; #DEBUG_LINE_NO:966
    Self.SetStage(6) ; #DEBUG_LINE_NO:968
  Else
    Self.SetStage(7) ; #DEBUG_LINE_NO:970
  EndIf
  Game.EnablePlayerControls(True, False, False, True, True, False, False, True, 0) ; #DEBUG_LINE_NO:972
EndFunction

Function Fragment_300()
  Self.SetObjectiveDisplayed(70, False, False) ; #DEBUG_LINE_NO:989
EndFunction

Function Fragment_65()
  Self.SetObjectiveDisplayed(70, False, False) ; #DEBUG_LINE_NO:998
EndFunction

Function Fragment_159()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1006
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1007
  Scene1.Stop() ; #DEBUG_LINE_NO:1011
  Scene2.Stop() ; #DEBUG_LINE_NO:1012
  Scene3.Stop() ; #DEBUG_LINE_NO:1013
  Scene4.Stop() ; #DEBUG_LINE_NO:1014
  kmyQuest.FactionPath = 2 ; #DEBUG_LINE_NO:1015
  Self.SetStage(200) ; #DEBUG_LINE_NO:1016
  KeepIntroSceneB.Start() ; #DEBUG_LINE_NO:1017
EndFunction

Function Fragment_56()
  Alias_CartHorse1.TryToDisable() ; #DEBUG_LINE_NO:1027
  Alias_CartHorse2.TryToDisable() ; #DEBUG_LINE_NO:1028
  Self.SetStage(0) ; #DEBUG_LINE_NO:1029
  Self.SetStage(80) ; #DEBUG_LINE_NO:1030
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1031
  Self.SetStage(100) ; #DEBUG_LINE_NO:1032
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1033
  Self.SetStage(120) ; #DEBUG_LINE_NO:1034
  Game.GetPlayer().moveto(PlayerMarker4, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1036
  Alias_Ulfric.TryToMoveTo(UlfricKeepMarker1) ; #DEBUG_LINE_NO:1037
  Alias_GeneralTullius.TryToMoveTo(TulliusKeepMarker1) ; #DEBUG_LINE_NO:1038
  Alias_Hadvar.TryToMoveTo(HadvarKeepMarker1) ; #DEBUG_LINE_NO:1039
  Alias_Ralof.TryToMoveTo(RalofKeepExteriorMarker) ; #DEBUG_LINE_NO:1040
  Alias_ImperialSoldier01.TryToMoveTo(Guard1KeepMarker1) ; #DEBUG_LINE_NO:1041
  Alias_ImperialSoldier02.TryToMoveTo(Guard2KeepMarker1) ; #DEBUG_LINE_NO:1042
  Alias_ImperialSoldierFort01.TryToMoveTo(TulliusKeepMarker1) ; #DEBUG_LINE_NO:1043
  Alias_StormcloakPrisoner01.TryToMoveTo(Son1KeepMarker1) ; #DEBUG_LINE_NO:1044
  Alias_StormcloakPrisoner02.TryToMoveTo(Son2KeepMarker1) ; #DEBUG_LINE_NO:1045
  Alias_StormcloakPrisoner03.TryToMoveTo(UlfricKeepMarker1) ; #DEBUG_LINE_NO:1046
  Alias_StormcloakPrisoner04.TryToMoveTo(RalofKeepMarker1) ; #DEBUG_LINE_NO:1047
EndFunction

Function Fragment_113()
  MQ101ImperialFaction.SetEnemy(PlayerFaction, False, False) ; #DEBUG_LINE_NO:1056
  Alias_BarracksRoomSoldier01.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:1059
  Alias_BarracksRoomSoldier02.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:1060
  Alias_KeepLever1.GetRef().BlockActivation(True) ; #DEBUG_LINE_NO:1063
  Alias_KeepLever2.GetRef().BlockActivation(True) ; #DEBUG_LINE_NO:1064
EndFunction

Function Fragment_67()
  Self.SetStage(270) ; #DEBUG_LINE_NO:1074
  Self.SetStage(252) ; #DEBUG_LINE_NO:1077
EndFunction

Function Fragment_27()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1093
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1094
  Game.SetInChargen(False, False, False) ; #DEBUG_LINE_NO:1099
  Game.GetPlayer().moveto(HelgenEndMarker, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1101
  Game.FadeOutGame(False, True, 1.0, 1.0) ; #DEBUG_LINE_NO:1103
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1104
  Game.ShowRaceMenu() ; #DEBUG_LINE_NO:1106
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1107
  kmyQuest.AddRaceSpells() ; #DEBUG_LINE_NO:1109
  If TempChooseSidesMessage.Show(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0) == 0 ; #DEBUG_LINE_NO:1111
    Alias_Hadvar.GetReference().moveto(HelgenFriendMarker, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1115
    MQ102A.SetStage(1) ; #DEBUG_LINE_NO:1117
  Else
    Alias_Ralof.GetReference().moveto(HelgenFriendMarker, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1122
    MQ102B.SetStage(1) ; #DEBUG_LINE_NO:1124
  EndIf
  Self.SetStage(25) ; #DEBUG_LINE_NO:1127
  Self.SetStage(26) ; #DEBUG_LINE_NO:1128
  Self.SetStage(1000) ; #DEBUG_LINE_NO:1129
  MUSDungeonChargen.Remove() ; #DEBUG_LINE_NO:1132
  MUSCombatBossChargen.Remove() ; #DEBUG_LINE_NO:1133
  ExtHelgenAttackASREF.Disable(False) ; #DEBUG_LINE_NO:1134
  Self.Stop() ; #DEBUG_LINE_NO:1135
EndFunction

Function Fragment_17()
  Self.SetObjectiveCompleted(30, True) ; #DEBUG_LINE_NO:1144
  Self.SetObjectiveCompleted(50, True) ; #DEBUG_LINE_NO:1145
  MQ101DragonAttack.SetStage(200) ; #DEBUG_LINE_NO:1146
  Self.SetStage(180) ; #DEBUG_LINE_NO:1147
  Self.SetStage(250) ; #DEBUG_LINE_NO:1148
  MUSCombatBossChargen.Remove() ; #DEBUG_LINE_NO:1151
  ExtHelgenAttackASREF.Disable(False) ; #DEBUG_LINE_NO:1152
  Alias_Alduin.GetActorRef().Disable(False) ; #DEBUG_LINE_NO:1155
EndFunction

Function Fragment_317()
  CharGenReadyWeapon.ShowAsHelpMessage("Ready Weapon", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:1164
EndFunction

Function Fragment_23()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1172
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1173
  kmyQuest.FactionPath = 2 ; #DEBUG_LINE_NO:1177
  Self.SetStage(200) ; #DEBUG_LINE_NO:1178
EndFunction

Function Fragment_294()
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1187
EndFunction

Function Fragment_15()
  Alias_Alduin.GetRef().Enable(False) ; #DEBUG_LINE_NO:1203
  Scene4.Stop() ; #DEBUG_LINE_NO:1205
  AlduinAttackScene.Start() ; #DEBUG_LINE_NO:1206
  ExplosionTriggerA01.Enable(False) ; #DEBUG_LINE_NO:1207
  BlockGateCollisionMarker.Enable(False) ; #DEBUG_LINE_NO:1209
  Alias_Ulfric.GetActorRef().PlayIdle(OffsetStop) ; #DEBUG_LINE_NO:1212
  Alias_Ulfric.GetActorRef().RemoveItem(PrisonerCuffs as Form, 1, False, None) ; #DEBUG_LINE_NO:1213
  Alias_Ralof.GetActorRef().SetOutfit(RalofOutfit, False) ; #DEBUG_LINE_NO:1214
  Alias_Ralof.GetActorRef().PlayIdle(OffsetStop) ; #DEBUG_LINE_NO:1215
  Alias_StormcloakPrisoner01.GetActorRef().RemoveItem(PrisonerCuffs as Form, 1, False, None) ; #DEBUG_LINE_NO:1216
  Alias_StormcloakPrisoner02.GetActorRef().RemoveItem(PrisonerCuffs as Form, 1, False, None) ; #DEBUG_LINE_NO:1217
  Alias_StormcloakPrisoner03.GetActorRef().RemoveItem(PrisonerCuffs as Form, 1, False, None) ; #DEBUG_LINE_NO:1218
  Alias_Ulfric.GetActorRef().UnEquipItem(ArmorGag as Form, False, False) ; #DEBUG_LINE_NO:1219
  Alias_Ulfric.GetActorRef().RemoveItem(ArmorGag as Form, 1, False, None) ; #DEBUG_LINE_NO:1220
EndFunction

Function Fragment_76()
  Alias_KeepLever1.GetRef().Activate(Alias_BarracksRoomSoldier02.GetRef(), False) ; #DEBUG_LINE_NO:1237
  (KeepGate1 as default2stateactivator).SetOpen(True) ; #DEBUG_LINE_NO:1239
EndFunction

Function Fragment_227()
  Game.GetPlayer().EquipItem(ClothesPrisoner as Form, False, True) ; #DEBUG_LINE_NO:1251
  Game.GetPlayer().EquipItem(ClothesPrisonerShoes as Form, False, True) ; #DEBUG_LINE_NO:1252
  Game.GetPlayer().EquipItem(PrisonerCuffsPlayer as Form, False, True) ; #DEBUG_LINE_NO:1253
  Game.EnablePlayerControls(True, False, False, True, False, False, False, False, 0) ; #DEBUG_LINE_NO:1255
  Game.SetPlayerAIDriven(True) ; #DEBUG_LINE_NO:1257
  SittingAngleLimit.SetValue(0 as Float) ; #DEBUG_LINE_NO:1259
  Scene4.Start() ; #DEBUG_LINE_NO:1262
  Scene1.Stop() ; #DEBUG_LINE_NO:1263
  Scene3.Stop() ; #DEBUG_LINE_NO:1264
  MQ101DragonAttack.SetStage(5) ; #DEBUG_LINE_NO:1267
EndFunction

Function Fragment_257()
  Alias_StoreroomStormcloak1.TryToEnable() ; #DEBUG_LINE_NO:1276
  Alias_StoreroomStormcloak2.TryToEnable() ; #DEBUG_LINE_NO:1277
EndFunction

Function Fragment_275()
  MQ101KeepEnemyScene1B.Start() ; #DEBUG_LINE_NO:1286
EndFunction

Function Fragment_316()
  StopSharedGuest()
  Game.GetPlayer().SetRestrained(False) ; #DEBUG_LINE_NO:1297
  Game.EnablePlayerControls(True, False, False, True, True, False, False, True, 0) ; #DEBUG_LINE_NO:1298
  Game.SetHudCartMode(False) ; #DEBUG_LINE_NO:1300
  Self.SetObjectiveDisplayed(30, True, False) ; #DEBUG_LINE_NO:1302
  If Game.UsingGamePad() ; #DEBUG_LINE_NO:1303
    CharGenMove.ShowAsHelpMessage("Move", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:1304
  Else
    CharGenMovePC.ShowAsHelpMessage("Forward", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:1306
  EndIf
EndFunction

Function Fragment_218()
  Scene1.Stop() ; #DEBUG_LINE_NO:1316
  Scene3.Start() ; #DEBUG_LINE_NO:1317
EndFunction

Function Fragment_105()
  Alias_PoolRoomStormcloak1.TryToEnable() ; #DEBUG_LINE_NO:1326
  Alias_PoolRoomStormcloak2.TryToEnable() ; #DEBUG_LINE_NO:1327
  Alias_PoolRoomStormcloak3.TryToEnable() ; #DEBUG_LINE_NO:1328
  Alias_PoolRoomStormcloak4.TryToEnable() ; #DEBUG_LINE_NO:1329
  Alias_PoolRoomStormcloak5.TryToEnable() ; #DEBUG_LINE_NO:1330
EndFunction

Function Fragment_327()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1338
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1339
  kmyQuest.RegisterForAnimationEvent(Alias_StormcloakPrisoner01.GetActorRef() as ObjectReference, "Decapitate") ; #DEBUG_LINE_NO:1343
EndFunction

Function Fragment_42()
  AlduinAttackScene.Stop() ; #DEBUG_LINE_NO:1354
  AlduinAttackWeather.ForceActive(True) ; #DEBUG_LINE_NO:1358
  ExtHelgenAttackASREF.Enable(False) ; #DEBUG_LINE_NO:1359
  Alias_CartHorse1.GetRef().DisableNoWait(False) ; #DEBUG_LINE_NO:1361
  Alias_Ulfric.TryToMoveTo(UlfricMarker8) ; #DEBUG_LINE_NO:1364
  Alias_GeneralTullius.TryToMoveTo(TulliusMarker8) ; #DEBUG_LINE_NO:1365
  Alias_Hadvar.TryToMoveTo(HadvarMarker8) ; #DEBUG_LINE_NO:1366
  Alias_Ralof.TryToMoveTo(RalofMarker8) ; #DEBUG_LINE_NO:1367
  Alias_ImperialSoldier01.TryToMoveTo(Guard1Marker8) ; #DEBUG_LINE_NO:1368
  Alias_ImperialSoldier02.TryToMoveTo(Guard2Marker8) ; #DEBUG_LINE_NO:1369
  Alias_ImperialSoldierFort01.TryToMoveTo(FortGuard1Marker8) ; #DEBUG_LINE_NO:1370
  Alias_StormcloakPrisoner04.TryToMoveTo(StormcloakPrisoner1Marker8) ; #DEBUG_LINE_NO:1371
  Alias_StormcloakPrisoner02.TryToMoveTo(StormcloakPrisoner2Marker8) ; #DEBUG_LINE_NO:1372
  Alias_StormcloakPrisoner03.TryToMoveTo(StormcloakPrisoner3Marker8) ; #DEBUG_LINE_NO:1373
  Alias_CivilianGunnar.TryToMoveTo(HadvarMarker8) ; #DEBUG_LINE_NO:1375
  Alias_CivilianTorri.TryToMoveTo(TorriMarker8) ; #DEBUG_LINE_NO:1376
  Alias_CivilianTorolf.TryToMoveTo(TorolfMarker8) ; #DEBUG_LINE_NO:1377
  Alias_CivilianMatlara.TryToMoveTo(TulliusMarker8) ; #DEBUG_LINE_NO:1378
  Alias_CivilianIngrid.TryToMoveTo(TulliusMarker8) ; #DEBUG_LINE_NO:1379
  Alias_CivilianVilod.TryToMoveTo(TulliusMarker8) ; #DEBUG_LINE_NO:1380
  Alias_Elenwen.GetActorRef().MoveToMyEditorLocation() ; #DEBUG_LINE_NO:1382
  Alias_Elenwen.GetActorRef().EnableNoWait(False) ; #DEBUG_LINE_NO:1383
  Alias_Priest.GetActorRef().DisableNoWait(False) ; #DEBUG_LINE_NO:1390
  Alias_ImperialSoldierFort01.GetActorRef().DisableNoWait(False) ; #DEBUG_LINE_NO:1391
  Alias_HelgenArcher01.GetActorRef().DisableNoWait(False) ; #DEBUG_LINE_NO:1392
  Alias_HelgenArcher02.GetActorRef().DisableNoWait(False) ; #DEBUG_LINE_NO:1393
  MQ101DragonAttack.SetStage(10) ; #DEBUG_LINE_NO:1398
EndFunction

Function Fragment_281()
  IntoTorriScene.Start() ; #DEBUG_LINE_NO:1431
EndFunction

Function Fragment_46()
  KeepIntroSceneA.Start() ; #DEBUG_LINE_NO:1440
  MQ101ImperialFaction.SetAlly(PlayerFaction, False, False) ; #DEBUG_LINE_NO:1442
  Alias_PlayerFriend.ForceRefTo(Alias_Hadvar.GetRef()) ; #DEBUG_LINE_NO:1444
  Alias_TrophyRoomPrisoner01.TryToEnable() ; #DEBUG_LINE_NO:1446
  Alias_TrophyRoomPrisoner02.TryToEnable() ; #DEBUG_LINE_NO:1447
  Alias_Ralof.TryToDisable() ; #DEBUG_LINE_NO:1450
EndFunction

Function Fragment_148()
  KeepScene4B.Start() ; #DEBUG_LINE_NO:1459
EndFunction

Function Fragment_268()
  DragonRoarDistant02.Play(MQ101DragonShoutsMarkerREF) ; #DEBUG_LINE_NO:1476
EndFunction

Function Fragment_44()
  Self.SetObjectiveDisplayed(100, True, False) ; #DEBUG_LINE_NO:1485
  AlduinAttackScene.Stop() ; #DEBUG_LINE_NO:1487
  Alias_KeepDoorInteriorImperials.GetRef().BlockActivation(True) ; #DEBUG_LINE_NO:1489
  Alias_KeepDoorInteriorStormcloaks.GetRef().BlockActivation(True) ; #DEBUG_LINE_NO:1490
  Weather.ReleaseOverride() ; #DEBUG_LINE_NO:1492
EndFunction

Function Fragment_121()
  MQ102B.SetStage(5) ; #DEBUG_LINE_NO:1501
  KeepIntroSceneB.Stop() ; #DEBUG_LINE_NO:1504
  KeepScene2B.Stop() ; #DEBUG_LINE_NO:1505
  KeepScene4B.Stop() ; #DEBUG_LINE_NO:1506
  KeepScene5B.Stop() ; #DEBUG_LINE_NO:1507
EndFunction

Function Fragment_85()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1515
  defaultquestrespawnscript kmyQuest = __temp as defaultquestrespawnscript ; #DEBUG_LINE_NO:1516
  Self.SetObjectiveDisplayed(75, False, False) ; #DEBUG_LINE_NO:1520
EndFunction

Function Fragment_177()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1536
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1537
  Alias_GeneralTullius.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1542
  Alias_CartHorse1.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1543
  Alias_CartHorse2.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1544
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1545
  Game.SetSittingRotation(-55 as Float) ; #DEBUG_LINE_NO:1549
EndFunction

Function Fragment_296()
  Alias_BarracksRoomSoldier01.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:1574
  Alias_BarracksRoomSoldier02.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:1575
EndFunction

Function Fragment_228()
  HelgenCrowdWallaMarker2.Disable(False) ; #DEBUG_LINE_NO:1586
EndFunction

Function Fragment_191()
  Game.SetInChargen(False, False, False) ; #DEBUG_LINE_NO:1595
  Self.SetStage(2) ; #DEBUG_LINE_NO:1600
  Self.SetStage(10) ; #DEBUG_LINE_NO:1601
  Game.GetPlayer().moveto(PlayerMarker5, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1603
  Alias_Ulfric.TryToMoveTo(UlfricMarker5) ; #DEBUG_LINE_NO:1604
  Alias_GeneralTullius.TryToMoveTo(TulliusMarker5) ; #DEBUG_LINE_NO:1605
  Alias_Hadvar.TryToMoveTo(HadvarMarker5) ; #DEBUG_LINE_NO:1606
  Alias_TulliusHorse.TryToMoveTo(TulliusMarker5) ; #DEBUG_LINE_NO:1607
  Alias_HadvarHorse.TryToMoveTo(HadvarMarker5) ; #DEBUG_LINE_NO:1608
  Alias_Ralof.TryToMoveTo(RalofMarker5) ; #DEBUG_LINE_NO:1609
  Alias_ImperialSoldier01.TryToMoveTo(Guard1Marker5) ; #DEBUG_LINE_NO:1610
  Alias_ImperialSoldier02.TryToMoveTo(Guard2Marker5) ; #DEBUG_LINE_NO:1611
  Alias_ImperialSoldierFort01.TryToMoveTo(FortGuard1Marker5) ; #DEBUG_LINE_NO:1612
  Alias_Prisoner01.TryToMoveTo(PrisonerMarker5) ; #DEBUG_LINE_NO:1613
  Alias_StormcloakPrisoner01.TryToMoveTo(Son1Marker5) ; #DEBUG_LINE_NO:1614
  Alias_StormcloakPrisoner02.TryToMoveTo(Son2Marker5) ; #DEBUG_LINE_NO:1615
  Alias_StormcloakPrisoner03.TryToMoveTo(Son3Marker5) ; #DEBUG_LINE_NO:1616
  Alias_StormcloakPrisoner04.TryToMoveTo(Son4Marker5) ; #DEBUG_LINE_NO:1617
  Alias_Elenwen.GetRef().moveto(Alias_ElenwenStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1618
  Alias_Cart1.TryToMoveTo(Cart1Marker5) ; #DEBUG_LINE_NO:1620
  Alias_Cart2.TryToMoveTo(Cart2Marker5) ; #DEBUG_LINE_NO:1621
  Alias_CartHorse1.TryToMoveTo(CartHorse1Marker5) ; #DEBUG_LINE_NO:1622
  Alias_CartHorse2.TryToMoveTo(CartHorse2Marker5) ; #DEBUG_LINE_NO:1623
  Game.DisablePlayerControls(False, True, True, False, False, False, True, True, 0) ; #DEBUG_LINE_NO:1626
  Game.GetPlayer().EquipItem(ClothesPrisoner as Form, False, False) ; #DEBUG_LINE_NO:1629
  Game.GetPlayer().EquipItem(ClothesPrisonerShoes as Form, False, False) ; #DEBUG_LINE_NO:1630
  Game.ShowFirstPersonGeometry(False) ; #DEBUG_LINE_NO:1633
EndFunction

Function Fragment_258()
  Alias_StoreroomImperial1.TryToEnable() ; #DEBUG_LINE_NO:1642
  Alias_StoreroomImperial2.TryToEnable() ; #DEBUG_LINE_NO:1643
EndFunction

Function Fragment_154()
  If Self.GetStageDone(800) == 0 as Bool ; #DEBUG_LINE_NO:1652
    KeepScene5B.Start() ; #DEBUG_LINE_NO:1653
  EndIf
  Alias_TortureRoomStormcloak1.TryToDisable() ; #DEBUG_LINE_NO:1657
  Alias_TortureRoomStormcloak2.TryToDisable() ; #DEBUG_LINE_NO:1658
  Alias_TortureRoomStormcloak3.TryToDisable() ; #DEBUG_LINE_NO:1659
EndFunction

Function Fragment_32()
  EastGate.SetOpen(False) ; #DEBUG_LINE_NO:1676
  EastGate.SetLockLevel(5) ; #DEBUG_LINE_NO:1677
EndFunction

Function Fragment_111()
  CiviliansOutsideHelgenMarker.Disable(False) ; #DEBUG_LINE_NO:1686
  Self.SetStage(27) ; #DEBUG_LINE_NO:1687
  Alias_CartHorse1.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1695
  Alias_CartHorse2.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1696
EndFunction

Function Fragment_82()
  Alias_Torturer.GetActorRef().GetActorBase().SetProtected(True) ; #DEBUG_LINE_NO:1723
  Self.SetStage(365) ; #DEBUG_LINE_NO:1724
EndFunction

Function Fragment_2()
  GameHour.SetValue(7 as Float) ; #DEBUG_LINE_NO:1734
  Self.SetStage(10) ; #DEBUG_LINE_NO:1735
EndFunction

Function Fragment_325()
  MQ102B.SetStage(3) ; #DEBUG_LINE_NO:1753
EndFunction

Function Fragment_216()
  Self.SetStage(70) ; #DEBUG_LINE_NO:1762
EndFunction

Function Fragment_193()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1770
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1771
  Game.PrecacheCharGen() ; #DEBUG_LINE_NO:1776
  Alias_Elenwen.TryToDisable() ; #DEBUG_LINE_NO:1779
  Alias_Justiciar01.TryToDisable() ; #DEBUG_LINE_NO:1780
  Alias_Justiciar02.TryToDisable() ; #DEBUG_LINE_NO:1781
  Alias_CivilianGunnar.TryToDisable() ; #DEBUG_LINE_NO:1782
  Alias_ImperialSoldier01.GetActorRef().PlayIdle(kmyQuest.IdleCartDriverIdle) ; #DEBUG_LINE_NO:1786
  Alias_Ralof.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerDIdle) ; #DEBUG_LINE_NO:1787
  Alias_Prisoner01.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerBIdle) ; #DEBUG_LINE_NO:1788
  Alias_Ulfric.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerAIdle) ; #DEBUG_LINE_NO:1789
  Game.GetPlayer().PlayIdle(kmyQuest.IdleCartPassengerCIdle) ; #DEBUG_LINE_NO:1790
  Alias_ImperialSoldier02.GetActorRef().PlayIdle(kmyQuest.IdleCartDriverIdle) ; #DEBUG_LINE_NO:1793
  Alias_StormcloakPrisoner02.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerBIdle) ; #DEBUG_LINE_NO:1794
  Alias_StormcloakPrisoner03.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerDIdle) ; #DEBUG_LINE_NO:1795
  Alias_StormcloakPrisoner04.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerAIdle) ; #DEBUG_LINE_NO:1796
  Alias_StormcloakPrisoner01.GetActorRef().PlayIdle(kmyQuest.IdleCartPassengerCIdle) ; #DEBUG_LINE_NO:1797
EndFunction

Function Fragment_117()
  Alias_PlayerFriend.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:1806
EndFunction

Function Fragment_11()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1814
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1815
  Game.SetInChargen(False, True, True) ; #DEBUG_LINE_NO:1820
  Game.RequestAutoSave() ; #DEBUG_LINE_NO:1823
  kmyQuest.AddRaceSpells() ; #DEBUG_LINE_NO:1826
  StartSharedGuest()
EndFunction

Function Fragment_74()
  Game.EnablePlayerControls(True, True, True, True, True, True, True, True, 0) ; #DEBUG_LINE_NO:1843
  Game.ShowFirstPersonGeometry(True) ; #DEBUG_LINE_NO:1845
  Game.GetPlayer().RemoveItem(PrisonerCuffsPlayer as Form, 99, True, None) ; #DEBUG_LINE_NO:1846
  Alias_Hadvar.GetActorRef().IgnoreFriendlyHits(True) ; #DEBUG_LINE_NO:1852
  Alias_Ralof.GetActorRef().IgnoreFriendlyHits(True) ; #DEBUG_LINE_NO:1853
  Alias_Hadvar.GetActorRef().SetNotShowOnStealthMeter(True) ; #DEBUG_LINE_NO:1854
  Alias_Ralof.GetActorRef().SetNotShowOnStealthMeter(True) ; #DEBUG_LINE_NO:1855
  Alias_Hadvar.GetActorRef().BlockActivation(True) ; #DEBUG_LINE_NO:1856
  Alias_Ralof.GetActorRef().BlockActivation(True) ; #DEBUG_LINE_NO:1857
EndFunction

Function Fragment_120()
  MQ102A.SetStage(5) ; #DEBUG_LINE_NO:1874
  KeepIntroSceneA.Stop() ; #DEBUG_LINE_NO:1877
  KeepScene2A.Stop() ; #DEBUG_LINE_NO:1878
  KeepScene4A.Stop() ; #DEBUG_LINE_NO:1879
  KeepScene5A.Stop() ; #DEBUG_LINE_NO:1880
EndFunction

Function Fragment_57()
  Self.SetStage(10) ; #DEBUG_LINE_NO:1907
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1908
  Alias_TrophyRoomPrisoner01.TryToEnable() ; #DEBUG_LINE_NO:1913
  Alias_TrophyRoomPrisoner02.TryToEnable() ; #DEBUG_LINE_NO:1914
  Alias_Hadvar.TryToMoveTo(HadvarKeepMarker1) ; #DEBUG_LINE_NO:1915
  Alias_Ralof.TryToMoveTo(RalofKeepMarker1) ; #DEBUG_LINE_NO:1916
  Scene1.Stop() ; #DEBUG_LINE_NO:1929
  Scene2.Stop() ; #DEBUG_LINE_NO:1930
  Scene3.Stop() ; #DEBUG_LINE_NO:1931
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:1932
  Self.SetStage(180) ; #DEBUG_LINE_NO:1933
  Self.SetStage(190) ; #DEBUG_LINE_NO:1934
  Self.SetStage(195) ; #DEBUG_LINE_NO:1935
  Game.GetPlayer().moveto(PlayerImperialKeepMarker1, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:1937
  Game.DisablePlayerControls(True, True, True, False, False, True, True, False, 0) ; #DEBUG_LINE_NO:1940
  Game.EnablePlayerControls(True, False, False, True, True, True, False, True, 0) ; #DEBUG_LINE_NO:1941
EndFunction

Function Fragment_101()
  NPCDragonFlightRoarObstructed.Play(BridgeDragonRoarMarker) ; #DEBUG_LINE_NO:1950
  CollapsingBridgeAnim.Enable(False) ; #DEBUG_LINE_NO:1951
  Game.ShakeCamera(CollapsingBridgeAnim, 0.5, 1.25) ; #DEBUG_LINE_NO:1953
  Game.ShakeController(0.5, 0.349999994, 1.0) ; #DEBUG_LINE_NO:1954
  Utility.wait(1.0) ; #DEBUG_LINE_NO:1955
  If !CollapsingBridgeAnim.PlayAnimation("PlayAnim02") ; #DEBUG_LINE_NO:1956
    
  EndIf
  Game.ShakeCamera(CollapsingBridgeAnim, 1.0, 2.0) ; #DEBUG_LINE_NO:1959
  Game.ShakeController(0.899999976, 0.899999976, 2 as Float) ; #DEBUG_LINE_NO:1960
  Utility.wait(0.5) ; #DEBUG_LINE_NO:1961
  BridgeOriginal.Disable(False) ; #DEBUG_LINE_NO:1962
  Utility.wait(1.0) ; #DEBUG_LINE_NO:1963
  Game.ShakeCamera(CollapsingBridgeAnim, 0.600000024, 2.0) ; #DEBUG_LINE_NO:1964
  Game.ShakeController(0.200000003, 0.200000003, 2.0) ; #DEBUG_LINE_NO:1965
  BridgeDebris.Enable(False) ; #DEBUG_LINE_NO:1966
EndFunction

Function Fragment_77()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1974
  defaultquestrespawnscript kmyQuest = __temp as defaultquestrespawnscript ; #DEBUG_LINE_NO:1975
EndFunction

Function Fragment_89()
  Self.SetStage(370) ; #DEBUG_LINE_NO:1988
EndFunction

Function Fragment_175()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:1996
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:1997
  Game.SetHudCartMode(True) ; #DEBUG_LINE_NO:2003
  Actor myImperialSoldier01 = Alias_ImperialSoldier01.GetActorRef() ; #DEBUG_LINE_NO:2006
  Actor myRalof = Alias_Ralof.GetActorRef() ; #DEBUG_LINE_NO:2007
  Actor myPrisoner01 = Alias_Prisoner01.GetActorRef() ; #DEBUG_LINE_NO:2008
  Actor myUlfric = Alias_Ulfric.GetActorRef() ; #DEBUG_LINE_NO:2009
  Actor myPlayer = Game.GetPlayer() ; #DEBUG_LINE_NO:2010
  ObjectReference myCart2 = Alias_Cart2.GetRef() ; #DEBUG_LINE_NO:2011
  Actor myImperialSoldier02 = Alias_ImperialSoldier02.GetActorRef() ; #DEBUG_LINE_NO:2013
  Actor myStormcloakPrisoner01 = Alias_StormcloakPrisoner01.GetActorRef() ; #DEBUG_LINE_NO:2014
  Actor myStormcloakPrisoner02 = Alias_StormcloakPrisoner02.GetActorRef() ; #DEBUG_LINE_NO:2015
  Actor myStormcloakPrisoner03 = Alias_StormcloakPrisoner03.GetActorRef() ; #DEBUG_LINE_NO:2016
  Actor myStormcloakPrisoner04 = Alias_StormcloakPrisoner04.GetActorRef() ; #DEBUG_LINE_NO:2017
  ObjectReference myCart1 = Alias_Cart1.GetRef() ; #DEBUG_LINE_NO:2018
  While !myPlayer.Is3DLoaded() || !myImperialSoldier01.Is3DLoaded() || !myRalof.Is3DLoaded() || !myPrisoner01.Is3DLoaded() || !myUlfric.Is3DLoaded() || !myImperialSoldier02.Is3DLoaded() || !myStormcloakPrisoner02.Is3DLoaded() || !myStormcloakPrisoner03.Is3DLoaded() || !myStormcloakPrisoner04.Is3DLoaded() ; #DEBUG_LINE_NO:2021
    Utility.wait(0.200000003) ; #DEBUG_LINE_NO:2022
  EndWhile
  myCart1.TetherToHorse(Alias_CartHorse1.GetActorRef() as ObjectReference) ; #DEBUG_LINE_NO:2027
  myCart2.TetherToHorse(Alias_CartHorse2.GetActorRef() as ObjectReference) ; #DEBUG_LINE_NO:2028
  Utility.wait(1 as Float) ; #DEBUG_LINE_NO:2029
  myImperialSoldier01.SetVehicle(myCart2) ; #DEBUG_LINE_NO:2032
  myRalof.SetVehicle(myCart2) ; #DEBUG_LINE_NO:2033
  myPrisoner01.SetVehicle(myCart2) ; #DEBUG_LINE_NO:2034
  myUlfric.SetVehicle(myCart2) ; #DEBUG_LINE_NO:2035
  myPlayer.SetVehicle(myCart2) ; #DEBUG_LINE_NO:2036
  myImperialSoldier02.SetVehicle(myCart1) ; #DEBUG_LINE_NO:2039
  myStormcloakPrisoner02.SetVehicle(myCart1) ; #DEBUG_LINE_NO:2040
  myStormcloakPrisoner03.SetVehicle(myCart1) ; #DEBUG_LINE_NO:2041
  myStormcloakPrisoner04.SetVehicle(myCart1) ; #DEBUG_LINE_NO:2042
  myStormcloakPrisoner01.SetVehicle(myCart1) ; #DEBUG_LINE_NO:2043
  myImperialSoldier01.PlayIdle(IdleCartDriverSway) ; #DEBUG_LINE_NO:2050
  myRalof.PlayIdle(IdleCartPrisonerDSway) ; #DEBUG_LINE_NO:2051
  myPrisoner01.PlayIdle(IdleCartPrisonerBSway) ; #DEBUG_LINE_NO:2052
  myUlfric.PlayIdle(IdleCartPrisonerASway) ; #DEBUG_LINE_NO:2053
  myPlayer.PlayIdle(IdleCartPrisonerCIdle) ; #DEBUG_LINE_NO:2054
  myImperialSoldier02.PlayIdle(IdleCartDriverSway) ; #DEBUG_LINE_NO:2058
  myStormcloakPrisoner02.PlayIdle(IdleCartPrisonerBSway) ; #DEBUG_LINE_NO:2059
  myStormcloakPrisoner03.PlayIdle(IdleCartPrisonerDSway) ; #DEBUG_LINE_NO:2060
  myStormcloakPrisoner04.PlayIdle(IdleCartPrisonerASway) ; #DEBUG_LINE_NO:2061
  myStormcloakPrisoner01.PlayIdle(IdleCartPrisonerCSway) ; #DEBUG_LINE_NO:2062
  myUlfric.EquipItem(ArmorGag as Form, False, False) ; #DEBUG_LINE_NO:2064
EndFunction

Function Fragment_238()
  Self.SetStage(26) ; #DEBUG_LINE_NO:2072
EndFunction

Function Fragment_264()
  Self.SetObjectiveCompleted(60, 1 as Bool) ; #DEBUG_LINE_NO:2081
EndFunction

Function Fragment_247()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2089
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2090
  Alias_Torturer.GetActorRef().ResetHealthAndLimbs() ; #DEBUG_LINE_NO:2094
EndFunction

Function Fragment_207()
  HelgenCrowdWallaMarker1.Enable(False) ; #DEBUG_LINE_NO:2105
EndFunction

Function Fragment_309()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2123
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2124
  kmyQuest.RegisterForAnimationEvent(Alias_Alduin.GetActorRef() as ObjectReference, "FlightLanding") ; #DEBUG_LINE_NO:2129
  kmyQuest.RegisterForAnimationEvent(Alias_Alduin.GetActorRef() as ObjectReference, "TowerLandImpact") ; #DEBUG_LINE_NO:2130
  MUSDungeonChargen.Remove() ; #DEBUG_LINE_NO:2133
  MUSCombatBossChargen.Add() ; #DEBUG_LINE_NO:2134
EndFunction

Function Fragment_292()
  Alias_CartHorse1.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2145
  Alias_CartHorse2.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2146
EndFunction

Function Fragment_312()
  CharGenSneak.ShowAsHelpMessage("Sneak", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2155
EndFunction

Function Fragment_303()
  Game.GetPlayer().AddItem(Longbow as Form, 1, False) ; #DEBUG_LINE_NO:2185
  Game.GetPlayer().AddItem(IronArrow as Form, 12, False) ; #DEBUG_LINE_NO:2186
EndFunction

Function Fragment_209()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2194
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2195
  Game.DisablePlayerControls(True, True, False, True, False, True, True, False, 0) ; #DEBUG_LINE_NO:2201
  kmyQuest.ExitCart(Alias_Ulfric, 1) ; #DEBUG_LINE_NO:2202
  kmyQuest.ExitCart(Alias_Ralof, 3) ; #DEBUG_LINE_NO:2203
  kmyQuest.ExitCart(Alias_Player, 4) ; #DEBUG_LINE_NO:2204
  kmyQuest.ExitCart(Alias_Prisoner01, 2) ; #DEBUG_LINE_NO:2205
EndFunction

Function Fragment_262()
  Self.SetObjectiveDisplayed(65, 1 as Bool, False) ; #DEBUG_LINE_NO:2215
  CharGenActivateBody.ShowAsHelpMessage("Activate", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2216
EndFunction

Function Fragment_214()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2224
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2225
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2229
EndFunction

Function Fragment_276()
  MQ101KeepEnemyScene2A.Start() ; #DEBUG_LINE_NO:2247
EndFunction

Function Fragment_181()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2255
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2256
EndFunction

Function Fragment_138()
  Alias_HallwayStormcloak01.TryToEnable() ; #DEBUG_LINE_NO:2286
  Alias_HallwayStormcloak02.TryToEnable() ; #DEBUG_LINE_NO:2287
  Alias_HallwayStormcloak03.TryToEnable() ; #DEBUG_LINE_NO:2288
  Alias_HallwayStormcloak01.GetActorRef().SetActorValue("Aggression", 0 as Float) ; #DEBUG_LINE_NO:2290
  Alias_HallwayStormcloak02.GetActorRef().SetActorValue("Aggression", 0 as Float) ; #DEBUG_LINE_NO:2291
  Alias_HallwayStormcloak03.GetActorRef().SetActorValue("Aggression", 0 as Float) ; #DEBUG_LINE_NO:2292
EndFunction

Function Fragment_109()
  EastGate.SetOpen(True) ; #DEBUG_LINE_NO:2301
  IntroMusic.Remove() ; #DEBUG_LINE_NO:2306
  MUSDungeonChargen.Add() ; #DEBUG_LINE_NO:2308
  Alias_CartHorse1.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2310
  Alias_CartHorse2.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2311
  Alias_Hadvar.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2312
  CartPathAmbientMarker.Disable(False) ; #DEBUG_LINE_NO:2314
EndFunction

Function Fragment_261()
  Self.SetObjectiveDisplayed(60, 1 as Bool, False) ; #DEBUG_LINE_NO:2331
  CharGenActivateOpen.ShowAsHelpMessage("Activate", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2332
EndFunction

Function Fragment_260()
  Game.SetInChargen(False, False, False) ; #DEBUG_LINE_NO:2351
EndFunction

Function Fragment_302()
  Self.SetObjectiveDisplayed(75, False, False) ; #DEBUG_LINE_NO:2360
EndFunction

Function Fragment_157()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2368
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2369
  Scene1.Stop() ; #DEBUG_LINE_NO:2373
  Scene2.Stop() ; #DEBUG_LINE_NO:2374
  Scene3.Stop() ; #DEBUG_LINE_NO:2375
  Scene4.Stop() ; #DEBUG_LINE_NO:2376
  kmyQuest.FactionPath = 1 ; #DEBUG_LINE_NO:2377
  Self.SetStage(200) ; #DEBUG_LINE_NO:2378
  KeepIntroSceneA.Start() ; #DEBUG_LINE_NO:2379
EndFunction

Function Fragment_29()
  NorthGate.SetOpen(False) ; #DEBUG_LINE_NO:2390
  NorthGate.SetLockLevel(5) ; #DEBUG_LINE_NO:2391
  IntoTorriScene.Start() ; #DEBUG_LINE_NO:2392
EndFunction

Function Fragment_288()
  Alias_ImperialSoldierFort01.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2409
  If SkyrimTogetherUtils.IsHelgenGuest()
    ; The remote host never activates the local chopping-block furniture script.
    ; Replay only its headsman cue; the host's own animation arrives over the network.
    ; PlayIdle is suppressed for remote actors; use the verified idle's graph event.
    Debug.SendAnimationEvent(Alias_Headsman.GetActorRef(), "IdleExecutionerChop_Player")
    Quest guestQuest = Self as Quest
    mq101questscript guestScript = guestQuest as mq101questscript
    guestScript.RegisterForAnimationEvent(Alias_Alduin.GetActorRef(), "FlightLanding")
    guestScript.RegisterForAnimationEvent(Alias_Alduin.GetActorRef(), "TowerLandImpact")
  EndIf
EndFunction

Function Fragment_311()
  Game.DisablePlayerControls(True, True, True, True, False, True, True, False, 0) ; #DEBUG_LINE_NO:2418
EndFunction

Function Fragment_107()
  Alias_PoolRoomImperial1.TryToEnable() ; #DEBUG_LINE_NO:2427
  Alias_PoolRoomImperial2.TryToEnable() ; #DEBUG_LINE_NO:2428
  Alias_PoolRoomImperial3.TryToEnable() ; #DEBUG_LINE_NO:2429
  Alias_PoolRoomImperial4.TryToEnable() ; #DEBUG_LINE_NO:2430
  Alias_PoolRoomImperial5.TryToEnable() ; #DEBUG_LINE_NO:2431
EndFunction

Function Fragment_269()
  Alias_TortureRoomImperialSoldier1.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2441
  Alias_TortureRoomImperialSoldier2.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2442
  Alias_TortureRoomImperialSoldier3.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2443
  Alias_Torturer.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2444
  Alias_TortureRoomStormcloak1.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2445
  Alias_TortureRoomStormcloak2.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2446
  Alias_TortureRoomStormcloak3.GetActorRef().SetActorValue("Aggression", 1 as Float) ; #DEBUG_LINE_NO:2447
  Alias_TortureRoomStormcloak1.GetActorRef().StartCombat(Alias_Torturer.GetActorRef()) ; #DEBUG_LINE_NO:2450
  Alias_TortureRoomStormcloak2.GetActorRef().StartCombat(Alias_TortureRoomImperialSoldier1.GetActorRef()) ; #DEBUG_LINE_NO:2451
  Alias_Torturer.GetActorRef().StartCombat(Alias_TortureRoomStormcloak1.GetActorRef()) ; #DEBUG_LINE_NO:2452
  Alias_TortureRoomImperialSoldier1.GetActorRef().StartCombat(Alias_TortureRoomStormcloak2.GetActorRef()) ; #DEBUG_LINE_NO:2453
EndFunction

Function Fragment_199()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2469
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2470
  Alias_Cart1.GetRef().SetMotionType(Alias_Cart1.GetRef().Motion_Keyframed, True) ; #DEBUG_LINE_NO:2474
  kmyQuest.ExitCart(Alias_ImperialSoldier02, 5) ; #DEBUG_LINE_NO:2476
EndFunction

Function Fragment_19()
  Alias_Hadvar.GetActorRef().IgnoreFriendlyHits(False) ; #DEBUG_LINE_NO:2485
  Alias_Ralof.GetActorRef().IgnoreFriendlyHits(False) ; #DEBUG_LINE_NO:2486
  Alias_Hadvar.GetActorRef().SetNotShowOnStealthMeter(False) ; #DEBUG_LINE_NO:2487
  Alias_Ralof.GetActorRef().SetNotShowOnStealthMeter(False) ; #DEBUG_LINE_NO:2488
  Alias_Hadvar.GetActorRef().BlockActivation(False) ; #DEBUG_LINE_NO:2489
  Alias_Ralof.GetActorRef().BlockActivation(False) ; #DEBUG_LINE_NO:2490
  HelgenReserved.Clear() ; #DEBUG_LINE_NO:2494
  Alias_Ulfric.GetActorRef().MoveToMyEditorLocation() ; #DEBUG_LINE_NO:2496
  Alias_Ulfric.TryToEnable() ; #DEBUG_LINE_NO:2497
  Alias_GeneralTullius.GetActorRef().MoveToMyEditorLocation() ; #DEBUG_LINE_NO:2498
  Alias_GeneralTullius.TryToEnable() ; #DEBUG_LINE_NO:2499
  Alias_KeepDoorInteriorImperials.GetRef().BlockActivation(False) ; #DEBUG_LINE_NO:2502
  Alias_KeepDoorInteriorStormcloaks.GetRef().BlockActivation(False) ; #DEBUG_LINE_NO:2503
  FortNeugradEnableMarker.Enable(False) ; #DEBUG_LINE_NO:2506
  NorthGate.Lock(False, False) ; #DEBUG_LINE_NO:2507
  EastGate.Lock(False, False) ; #DEBUG_LINE_NO:2508
  PostMQ101KeepMarker.EnableNoWait(False) ; #DEBUG_LINE_NO:2509
  MQ101OutsideClutterMarker.EnableNoWait(False) ; #DEBUG_LINE_NO:2510
  Alias_Ulfric.GetActorRef().SetOutfit(UlfricOutfit, False) ; #DEBUG_LINE_NO:2513
  Self.Stop() ; #DEBUG_LINE_NO:2514
EndFunction

Function Fragment_204()
  Self.SetStage(12) ; #DEBUG_LINE_NO:2523
  Self.SetStage(35) ; #DEBUG_LINE_NO:2525
  SkyrimClear.ForceActive(True) ; #DEBUG_LINE_NO:2527
  Game.DisablePlayerControls(True, True, True, False, True, True, True, False, 0) ; #DEBUG_LINE_NO:2529
EndFunction

Function Fragment_278()
  Self.SetStage(500) ; #DEBUG_LINE_NO:2540
EndFunction

Function Fragment_241()
  Alias_CivilianTorri.TryToDisable() ; #DEBUG_LINE_NO:2550
  Alias_CivilianTorolf.TryToDisable() ; #DEBUG_LINE_NO:2551
EndFunction

Function Fragment_4()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2559
  mq101questscript kmyQuest = __temp as mq101questscript ; #DEBUG_LINE_NO:2560
  Game.ShowFirstPersonGeometry(False) ; #DEBUG_LINE_NO:2565
  Game.SetInChargen(True, True, True) ; #DEBUG_LINE_NO:2567
  Game.DisablePlayerControls(True, True, True, False, True, True, True, True, 0) ; #DEBUG_LINE_NO:2569
  SittingAngleLimit.SetValue(2.618000031) ; #DEBUG_LINE_NO:2571
  Game.GetPlayer().RemoveAllItems(None, False, False) ; #DEBUG_LINE_NO:2573
  Game.GetPlayer().EquipItem(ClothesPrisoner as Form, False, True) ; #DEBUG_LINE_NO:2575
  Game.GetPlayer().EquipItem(ClothesPrisonerShoes as Form, False, True) ; #DEBUG_LINE_NO:2576
  Game.GetPlayer().EquipItem(PrisonerCuffsPlayer as Form, False, True) ; #DEBUG_LINE_NO:2577
  Alias_Elenwen.GetRef().moveto(Alias_ElenwenStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2579
  Alias_GeneralTullius.GetRef().moveto(Alias_TulliusStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2580
  Alias_Hadvar.GetRef().moveto(Alias_HadvarStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2581
  Alias_Ralof.GetRef().moveto(Alias_RalofStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2582
  Alias_ImperialSoldier01.GetRef().moveto(Alias_SoldierMarker1.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2583
  Alias_ImperialSoldier02.GetRef().moveto(Alias_SoldierMarker1.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2584
  Alias_Ulfric.GetRef().moveto(Alias_StormcloakPrisoner1StartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2585
  Alias_StormcloakPrisoner01.GetRef().moveto(Alias_UlfricStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2586
  Alias_StormcloakPrisoner02.GetRef().moveto(Alias_StormcloakPrisoner1StartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2587
  Alias_StormcloakPrisoner03.GetRef().moveto(Alias_StormcloakPrisoner1StartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2588
  Alias_StormcloakPrisoner04.GetRef().moveto(Alias_StormcloakPrisoner1StartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2589
  Alias_Prisoner01.GetRef().moveto(Alias_PrisonerMarker1.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2590
  Alias_CartHorse1.GetRef().Enable(False) ; #DEBUG_LINE_NO:2593
  Game.GetPlayer().moveto(Alias_PlayerStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2595
  SkyrimClear.ForceActive(True) ; #DEBUG_LINE_NO:2597
  Utility.wait(0.100000001) ; #DEBUG_LINE_NO:2602
  Game.FadeOutGame(False, True, 14.0, 15.0) ; #DEBUG_LINE_NO:2605
  Game.ShowTitleSequenceMenu() ; #DEBUG_LINE_NO:2609
  kmyQuest.RegisterForSingleUpdate(4 as Float) ; #DEBUG_LINE_NO:2610
  IntroMusic.Add() ; #DEBUG_LINE_NO:2614
  kmyQuest.CartSoundInstanceID = CartSound.Play(Game.GetPlayer() as ObjectReference) ; #DEBUG_LINE_NO:2616
  NorthGate.SetOpen(True) ; #DEBUG_LINE_NO:2619
EndFunction

Function Fragment_112()
  ; Hadvar has opened MQ101KeepGate2Ref before this combat stage; delayed guests may miss its lever event.
  default2stateactivator gate = Game.GetForm(734639) as default2stateactivator ; Skyrim.esm 000B35AF, doOnce
  gate.SetOpen(True)
  gate.GotoState("done")
  MQ101StormcloakFaction.SetEnemy(PlayerFaction, False, False) ; #DEBUG_LINE_NO:2629
  Alias_TrophyRoomPrisoner01.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:2631
  Alias_TrophyRoomPrisoner02.GetActorRef().StartCombat(Game.GetPlayer()) ; #DEBUG_LINE_NO:2632
EndFunction

Function Fragment_308()
  Game.GetPlayer().SetRestrained(True) ; #DEBUG_LINE_NO:2649
EndFunction

Function Fragment_274()
  MQ101KeepEnemyScene1A.Start() ; #DEBUG_LINE_NO:2658
EndFunction

Function Fragment_315()
  CharGenLook.ShowAsHelpMessage("Look", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2667
EndFunction

Function Fragment_263()
  CharGenOpenInventory.ShowAsHelpMessage("Tween Menu", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2702
EndFunction

Function Fragment_318()
  Game.SetInChargen(False, True, True) ; #DEBUG_LINE_NO:2711
  Game.RequestSave() ; #DEBUG_LINE_NO:2714
EndFunction

Function Fragment_97()
  KeepScene4A.Start() ; #DEBUG_LINE_NO:2723
EndFunction

Function Fragment_306()
  Alias_Ulfric.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2733
  Alias_Ralof.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2734
  Alias_Ulfric.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2735
  Alias_StormcloakPrisoner01.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2737
  Alias_StormcloakPrisoner02.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2738
  Alias_StormcloakPrisoner03.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2739
  Alias_StormcloakPrisoner04.GetActorRef().SetHeadTracking(True) ; #DEBUG_LINE_NO:2740
EndFunction

Function Fragment_265()
  Self.SetObjectiveCompleted(65, 1 as Bool) ; #DEBUG_LINE_NO:2749
EndFunction

Function Fragment_152()
  If Self.GetStageDone(800) == 0 as Bool ; #DEBUG_LINE_NO:2758
    KeepScene5A.Start() ; #DEBUG_LINE_NO:2759
  EndIf
  Alias_Torturer.TryToDisable() ; #DEBUG_LINE_NO:2763
  Alias_TortureRoomImperialSoldier1.TryToDisable() ; #DEBUG_LINE_NO:2764
  Alias_TortureRoomImperialSoldier2.TryToDisable() ; #DEBUG_LINE_NO:2765
  Alias_TortureRoomImperialSoldier3.TryToDisable() ; #DEBUG_LINE_NO:2766
EndFunction

Function Fragment_331()
  CharGenPowerAttack.ShowAsHelpMessage("Right Attack/Block", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2775
EndFunction

Function Fragment_243()
  Alias_Ulfric.TryToDisable() ; #DEBUG_LINE_NO:2786
  Alias_StormcloakPrisoner01.TryToDisable() ; #DEBUG_LINE_NO:2787
  Alias_StormcloakPrisoner02.TryToDisable() ; #DEBUG_LINE_NO:2788
  Alias_StormcloakPrisoner03.TryToDisable() ; #DEBUG_LINE_NO:2789
  Alias_StormcloakPrisoner04.TryToDisable() ; #DEBUG_LINE_NO:2790
  Alias_GeneralTullius.TryToDisable() ; #DEBUG_LINE_NO:2792
  Alias_ImperialSoldierFort01.TryToDisable() ; #DEBUG_LINE_NO:2793
  Alias_ImperialSoldier01.TryToDisable() ; #DEBUG_LINE_NO:2794
  Alias_ImperialSoldier02.TryToDisable() ; #DEBUG_LINE_NO:2795
  Alias_Prisoner01.TryToDisable() ; #DEBUG_LINE_NO:2797
EndFunction

Function Fragment_63()
  KeepScene2B.Start() ; #DEBUG_LINE_NO:2806
EndFunction

Function Fragment_324()
  MQ102A.SetStage(3) ; #DEBUG_LINE_NO:2825
EndFunction

Function Fragment_131()
  Alias_Bear.GetActorRef().EvaluatePackage() ; #DEBUG_LINE_NO:2834
EndFunction

Function Fragment_266()
  DragonRoarDistant02.Play(MQ101DragonShoutsMarkerREF) ; #DEBUG_LINE_NO:2843
EndFunction

Function Fragment_54()
  TemporaryFriendsFaction.SetEnemy(TemporaryFriendsFaction, True, True) ; #DEBUG_LINE_NO:2853
  CharGenAttackwithPower.ShowAsHelpMessage("Right Attack/Block", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2854
EndFunction

Function Fragment_277()
  MQ101KeepEnemyScene2B.Start() ; #DEBUG_LINE_NO:2863
EndFunction

Function Fragment_251()
  Self.SetStage(600) ; #DEBUG_LINE_NO:2882
EndFunction

Function Fragment_190()
  Self.SetStage(10) ; #DEBUG_LINE_NO:2892
  Game.GetPlayer().moveto(PlayerMarker4, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2894
  Alias_Ulfric.TryToMoveTo(UlfricMarker8) ; #DEBUG_LINE_NO:2895
  Alias_GeneralTullius.TryToMoveTo(TulliusMarker4) ; #DEBUG_LINE_NO:2896
  Alias_TulliusHorse.TryToMoveTo(TulliusMarker4) ; #DEBUG_LINE_NO:2897
  Alias_HadvarHorse.TryToMoveTo(HadvarHorseMarker4) ; #DEBUG_LINE_NO:2898
  Alias_Hadvar.TryToMoveTo(HadvarMarker4) ; #DEBUG_LINE_NO:2899
  Alias_Ralof.TryToMoveTo(RalofMarker4) ; #DEBUG_LINE_NO:2900
  Alias_ImperialSoldier01.TryToMoveTo(Guard1Marker4) ; #DEBUG_LINE_NO:2901
  Alias_ImperialSoldier02.TryToMoveTo(Guard2Marker4) ; #DEBUG_LINE_NO:2902
  Alias_Prisoner01.TryToMoveTo(PrisonerMarker4) ; #DEBUG_LINE_NO:2903
  Alias_StormcloakPrisoner01.TryToMoveTo(Son1Marker4) ; #DEBUG_LINE_NO:2904
  Alias_StormcloakPrisoner02.TryToMoveTo(StormcloakPrisoner2Marker8) ; #DEBUG_LINE_NO:2905
  Alias_StormcloakPrisoner03.TryToMoveTo(StormcloakPrisoner3Marker8) ; #DEBUG_LINE_NO:2906
  Alias_StormcloakPrisoner04.TryToMoveTo(StormcloakPrisoner1Marker8) ; #DEBUG_LINE_NO:2907
  Alias_Elenwen.GetRef().moveto(Alias_ElenwenStartMarker.GetRef(), 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2908
  Game.GetPlayer().moveto(PlayerMarker4, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:2910
  Alias_Cart1.TryToMoveTo(Cart1Marker4) ; #DEBUG_LINE_NO:2912
  Alias_Cart2.TryToMoveTo(Cart2Marker4) ; #DEBUG_LINE_NO:2913
  Alias_CartHorse1.TryToMoveTo(CartHorse1Marker4) ; #DEBUG_LINE_NO:2914
  Alias_CartHorse2.TryToMoveTo(CartHorse2Marker4) ; #DEBUG_LINE_NO:2915
  Utility.wait(2 as Float) ; #DEBUG_LINE_NO:2916
  Self.SetStage(3) ; #DEBUG_LINE_NO:2917
EndFunction

Function Fragment_13()
  Game.ShowRaceMenu() ; #DEBUG_LINE_NO:2934
EndFunction

Function Fragment_95()
  Quest __temp = Self as Quest ; #DEBUG_LINE_NO:2942
  defaultquestrespawnscript kmyQuest = __temp as defaultquestrespawnscript ; #DEBUG_LINE_NO:2943
EndFunction

Function Fragment_62()
  KeepScene2A.Start() ; #DEBUG_LINE_NO:2956
EndFunction

Function Fragment_328()
  AlduinAttackEnableMarker1.EnableNoWait(False) ; #DEBUG_LINE_NO:2965
EndFunction

Function Fragment_79()
  FortNeugradEnableMarker.Disable(False) ; #DEBUG_LINE_NO:2975
  TEMPEndGate.Disable(False) ; #DEBUG_LINE_NO:2978
EndFunction

Function Fragment_330()
  CharGenBlock02.ShowAsHelpMessage("Left Attack/Block", 5 as Float, 30 as Float, 3) ; #DEBUG_LINE_NO:2988
EndFunction

Function Fragment_299()
  Self.SetObjectiveDisplayed(70, 1 as Bool, False) ; #DEBUG_LINE_NO:2997
EndFunction

Function StartSharedGuest()
  Actor host = Alias_Player.GetActorRef()
  Debug.Trace("SharedHelgen: starting guest, host=" + host + ", id=" + SkyrimTogetherUtils.GetHelgenHostId(), 0)
  If !SkyrimTogetherUtils.IsHelgenGuest() || host == None || host == Game.GetPlayer()
    Return
  EndIf
  sharedGuestStarted = True
  Quest guestQuest = Self as Quest
  (guestQuest as mq101questscript).CameraBobStop()
  Game.ForceFirstPerson()
  Game.SetPlayerAIDriven(True)
  Game.GetPlayer().KeepOffsetFromActor(host, 0.0, -160.0, 0.0, 0.0, 0.0, 0.0, 300.0, 20.0)
  RegisterForSingleUpdate(0.5)
EndFunction

Function StopSharedGuest()
  If !sharedGuestStarted
    Return
  EndIf
  sharedGuestStarted = False
  Quest guestQuest = Self as Quest
  (guestQuest as mq101questscript).CameraBobStop()
  Game.GetPlayer().ClearKeepOffsetFromActor()
  Alias_Player.ForceRefTo(Game.GetPlayer())
  Game.SetPlayerAIDriven(False)
  Game.GetPlayer().EvaluatePackage()
  Game.ForceFirstPerson()
EndFunction

Event OnUpdate()
  If sharedGuestStarted
    If SkyrimTogetherUtils.IsHelgenGuest() && GetStage() < 160
      RegisterForSingleUpdate(0.5)
    Else
      StopSharedGuest()
    EndIf
  EndIf
EndEvent

