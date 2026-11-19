"""Compile the actual router-entry ownership policy against query-result stubs (VS shell)."""
from pathlib import Path
import subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'Code/client/Services/Generic/HostSession.cpp').read_text()
body=source[source.index('    auto mapping ='):source.index('    // All paths converge')]
code=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
const char *client="192.168.1.2",*internal="10578",*desc="SkyrimTogether-old";int result=0;
int UPNP_GetSpecificPortMappingEntry(const char*,const char*,const char*,const char*,void*,char*c,char*p,char*d,char*,char*){std::strcpy(c,client);std::strcpy(p,internal);std::strcpy(d,desc);return result;}
int main(){
struct { const char* controlURL=""; } urls;struct {struct {const char* servicetype="";}first;}data;
const char* lan="192.168.1.2";std::string port="10578",description="SkyrimTogether-current";
'''+body+r'''
bool own=false,reuse=false;mapping(own,&reuse);assert(!own&&reuse);
desc="manual-forward";mapping(own,&reuse);assert(!own&&!reuse);
desc="SkyrimTogether-old";client="192.168.1.3";mapping(own,&reuse);assert(!own&&!reuse);
client=lan;internal="10579";mapping(own,&reuse);assert(!own&&!reuse);
internal="10578";desc=description.c_str();mapping(own,&reuse);assert(own&&reuse);
result=714;mapping(own,&reuse);assert(!own&&!reuse);
}
'''
with tempfile.TemporaryDirectory(prefix='host-mapping-') as directory:
 p=Path(directory);(p/'check.cpp').write_text(code)
 subprocess.run(['cl','/nologo','/EHsc','/std:c++20','check.cpp','/Fe:check.exe'],cwd=p,check=True)
 subprocess.run([str(p/'check.exe')],cwd=p,check=True)
print('PASS actual mapping policy: stale/current STR lease, foreign description, other PC/port, absent mapping')
