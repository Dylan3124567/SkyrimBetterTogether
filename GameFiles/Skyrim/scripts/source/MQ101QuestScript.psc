ScriptName MQ101QuestScript Extends Quest conditional
{ MQ101 main quest script }

;-- Variables ---------------------------------------
Int TitleSequenceCount
Int stage280Count
Int stage340Count
Int stage380Count
Int stage440Count
Int stage45count
Int stage46count
Int stage470Count
Int stage52count
Int stage56count
Int stage60count
Int stage630Count
Int stage64count
Int startingCellLoadCount = 0
Int startingCellTotal = 2

;-- Properties --------------------------------------
ReferenceAlias Property Alduin Auto
Race Property ArgonianRace Auto
Race Property BretonRace Auto
ImageSpaceModifier Property CGDragonAttackBlurLong Auto
Int Property CartSoundInstanceID Auto
{ instance ID of the cart sound (so I can stop it) }
Spell Property ConjureFamiliar Auto
Race Property DarkElfRace Auto
Int Property FactionPath Auto conditional
{ Current faction side you're with (can change during MQ101)
1 = Imperial
2 = Stormcloak }
Spell Property Fury Auto
ReferenceAlias Property Headsman Auto
ObjectReference Property HelgenCrowdWallaMarker1 Auto
ObjectReference Property HelgenCrowdWallaMarker2 Auto
Race Property HighElfRace Auto
Idle Property IdleCartDriverExit Auto
Idle Property IdleCartDriverIdle Auto
{ cart driver idle }
Idle Property IdleCartPassengerAExit Auto
Idle Property IdleCartPassengerAIdle Auto
Idle Property IdleCartPassengerBExit Auto
Idle Property IdleCartPassengerBIdle Auto
Idle Property IdleCartPassengerCExit Auto
Idle Property IdleCartPassengerCIdle Auto
Idle Property IdleCartPassengerDExit Auto
Idle Property IdleCartPassengerDIdle Auto
Idle Property IdleWalkingCameraEnd Auto
Idle Property IdleWalkingCameraStart Auto
Race Property ImperialRace Auto
Message Property KeepDoorMessage Auto
{ message to prevent exit from keep }
Race Property KhajiitRace Auto
Idle Property NextClip Auto
Race Property NordRace Auto
Race Property OrcRace Auto
ImageSpaceModifier Property PlayerAlduinIMOD Auto
Race Property RedguardRace Auto
Location Property Riverwood Auto
Spell Property Sparks Auto
ReferenceAlias Property StormcloakPrisoner01 Auto
Race Property WoodElfRace Auto
Int Property stage280Target = 2 Auto
Int Property stage340Target = 2 Auto
Int Property stage380Target = 2 Auto
Int Property stage440Target = 2 Auto
Int Property stage45Total = 1 Auto
Int Property stage46Total = 1 Auto
Int Property stage470Target = 5 Auto
Int Property stage52Total = 2 Auto
Int Property stage56Total = 2 Auto
Int Property stage60Total = 2 Auto
Int Property stage630Target = 5 Auto
Int Property stage64Total = 2 Auto

;-- Functions ---------------------------------------

; Skipped compiler generated GetState

; Skipped compiler generated GotoState

Function AdvanceHeadsmanClip()
  If SkyrimTogetherUtils.IsHelgenGuest()
    ; The remote actor rejects PlayIdle; 00NextClip uses this graph event.
    Debug.SendAnimationEvent(Headsman.GetActorRef(), "ExecutionNextClip")
  Else
    Headsman.GetActorRef().PlayIdle(NextClip)
  EndIf
EndFunction

