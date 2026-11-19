Scriptname SkyrimTogetherUtils Native

bool Function IsRemotePlayer(Actor actor) global native

bool Function IsPlayer(Actor actor) global native

; Local form ID published by the shared intro. Zero outside the guest cinematic.
int Function GetHelgenHostId() global native

bool Function IsHelgenGuest() global
    return GetHelgenHostId() != 0
EndFunction
