// Runs the real service handlers with RxJS; Angular's decorator is irrelevant here.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const ts = require('typescript');
const { Subject, BehaviorSubject } = require('rxjs');
const source = fs.readFileSync('src/app/services/player-list.service.ts', 'utf8');
const exportsObject = {};
vm.runInNewContext(ts.transpileModule(source, {compilerOptions: {module: ts.ModuleKind.CommonJS, experimentalDecorators: true}}).outputText, {
  exports: exportsObject,
  require: name => name === '@angular/core' ? {Injectable: () => value => value} : name === '../models/player-list' ? {PlayerList: class {players = [];}} : require(name),
  console,
});
const service = Object.create(exportsObject.PlayerListService.prototype);
service.playerList = new BehaviorSubject({players: [{id: 1, name: 'one', hasBeenInvited: true}, {id: 2, name: 'Prisoner', hasBeenInvited: true}]});
let invitations = 0;
service.clientService = {memberKickedChange: new Subject(), playerConnectedChange: new Subject(), partyInviteReceivedChange: new Subject(), createPartyInvite: () => ++invitations, acceptPartyInvite: () => ++invitations};
service.popupNotificationService = {addPartyInvite: () => ++invitations};
service.onMemberKicked(); service.onPlayerConnected(); service.onPartyInviteReceived();
service.clientService.memberKickedChange.next(2);
assert.equal(service.getPlayerById(1).hasBeenInvited, true);
assert.equal(service.getPlayerById(2).hasBeenInvited, false);
service.clientService.memberKickedChange.next(999);
service.clientService.playerConnectedChange.next({id: 2, name: 'Orc'});
assert.equal(service.getListLength(), 2);
assert.equal(service.getPlayerById(2).name, 'Orc');
service.sendPartyInvite(999); service.acceptPartyInvite(999);
service.clientService.partyInviteReceivedChange.next(999);
assert.equal(invitations, 0);
service.clientService.partyInviteReceivedChange.next(1);
assert.equal(service.getPlayerById(1).hasInvitedLocalPlayer, true);
service.acceptPartyInvite(1);
assert.equal(invitations, 2);
assert.equal(service.getPlayerById(1).hasInvitedLocalPlayer, false);
console.log('PASS party kick identity, rename without duplication, and departed-player invitations');