Event OnAnimationEvent(ObjectReference akSource, String asEventName)
  If (akSource == Alduin.GetActorRef() as ObjectReference) && asEventName == "FlightLanding" ; #DEBUG_LINE_NO:47
    AdvanceHeadsmanClip()
    Self.UnregisterForAnimationEvent(Alduin.GetActorRef() as ObjectReference, "FlightLanding") ; #DEBUG_LINE_NO:50
  EndIf
  If (akSource == Alduin.GetActorRef() as ObjectReference) && asEventName == "TowerLandImpact" ; #DEBUG_LINE_NO:53
    AdvanceHeadsmanClip()
    Self.UnregisterForAnimationEvent(Alduin.GetActorRef() as ObjectReference, "TowerLandImpact") ; #DEBUG_LINE_NO:56
  EndIf
  If (akSource == StormcloakPrisoner01.GetActorRef() as ObjectReference) && asEventName == "Decapitate" ; #DEBUG_LINE_NO:59
    HelgenCrowdWallaMarker1.Disable(False) ; #DEBUG_LINE_NO:61
    HelgenCrowdWallaMarker2.Enable(False) ; #DEBUG_LINE_NO:62
    Self.UnregisterForAnimationEvent(StormcloakPrisoner01.GetActorRef() as ObjectReference, "Decapitate") ; #DEBUG_LINE_NO:63
  EndIf
  If (akSource == Game.GetPlayer() as ObjectReference) && asEventName == "PlayImod" ; #DEBUG_LINE_NO:66
    PlayerAlduinIMOD.Apply(1.0) ; #DEBUG_LINE_NO:68
    CGDragonAttackBlurLong.Apply(1.0) ; #DEBUG_LINE_NO:69
    Self.SetStage(145) ; #DEBUG_LINE_NO:70
    Self.UnregisterForAnimationEvent(Game.GetPlayer() as ObjectReference, "PlayImod") ; #DEBUG_LINE_NO:71
  EndIf
  If (akSource == Game.GetPlayer() as ObjectReference) && asEventName == "IdleFurnitureExit" ; #DEBUG_LINE_NO:74
    Self.SetStage(160) ; #DEBUG_LINE_NO:76
    Self.UnregisterForAnimationEvent(Game.GetPlayer() as ObjectReference, "IdleFurnitureExit") ; #DEBUG_LINE_NO:77
  EndIf
EndEvent

Event OnUpdate()
  If TitleSequenceCount == 0 ; #DEBUG_LINE_NO:84
    Game.StartTitleSequence("Sequence1") ; #DEBUG_LINE_NO:86
    TitleSequenceCount = 1 ; #DEBUG_LINE_NO:87
    Self.RegisterForSingleUpdate(5 as Float) ; #DEBUG_LINE_NO:88
  ElseIf TitleSequenceCount == 1 ; #DEBUG_LINE_NO:89
    Game.StartTitleSequence("Sequence2") ; #DEBUG_LINE_NO:91
    TitleSequenceCount = 2 ; #DEBUG_LINE_NO:92
    Self.RegisterForSingleUpdate(12 as Float) ; #DEBUG_LINE_NO:93
  ElseIf TitleSequenceCount == 2 ; #DEBUG_LINE_NO:94
    Game.StartTitleSequence("Sequence3") ; #DEBUG_LINE_NO:96
    TitleSequenceCount = 3 ; #DEBUG_LINE_NO:97
    Self.RegisterForSingleUpdate(5 as Float) ; #DEBUG_LINE_NO:98
  ElseIf TitleSequenceCount == 3 ; #DEBUG_LINE_NO:99
    Game.StartTitleSequence("Sequence4") ; #DEBUG_LINE_NO:101
    TitleSequenceCount = 4 ; #DEBUG_LINE_NO:102
    Self.RegisterForSingleUpdate(10 as Float) ; #DEBUG_LINE_NO:103
  ElseIf TitleSequenceCount == 4 ; #DEBUG_LINE_NO:104
    Game.HideTitleSequenceMenu() ; #DEBUG_LINE_NO:105
  EndIf
EndEvent

Function CameraBobStart()
  ; The camera is local; late package callbacks must not relock it after the escape handoff.
  If SkyrimTogetherUtils.IsHelgenGuest() || GetStage() >= 160
    Return
  EndIf
  Game.DisablePlayerControls(True, True, False, True, False, True, True, False, 0) ; #DEBUG_LINE_NO:112
  Game.GetPlayer().PlayIdle(IdleWalkingCameraStart) ; #DEBUG_LINE_NO:113
EndFunction

