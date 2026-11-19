import { Component, EventEmitter, Output } from '@angular/core';
import { ClientService } from '../../services/client.service';

@Component({
  selector: 'app-host',
  templateUrl: './host.component.html',
  styleUrls: ['../connect/connect.component.scss'],
})
export class HostComponent {
  @Output() done = new EventEmitter<void>();
  port = 10578;
  internet = true;
  password = this.client.hostPassword || Array.from(crypto.getRandomValues(new Uint8Array(8)), byte => byte.toString(16).padStart(2, '0')).join('');
  status$ = this.client.hostStatus;
  connected$ = this.client.connectionStateChange;
  copyFailed = false;
  copied = false;

  constructor(public readonly client: ClientService) {}

  get valid(): boolean {
    return Number.isInteger(this.port) && this.port >= 1 && this.port <= 65535 && /^[\x20-\x7e]{1,64}$/.test(this.password);
  }

  start(): void {
    if (this.valid) this.client.startHosting(this.port, this.password, this.internet);
  }

  async copy(address: string): Promise<void> {
    // CEF clipboard access is not available on every installation; the text stays selectable.
    try {
      await navigator.clipboard.writeText(address);
      this.copied = true;
    } catch { this.copyFailed = true; }
  }
}
