import { Overlay } from '@angular/cdk/overlay';
import { Component, ElementRef, OnInit, ViewChild } from '@angular/core';
import { TranslocoService } from '@ngneat/transloco';
import { takeUntil } from 'rxjs';
import { environment } from '../../../environments/environment';
import { fadeInOutActiveAnimation } from '../../animations/fade-in-out-active.animation';
import { View } from '../../models/view.enum';
import { ClientService } from '../../services/client.service';
import { DestroyService } from '../../services/destroy.service';
import {
  SettingService,
  fontSizeToPixels,
} from '../../services/setting.service';
import { Sound, SoundService } from '../../services/sound.service';
import { UiRepository } from '../../store/ui.repository';
import { ChatComponent } from '../chat/chat.component';
import { GroupComponent } from '../group/group.component';
import { controlsAnimation } from './controls.animation';
import { notificationsAnimation } from './notifications.animation';
import { map } from 'rxjs/operators';

const REVEAL_EFFECT_DURATION_MS = 10000 // todo: pass value from C++?

@Component({
  selector: 'app-root',
  templateUrl: './root.component.html',
  styleUrls: ['./root.component.scss'],
  animations: [
    controlsAnimation,
    fadeInOutActiveAnimation,
    notificationsAnimation,
  ],
  host: { 'data-app-root-game': environment.game.toString() },
  providers: [DestroyService],
})
export class RootComponent implements OnInit {
  /* ### ENUMS ### */
  readonly RootView = View;

  view$ = this.uiRepository.view$;

  connected$ = this.client.connectionStateChange.asObservable();
  helgenStatus$ = this.client.helgenStatus.asObservable();
  menuShortcut$ = this.client.menuShortcut.asObservable();
  returnToGame(): void { this.client.deactivate(); }
  menuOpen$ = this.client.openingMenuChange.asObservable();
  inGame$ = this.client.inGameStateChange.asObservable();
  active$ = this.client.activationStateChange.asObservable();
  connectionInProgress$ = this.client.isConnectionInProgressChange.asObservable();
  revealingInProgress$ = false;

  @ViewChild('chat') private chatComp!: ChatComponent;
  @ViewChild(GroupComponent) private groupComponent: GroupComponent;

  private shortcutBox?: HTMLElement;
  private readonly shortcutObserver = new MutationObserver(() =>
    this.fitShortcut(),
  );

  @ViewChild('shortcut', { read: ElementRef })
  private set shortcut(ref: ElementRef<HTMLElement> | undefined) {
    this.shortcutObserver.disconnect();
    this.shortcutBox = ref?.nativeElement;
    if (this.shortcutBox) {
      // Labels change with the connection state and the language.
      this.shortcutObserver.observe(this.shortcutBox, {
        childList: true,
        characterData: true,
        subtree: true,
      });
      this.fitShortcut();
    }
  }

  public constructor(
    private readonly destroy$: DestroyService,
    private readonly client: ClientService,
    private readonly sound: SoundService,
    private readonly uiRepository: UiRepository,
    private readonly translocoService: TranslocoService,
    private readonly settingService: SettingService,
    public readonly overlay: Overlay, // used for mockup
  ) {
    this.translocoService.setActiveLang(
      this.settingService.settings.language.getValue(),
    );
  }

  public ngOnInit(): void {
    this.onInGameStateSubscription();
    this.onActivationStateSubscription();
    this.onFontSizeSubscription();
    // Text metrics change once the font arrives, and rounding changes with the view size.
    document.fonts.addEventListener('loadingdone', () => this.fitShortcut());
    window.addEventListener('resize', () => this.fitShortcut());
  }

  // The multiplayer box has a fixed rectangle (see root.component.scss). English fits at full size; a longer
  // translation scales the box's text down in 5% steps until no label is cut. Labels change only while the box is
  // hidden or right after a click, so these few layouts never cost a visible frame.
  private fitShortcut(): void {
    const box = this.shortcutBox;
    if (!box) {
      return;
    }
    const entries = box.querySelector('.multiplayer-shortcut-entries');
    const buttons = Array.from(box.querySelectorAll('button'));
    const last = buttons[buttons.length - 1];
    // Rows never shrink below their lines, so they overflow downwards when they do not fit (the box's padding below
    // takes a sub-pixel of rounding). Line boxes are 1em, so scrollHeight would also count the glyphs' unused descent.
    const overflows = () =>
      (!!entries &&
        !!last &&
        last.getBoundingClientRect().bottom >
          entries.getBoundingClientRect().bottom + 1) ||
      buttons.some(b => b.scrollWidth > b.clientWidth);
    let fit = 1;
    box.style.setProperty('--sbt-fit', '1');
    while (fit > 0.5 && overflows()) {
      fit -= 0.05;
      box.style.setProperty('--sbt-fit', fit.toFixed(2));
    }
  }

  public onInGameStateSubscription() {
    this.client.inGameStateChange
      .pipe(takeUntil(this.destroy$))
      .subscribe(state => {
        if (!state) {
          this.closeView();
        }
      });
  }

  public onActivationStateSubscription() {
    this.client.activationStateChange
      .pipe(takeUntil(this.destroy$))
      .subscribe(state => {
        if (
          this.client.inGameStateChange.getValue() &&
          state &&
          !this.uiRepository.isViewOpen()
        ) {
          setTimeout(() => this.chatComp.focus(), 100);
        }
        if (!state) {
          this.closeView();
        }
      });
  }

  public onFontSizeSubscription() {
    this.settingService.settings.fontSize
      .pipe(
        takeUntil(this.destroy$),
        map(size => fontSizeToPixels[size]),
      )
      .subscribe(size => {
        document.documentElement.setAttribute('style', `font-size: ${size}px;`);
      });
  }

  public setView(view: View | null) {
    this.uiRepository.openView(view);

    if (view) {
      this.sound.play(Sound.Focus);
    } else if (this.chatComp) {
      this.chatComp.focus();
    }
  }

  public closeView() {
    this.uiRepository.openView(null);
  }

  public reconnect(): void {
    this.client.reconnect();
  }

  public revealPlayers(): void {
    if (this.revealingInProgress$)
      return;

    this.revealingInProgress$ = true;
    setTimeout(() => { this.revealingInProgress$ = false }, REVEAL_EFFECT_DURATION_MS);

    this.sound.play(Sound.Focus);
    this.client.revealPlayers();
  }
}