Function CameraBobStop()
  Game.GetPlayer().PlayIdle(IdleWalkingCameraEnd) ; #DEBUG_LINE_NO:117
  Game.EnablePlayerControls(False, False, False, True, False, False, False, False, 0) ; #DEBUG_LINE_NO:118
EndFunction

Function PlayerImodAnimation()
  If Self.RegisterForAnimationEvent(Game.GetPlayer() as ObjectReference, "PlayImod") ; #DEBUG_LINE_NO:123
    
  EndIf
EndFunction

Function PlayerFurnitureAnimation()
  If Self.RegisterForAnimationEvent(Game.GetPlayer() as ObjectReference, "IdleFurnitureExit") ; #DEBUG_LINE_NO:132
    
  EndIf
EndFunction

Function SetStageOnDeadCount(Int stageToSet)
  If stageToSet == 280 ; #DEBUG_LINE_NO:143
    stage280Count += 1 ; #DEBUG_LINE_NO:144
    If stage280Count >= stage280Target ; #DEBUG_LINE_NO:145
      Self.SetStage(280) ; #DEBUG_LINE_NO:146
    EndIf
  ElseIf stageToSet == 340 ; #DEBUG_LINE_NO:148
    stage340Count += 1 ; #DEBUG_LINE_NO:149
    If stage340Count >= stage340Target ; #DEBUG_LINE_NO:150
      Self.SetStage(340) ; #DEBUG_LINE_NO:151
    EndIf
  ElseIf stageToSet == 380 ; #DEBUG_LINE_NO:153
    stage380Count += 1 ; #DEBUG_LINE_NO:154
    If stage380Count >= stage380Target ; #DEBUG_LINE_NO:155
      Self.SetStage(380) ; #DEBUG_LINE_NO:156
    EndIf
  ElseIf stageToSet == 440 ; #DEBUG_LINE_NO:158
    stage440Count += 1 ; #DEBUG_LINE_NO:159
    If stage440Count >= stage440Target ; #DEBUG_LINE_NO:160
      Self.SetStage(440) ; #DEBUG_LINE_NO:161
    EndIf
  ElseIf stageToSet == 470 ; #DEBUG_LINE_NO:163
    stage470Count += 1 ; #DEBUG_LINE_NO:164
    If stage470Count >= stage470Target ; #DEBUG_LINE_NO:165
      Self.SetStage(470) ; #DEBUG_LINE_NO:166
    EndIf
  ElseIf stageToSet == 630 ; #DEBUG_LINE_NO:168
    stage630Count += 1 ; #DEBUG_LINE_NO:169
    If stage630Count >= stage630Target ; #DEBUG_LINE_NO:170
      Self.SetStage(630) ; #DEBUG_LINE_NO:171
    EndIf
  EndIf
EndFunction

Function RegisterStartingCellLoad()
  startingCellLoadCount += 1 ; #DEBUG_LINE_NO:183
  If startingCellLoadCount >= startingCellTotal ; #DEBUG_LINE_NO:184
    Self.SetStage(12) ; #DEBUG_LINE_NO:185
  EndIf
EndFunction

Function ExitCart(ReferenceAlias cartActorAlias, Int seat)
  Actor cartActor = cartActorAlias.GetActorRef() ; #DEBUG_LINE_NO:193
  cartActorAlias.RegisterForAnimationEvent(cartActor as ObjectReference, "ExitCartEnd") ; #DEBUG_LINE_NO:196
  If seat == 1 ; #DEBUG_LINE_NO:198
    cartActor.PlayIdle(IdleCartPassengerAExit) ; #DEBUG_LINE_NO:199
  ElseIf seat == 2 ; #DEBUG_LINE_NO:200
    cartActor.PlayIdle(IdleCartPassengerBExit) ; #DEBUG_LINE_NO:201
  ElseIf seat == 3 ; #DEBUG_LINE_NO:202
    cartActor.PlayIdle(IdleCartPassengerDExit) ; #DEBUG_LINE_NO:203
  ElseIf seat == 4 ; #DEBUG_LINE_NO:204
    cartActor.PlayIdle(IdleCartPassengerCExit) ; #DEBUG_LINE_NO:205
  ElseIf seat == 5 ; #DEBUG_LINE_NO:206
    cartActor.PlayIdle(IdleCartDriverExit) ; #DEBUG_LINE_NO:207
  EndIf
