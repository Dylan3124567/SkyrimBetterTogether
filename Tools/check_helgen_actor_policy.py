"""Compile the actual Helgen actor policy against small value-only stubs (VS shell)."""
from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'Code/client/Services/Generic/CharacterService.cpp').read_text()
body=source[source.index('static bool PreserveLocalHelgenActor'):source.index('CharacterService::CharacterService')]
code=r'''
#include <cstdint>
#include <cassert>
struct Actor { uint32_t formID; bool disabled{}; bool IsDisabled(){return disabled;} };
struct TESQuest {
 bool stopped{},keep{},finished{}; Actor *ralof{},*hadvar{};
 bool IsStopped(){return stopped;}
 bool IsStageReached(int n){return n==250?keep:finished;}
 Actor* GetAliasedRef(int n){return n==12?ralof:hadvar;}
} quest;
struct TESForm { static TESQuest* GetById(int){return &quest;} };
template<class T> T* Cast(T* p){return p;}
''' + body + r'''
int main(){
 Actor alduin{0x32DB7},ralof{0x2BF9E},hadvar{0x2BFA2},other{9,true};
 quest.ralof=&ralof;quest.hadvar=&hadvar;
 assert(!PreserveLocalHelgenActor(nullptr));
 assert(PreserveLocalHelgenActor(&alduin));
 assert(!PreserveLocalHelgenActor(&other));
 ralof.disabled=true;assert(!PreserveLocalHelgenActor(&ralof));
 quest.keep=true;assert(PreserveLocalHelgenActor(&ralof));
 assert(!PreserveLocalHelgenActor(&hadvar));
 assert(!PreserveLocalHelgenActor(&other));
 ralof.disabled=false;hadvar.disabled=true;
 assert(!PreserveLocalHelgenActor(&ralof));assert(PreserveLocalHelgenActor(&hadvar));
 quest.finished=true;assert(!PreserveLocalHelgenActor(&alduin));assert(!PreserveLocalHelgenActor(&hadvar));
 quest.finished=false;quest.stopped=true;assert(!PreserveLocalHelgenActor(&alduin));
}
'''
with tempfile.TemporaryDirectory(prefix='helgen-policy-') as directory:
 p=Path(directory);(p/'check.cpp').write_text(code)
 subprocess.run(['cl','/nologo','/EHsc','/std:c++20','check.cpp','/Fe:check.exe'],cwd=p,check=True)
 subprocess.run([str(p/'check.exe')],cwd=p,check=True)
print('PASS actual actor policy: local Alduin, both disabled companions, active companions, unrelated actors and completed/stopped quest')