EndFunction

Function IncrementExitCartCount(Int stageToSet)
  Bool doSetStage = False ; #DEBUG_LINE_NO:231
  If stageToSet == 45 ; #DEBUG_LINE_NO:232
    stage45count += 1 ; #DEBUG_LINE_NO:233
    If stage45count >= stage45Total ; #DEBUG_LINE_NO:234
      doSetStage = True ; #DEBUG_LINE_NO:235
    EndIf
  ElseIf stageToSet == 46 ; #DEBUG_LINE_NO:237
    stage46count += 1 ; #DEBUG_LINE_NO:238
    If stage46count >= stage46Total ; #DEBUG_LINE_NO:239
      doSetStage = True ; #DEBUG_LINE_NO:240
    EndIf
  ElseIf stageToSet == 52 ; #DEBUG_LINE_NO:242
    stage52count += 1 ; #DEBUG_LINE_NO:243
    If stage52count >= stage52Total ; #DEBUG_LINE_NO:244
      doSetStage = True ; #DEBUG_LINE_NO:245
    EndIf
  ElseIf stageToSet == 56 ; #DEBUG_LINE_NO:247
    stage56count += 1 ; #DEBUG_LINE_NO:248
    If stage56count >= stage56Total ; #DEBUG_LINE_NO:249
      doSetStage = True ; #DEBUG_LINE_NO:250
    EndIf
  ElseIf stageToSet == 60 ; #DEBUG_LINE_NO:252
    stage60count += 1 ; #DEBUG_LINE_NO:253
    If stage60count >= stage60Total ; #DEBUG_LINE_NO:254
      doSetStage = True ; #DEBUG_LINE_NO:255
    EndIf
  ElseIf stageToSet == 64 ; #DEBUG_LINE_NO:257
    stage64count += 1 ; #DEBUG_LINE_NO:258
    If stage64count >= stage64Total ; #DEBUG_LINE_NO:259
      doSetStage = True ; #DEBUG_LINE_NO:260
    EndIf
  EndIf
  If doSetStage ; #DEBUG_LINE_NO:263
    Self.SetStage(stageToSet) ; #DEBUG_LINE_NO:264
  EndIf
EndFunction

Function AddRaceSpells()
  Actor player = Game.GetPlayer() ; #DEBUG_LINE_NO:276
  Self.RemoveRaceSpells(player) ; #DEBUG_LINE_NO:277
  Race playerRace = player.GetActorBase().GetRace() ; #DEBUG_LINE_NO:280
  If playerRace == ArgonianRace ; #DEBUG_LINE_NO:281
    
  ElseIf playerRace == BretonRace ; #DEBUG_LINE_NO:283
    player.AddSpell(ConjureFamiliar, True) ; #DEBUG_LINE_NO:284
  ElseIf playerRace == DarkElfRace ; #DEBUG_LINE_NO:286
    player.AddSpell(Sparks, True) ; #DEBUG_LINE_NO:287
  ElseIf playerRace == HighElfRace ; #DEBUG_LINE_NO:289
    player.AddSpell(Fury, True) ; #DEBUG_LINE_NO:290
  ElseIf playerRace == ImperialRace ; #DEBUG_LINE_NO:292
    
  ElseIf playerRace == KhajiitRace ; #DEBUG_LINE_NO:294
    
  ElseIf playerRace == NordRace ; #DEBUG_LINE_NO:296
    
  ElseIf playerRace == OrcRace ; #DEBUG_LINE_NO:298
    
  ElseIf playerRace == RedguardRace ; #DEBUG_LINE_NO:300
    
  ElseIf playerRace == WoodElfRace ; #DEBUG_LINE_NO:302
    
  EndIf
EndFunction

Function RemoveRaceSpells(Actor pActor)
  pActor.RemoveSpell(ConjureFamiliar) ; #DEBUG_LINE_NO:310
  pActor.RemoveSpell(Sparks) ; #DEBUG_LINE_NO:311
  pActor.RemoveSpell(Fury) ; #DEBUG_LINE_NO:312
EndFunction
