# StreamFlex: triage of candidate "other" services (US, 2026-09-28)

Scope: schema fields 1, 2, 8, 9, 10 plus a one-line note on field 4. Tags: VERIFIED (official or several recent sources), REPORTED (single or secondary source), UNKNOWN (not found).

**Research history.** The first pass ran out of shared search budget, so a second pass (2026-09-28, about 40 searches plus page fetches) triaged the remaining 14: Starz, AMC+, BritBox, Discovery+, Shudder, Criterion Channel, Kanopy, Curiosity Stream, DAZN, NBA League Pass, MLB.tv, Apple Music web, Pandora and Spotify web. Several official help pages returned 403 or truncated content (Starz, Criterion, NBA, MLB, Fubo), so those claims rest on search snippets and are tagged REPORTED. No source read mentions a 10-foot or TV mode in the browser for any of the 14, so that column is UNKNOWN throughout.

**General baseline (REPORTED).** Firefox on Windows and Linux uses Widevine L3 (software), so DRM services are commonly capped near 720p there, while Windows Edge can use PlayReady (up to 4K on services that allow it). https://www.forasoft.com/learn/video-streaming/articles-streaming/widevine-l1-l2-l3 ; https://itsfoss.com/netflix-firefox-linux/ . Whether each service allows more than 720p on Windows Chrome or Firefox is UNKNOWN unless stated.

## Triage table

| Service | Win Chrome / Edge / Firefox | Linux (Firefox unless a cell says otherwise; see "Linux per browser" below) | Max quality (browser) | US weight | 10-foot web UI | Overlay difficulty | Early tier? |
|---|---|---|---|---|---|---|---|
| Plex (web) | yes / yes / yes (REPORTED) | yes (REPORTED) | Direct play of own files; DRM-free | large (MAU UNKNOWN) | No 10-foot in the web app; the separate Plex HTPC app is the 10-foot UI (VERIFIED) | low-med | YES |
| Jellyfin (web) | yes / yes / yes (VERIFIED, 2 latest versions) | yes (VERIFIED, Firefox and ESR supported) | Direct play; DRM-free | niche self-hosted (UNKNOWN count) | YES: TV layout in web client, active keyboard/remote fixes (VERIFIED) | low | YES |
| Tubi | yes (REPORTED) | UNKNOWN | UNKNOWN | 110M MAU global, 2.3% of US TV time (VERIFIED) | UNKNOWN | low-med | YES |
| Pluto TV | yes (REPORTED) | UNKNOWN | UNKNOWN | ~80M MAU global (REPORTED) | UNKNOWN | med | YES |
| Twitch | yes (REPORTED) | yes (REPORTED, no DRM) | up to source (1080p60, higher for some partners) | ~37M US users (REPORTED) | UNKNOWN | med | maybe |
| The Roku Channel (web) | web exists at therokuchannel.roku.com (VERIFIED) | UNKNOWN | UNKNOWN | large (UNKNOWN count) | UNKNOWN | med | maybe |
| Freevee | Shut down; folded into Prime Video "Watch for Free" (VERIFIED) | n/a | n/a | n/a | n/a | n/a | EXCLUDE (covered by Prime Video) |
| ESPN | ESPN.com min: Chrome 50, Edge 12, Firefox 48 (VERIFIED, old page) | UNKNOWN (old reports say it works) | UNKNOWN | large (UNKNOWN) | UNKNOWN | med-high | Tier 2 |
| Fubo (now with Hulu + Live TV) | Chrome, Edge, Firefox, Safari (REPORTED) | not listed (UNKNOWN) | UNKNOWN | 5.9M combined subs, Q2 2026 (REPORTED) | UNKNOWN | high (live guide) | Tier 2-3 |
| Sling TV | Chrome, Edge, Firefox (VERIFIED); Edge 4K via PlayReady (REPORTED) | Not officially supported; workaround needs UA change (REPORTED) | up to 4K on Edge | UNKNOWN | UNKNOWN | med-high | Tier 3 |
| Philo | Chrome, Edge, Firefox, Safari on Mac/PC (VERIFIED) | Not listed, so not supported (VERIFIED by omission) | UNKNOWN | ~1M+ subs (REPORTED) | UNKNOWN | med | Tier 3 |
| Spotify (web player) | Chrome, Firefox, Edge, Opera, Safari (VERIFIED) | Not excluded: official page tells Firefox users to click "Enable DRM" (VERIFIED); older community threads say the new player failed on Linux Firefox (REPORTED, probably old) | Audio only; Widevine needed | 777M MAU, 300M paid, global, Q2 2026 (VERIFIED); US share UNKNOWN | UNKNOWN (none found) | low-med | Tier 2 (audio) |
| Apple Music (web) | Chrome, Edge, Firefox, Opera, Safari (REPORTED, third-party pages) | CONFLICTING: one source says Firefox works after enabling DRM, another says Chromium is needed (REPORTED); UNKNOWN | Audio only; Widevine | Apple does not publish (UNKNOWN) | UNKNOWN | low-med | Tier 3 |
| Pandora | Firefox troubleshooting page exists; no supported-browser list read (REPORTED) | Not officially supported on Linux (REPORTED) | Audio only | 39.8M MAU, 5.6M paid, Jun 30 2026, US, declining 7% YoY (VERIFIED, SiriusXM 10-Q) | UNKNOWN | low-med | Tier 3 |
| Starz | Edge, Chrome, Firefox, Safari (REPORTED, search snippet of official page) | UNKNOWN; old forum reports of DRM errors on Linux Firefox (REPORTED) | UNKNOWN | 19.2M North America (US+Canada, OTT+linear), Q4 2025; no longer disclosed (REPORTED) | UNKNOWN | med | Tier 3 |
| AMC+ | UNKNOWN (help pages say only "use a web browser") | UNKNOWN | UNKNOWN | 10.1M across all AMC streaming services (AMC+, Acorn, Shudder, etc.), Q1 2026, no longer reported (REPORTED) | UNKNOWN | med | Tier 3 |
| BritBox | Chrome, Firefox, Opera, Edge, Safari (REPORTED, third-party) | UNKNOWN | UNKNOWN | ~4.5M global, 2026 (REPORTED, vague) | UNKNOWN | med | Tier 3 |
| Discovery+ | Chrome, Firefox, Edge on Windows, Safari (VERIFIED) | Linux not mentioned (VERIFIED by omission) | UNKNOWN | Not broken out by WBD; reporting stopped after Q4 2025 (REPORTED) | UNKNOWN | med | Tier 3 |
| Shudder | Chrome, Safari, Firefox (VERIFIED); Edge not named | Not stated officially; a third-party page says Linux works (REPORTED) | UNKNOWN | Small; inside the AMC 10.1M (REPORTED) | UNKNOWN | med | Tier 3 |
| Criterion Channel | Chrome and Firefox (REPORTED); official page not readable | Linux/ChromeOS reportedly blocked (REPORTED, old forum); Firefox reportedly struggles above 720p (REPORTED) | UNKNOWN | Niche; count UNKNOWN | UNKNOWN | med | Tier 3 (Linux risk) |
| Kanopy | Chrome, Safari, Firefox, Edge on Windows, Mac, Chromebook (VERIFIED) | Linux not listed (VERIFIED by omission) | UNKNOWN | Free via library card; users UNKNOWN; library credit caps cut Feb 2026 (REPORTED) | UNKNOWN | med | Tier 3 |
| CuriosityStream | Firefox 54+, Chrome 58+, Edge 16+ (REPORTED, old list) | UNKNOWN (help page not readable) | UNKNOWN | Subscription revenue flat ~$9M/quarter, subscriber count falling (REPORTED); business shifting to AI data licensing | UNKNOWN | med | EXCLUDE |
| DAZN | Win: Chrome 103+, Edge 103+, Firefox 102+, all up to 1080p (VERIFIED) | Not officially supported; Chrome/Firefox "may work", up to 720p (VERIFIED) | 1080p Win; 720p Linux | UNKNOWN; price raised to $34.99/mo after Sep 10 2026 (REPORTED) | UNKNOWN | med-high | Tier 3 |
| NBA League Pass | Edge, Chrome, Firefox (REPORTED); Edge 1080p, Firefox 720p (REPORTED, user reports) | Linux not supported (REPORTED, user reports) | Edge 1080p; Firefox 720p | UNKNOWN | UNKNOWN | high | Tier 3 |
| MLB.tv | Edge, Firefox, Chrome, Safari on Windows 10/11 (REPORTED, search snippet of official page) | Linux not listed (REPORTED) | UNKNOWN | UNKNOWN; sold through ESPN since 2026 (REPORTED) | UNKNOWN | high | Tier 3 (fold under ESPN) |

## Linux per browser (targets updated 2026-09-28: Linux Chrome, Edge and Firefox)

The table's Linux column was researched for Firefox only. Most sources do not separate the three Linux browsers, so where they do not, treat one cell as covering all three (UNKNOWN per browser).

- **Widevine level.** Desktop Chrome exposes Widevine L3, which studios cap at 720p or lower; on Linux the CDM is L3 in practice, and the only officially supported Widevine-on-Linux route is Chrome on x86_64 (REPORTED, general): https://www.forasoft.com/learn/video-streaming/articles-streaming/widevine-l1-l2-l3 . Caps are set by each service, not by Widevine (REPORTED). Edge for Linux's Widevine behavior: UNKNOWN, nothing found. Firefox on Linux uses a downloaded L3 CDM (REPORTED, earlier baseline).
- **Chrome and Edge are chromium, so they usually pass the same user-agent and DRM checks and are the safer Linux bets** for the services that sniff (REPORTED inference, not tested). Firefox is where the old "protected content" and UA-workaround reports cluster (Spotify, Sling, Starz).
- **DAZN:** the official matrix lists Linux Chrome 103+ and Linux Firefox 102+, both up to 720p; Edge is not listed on Linux (VERIFIED): https://www.dazn.com/en-US/help/articles/16271409386269-which-browsers-does-dazn-support .
- **Spotify:** Chrome's fix is the "Protected content IDs" setting for open.spotify.com; Edge has only a Windows 10 N media-pack note; Firefox has the "Enable DRM" bar (VERIFIED): https://support.spotify.com/us/article/web-player-help/ . Linux Chrome should be the lowest-risk of the three (inference).
- **NBA League Pass, Criterion:** reports say Linux is unsupported or blocked as an OS, so all three Linux browsers are suspect, not just Firefox (REPORTED).
- **Kanopy, Discovery+, MLB.tv, Sling, Philo:** Linux is absent from the official lists, so no Linux browser is officially supported (VERIFIED by omission).
- **Everything else in the 14:** Linux Chrome and Edge are UNKNOWN. A one-hour hands-on test of Spotify, Plex, Tubi, Pluto TV and Twitch on Linux Chrome, Edge and Firefox would settle the recommended set.

### Linux matrix: Chrome / Edge / Firefox (all services)

"Same" means the source names the browser family without splitting by OS, so Linux is assumed to behave like the listed browsers; that assumption is an inference unless a Linux mention is quoted. Widevine on Linux is L3 for all three (REPORTED, above).

| Service | Linux Chrome | Linux Edge | Linux Firefox | Basis |
|---|---|---|---|---|
| Jellyfin | supported | supported | supported (and ESR) | Official client page lists Firefox, Firefox ESR, Chrome, Edge with no OS split; DRM-free (VERIFIED, family-level): https://jellyfin.org/docs/general/clients/ |
| Plex (web) | supported | supported | supported | Support page lists recent Chrome, Edge, Firefox, Safari; Plex apps cover Linux desktops (REPORTED, search snippet of support.plex.tv; page returned 403): https://support.plex.tv/articles/200392226-plex-web-app-player/ |
| Tubi | supported | supported | supported | Official page lists Chrome, Safari, Firefox, Edge; Linux not mentioned (VERIFIED family-level): https://tubitv.com/static/supported-browsers |
| Pluto TV | works | works | works | Only a third-party page says Linux Chrome, Firefox and Chromium Edge work (REPORTED): https://plotutv.com/on-linux/ ; official support page returned 401 |
| Twitch | supported | supported | supported | Official list is Chrome, Firefox, Edge, Safari on the latest two versions (REPORTED, search snippet; page did not load): https://help.twitch.tv/s/article/supported-browsers?language=en_US ; no DRM |
| Spotify | works with the "Protected content" setting | UNKNOWN | works with "Enable DRM"; older reports of failure (REPORTED) | https://support.spotify.com/us/article/web-player-help/ |
| The Roku Channel | UNKNOWN | UNKNOWN | UNKNOWN | Roku Help does not list Linux as a supported OS; a community thread on Linux browsers and Widevine exists but was not readable (REPORTED): https://community.roku.com/t5/Channels-viewing/Linux-Web-Browsers-and-The-Roku-Channel-DRM-Widevine-Protection/td-p/776117 |
| ESPN | UNKNOWN | UNKNOWN | UNKNOWN | ESPN.com page lists Chrome 50+, Edge 12+, Firefox 48+, Safari 10+ with no Linux; the live-streaming page lists Windows 8+ and OS X only, and is stale (references Flash) (VERIFIED, both old): https://support.espn.com/hc/en-us/articles/360028985692-ESPN-com-Minimum-Browser-Requirements |
| Fubo, Philo | Fubo UNKNOWN; Philo not supported | same | same | Philo lists Mac/PC only; Fubo Linux not stated (see notes) |
| Sling | UA workaround reported | UNKNOWN | UA workaround reported | Not officially supported on Linux (REPORTED, probably old) |
| DAZN | 720p (listed) | not listed | 720p (listed) | VERIFIED: https://www.dazn.com/en-US/help/articles/16271409386269-which-browsers-does-dazn-support |
| NBA League Pass | suspect | suspect | suspect | Linux unsupported per user reports (REPORTED) |
| MLB.tv | not listed | not listed | not listed | Linux absent (REPORTED) |
| Discovery+, Kanopy | not listed | not listed | not listed | Linux absent from official pages (VERIFIED by omission) |
| Criterion | reportedly blocked | reportedly blocked | reportedly blocked | Old forum posts (REPORTED) |
| Shudder, BritBox, Starz, AMC+, Curiosity, Pandora, Apple Music | UNKNOWN | UNKNOWN | see rows above | Not resolved; no official Linux statement found |

Reading of the matrix: only DRM-free or open-format services (Jellyfin, Plex, Twitch) are confirmed on all six targets. Tubi and Pluto TV are listed for the browser families and should work on all three Linux browsers, but neither names Linux, so they need a smoke test.

## Per-service notes

**Plex.** Plex's 10-foot interface is Plex HTPC (Windows, Mac, Linux; Flathub and Snap builds), not the browser app. VERIFIED: https://support.plex.tv/articles/htpc-getting-started/ , https://www.plex.tv/blog/way-to-be-htpc/ , https://flathub.org/en/apps/tv.plex.PlexHTPC . For StreamFlex this cuts both ways: many HTPC users already have HTPC, but app.plex.tv in a browser has no TV layout, so an overlay adds real value there. It is DRM-free for personal media. UI framework and keyboard accessibility: UNKNOWN.

**Jellyfin.** Officially supports the two latest versions of Firefox and Firefox ESR, Chrome and Edge. VERIFIED: https://jellyfin.org/docs/general/clients/ . The web client has a TV layout and the 12.0 release (September 8, 2026) fixes TV-layout focus and keyboard handling. VERIFIED: https://github.com/jellyfin/jellyfin-web/releases/tag/v12.0 . Open issues show some pages (Quick Connect, forgot password) were not focusable by keys in TV layout. REPORTED: https://github.com/jellyfin/jellyfin-web/issues/7030 . So D-pad navigation is partly native and the overlay should only fill gaps. The client is self-hosted, so the URL is user-configured.

**Tubi.** 110M MAU (global, Aug 6, 2026), up 14% year over year, and 2.3% of US TV viewing in April 2026 per Nielsen. VERIFIED: https://corporate.tubitv.com/press/tubi-reaches-110-million-monthly-active-users-as-record-engagement-and-growing-fandoms-fuel-continued-momentum/ ; https://thedesk.net/2026/08/tubi-user-count-july-2026/ . Free with ads, and needs no sign-in for most content (REPORTED). Resolution, Linux behavior, DRM and UI tech: UNKNOWN.

**Pluto TV.** About 80M MAU globally, the last reported figure. REPORTED: https://streamingbetter.com/pluto-tv-80-million-monthly-active-users/ . Free with ads, and its live-channel guide is a grid, which typically makes D-pad focus harder. Browser and Linux support: UNKNOWN.

**Twitch.** About 37.2M US users. REPORTED: https://backlinko.com/twitch-users . No DRM, so it should work everywhere. Whether a browser TV layout exists is UNKNOWN.

**The Roku Channel.** Available in browsers on PCs, phones and tablets at therokuchannel.roku.com. VERIFIED: https://en.wikipedia.org/wiki/The_Roku_Channel . Browser matrix and quality: UNKNOWN.

**Freevee.** Standalone apps were decommissioned in September 2025, and the content moved into Prime Video. VERIFIED: https://www.cnbc.com/2025/07/02/amazon-freevee-streaming.html ; https://www.howtogeek.com/this-is-when-amazon-freevee-will-shut-down/ . Nothing to build; it is covered by the Prime Video entry.

**ESPN.** The ESPN.com browser-requirements page lists Chrome 50+, Edge 12+, Firefox 48+, Safari 10+ (VERIFIED, but the page is old and says nothing on Linux or the new DTC app): https://support.espn.com/hc/en-us/articles/360028985692-ESPN-com-Minimum-Browser-Requirements . Old forum reports say ESPN plays on Linux Firefox, which is not current evidence (REPORTED): https://www.tigerdroppings.com/rant/tech/watching-espn-on-linux/83013670/ .

**Fubo.** Supports the latest Chrome, Edge, Firefox and Safari. REPORTED (the official help page returned 403 to fetch, so this is from a search snippet): https://support.fubo.tv/hc/en-us/articles/44362610799117-How-do-I-watch-Fubo-on-my-Web-Browser . The Hulu + Live TV merger closed on October 29, 2025, and the combined subscriber count was 5.9M in Q2 2026 (down from 6.2M in Q1). REPORTED: https://www.hollywoodreporter.com/business/business-news/fubo-subscribers-second-quarter-1236587709/ . Linux support is not mentioned (UNKNOWN).

**Sling TV.** Help center says all modern browsers should work, and names Chrome, Firefox and Edge. VERIFIED: https://support.getsling.com/en/articles/511200-which-browser-should-i-use-for-sling . Linux is reportedly unsupported, with a Firefox-plus-DRM-plus-UA-change workaround: https://linuxconfig.org/watch-sling-tv-with-firefox-on-linux (REPORTED, and probably old). Edge is reported to get 4K via PlayReady: https://windowsreport.com/best-browser-sling-tv/ (REPORTED).

**Philo.** Watch on Mac or PC in the latest Safari, Chrome, Edge or Firefox; Linux is not listed. VERIFIED: https://help.philo.com/using-philo/browsers/ . Pricing since March 10, 2026: Essential $25, Bundle+ $33. REPORTED: https://deadline.com/2026/03/streaming-pay-tv-service-philo-cheaper-subscription-tier-1236746066/ . Subscribers are "more than 1 million" (REPORTED, https://en.wikipedia.org/wiki/Philo_(company)).

**Spotify (web).** The official web-player help page lists Chrome, Firefox, Edge, Opera and Safari (baseline: updated within the last 30 months) and says Firefox users who see "Playback of protected content is not enabled" should click "Enable DRM" or enable "Play DRM content" in settings. VERIFIED: https://support.spotify.com/us/article/web-player-help/ . So Widevine is required and Linux Firefox is not blocked by policy. Older community threads report the new player failing on Linux Firefox and a User-Agent workaround (REPORTED, undated, likely stale): https://community.spotify.com/t5/Other-Partners-Web-Player-etc/Spotify-web-player-in-Firefox-on-linux-won-t-play-Widevine/td-p/1717348 . Weight: 777M MAU and 300M premium subscribers globally at Q2 2026 (reported Aug 4, 2026; VERIFIED, several sources): https://variety.com/2026/music/news/spotify-300-million-subscribers-q2-2026-earnings-1236826641/ . US-only figure UNKNOWN. Audio only, so no resolution ceiling. Needs a hands-on test on Linux Firefox before committing.

**Apple Music (web).** Third-party pages list Chrome, Edge, Firefox, Opera and Safari (REPORTED): https://wavebrowser.co/blog/apple-music-browser-player . On Linux the evidence conflicts: one guide says Firefox works after you click to enable DRM, another says Chromium with a DRM-capable build is needed (REPORTED): https://www.linuxfordevices.com/tutorials/apple-music-linux . No Apple support page was reachable (404). Subscriber count is not published by Apple (UNKNOWN).

**Pandora.** Pandora states it does not officially support Linux clients (REPORTED, search snippet of the help center): https://help.pandora.com/s/article/Firefox-Troubleshooting-1519949297419 . Weight is shrinking: about 39.8M monthly active users at June 30, 2026, down 7% year over year, and about 5.6M paid subscribers (VERIFIED, SiriusXM 10-Q): https://www.sec.gov/Archives/edgar/data/0000908937/000090893726000022/siri-20260630.htm .

**Starz.** Supports current Edge, Chrome, Firefox and Safari (REPORTED, search snippet of https://support.starz.com/en_us/supported-devices-B1GOf4WJj ; the fetched page was truncated). Linux: an old forum thread shows Firefox reporting missing DRM components (REPORTED): https://forum.vivaldi.net/topic/34948/can-t-stream-starz-video . Last disclosed size was 19.2M North America subscribers (US and Canada, OTT and linear) for Q4 2025; Starz stopped disclosing after Q1 2026 (REPORTED): https://investors.starz.com/news-releases/news-release-details/starz-entertainment-corp-reports-results-fourth-quarter-ended . Now sold in a $29.99 five-service Prime Video bundle (REPORTED): https://thedesk.net/2026/09/amazon-new-bundle-starz-amc-plus-britbox/ .

**AMC+ (and Shudder).** No official browser list found: the AMC+ device page says only that it works "using a web browser" (VERIFIED, but empty): https://support.amcplus.com/kb/guide/en/what-devices-does-amc-support-mJTWlwqXSj/Steps/3678016 . AMC Global Media reported 10.1M streaming subscribers across AMC+, Acorn, Shudder and others in Q1 2026 and stopped regular reporting (REPORTED): https://variety.com/2026/tv/news/amc-global-streaming-ad-sales-q2-earnings-1236824072/ . Shudder officially says "Chrome, Safari, or Firefox" and gives no OS or resolution detail (VERIFIED): https://support.shudder.com/kb/guide/en/shudder-on-web-dLwVzPFEei/Steps/4685119 ; a claim that Linux works comes only from a third-party site (REPORTED).

**BritBox.** Supports Chrome, Firefox, Opera, Edge and Safari (REPORTED, third-party): https://windowsreport.com/britbox-browser/ . About 4.5M subscribers worldwide in 2026 (REPORTED, vague sourcing; an earlier report gave 4M across US, Canada, Australia and the Nordics in March 2025): https://www.hollywoodreporter.com/tv/tv-news/britbox-us-subscriber-update-4-million-1236153363/ . US share UNKNOWN.

**Discovery+.** Help center recommends the latest Chrome, Firefox, Microsoft Edge (Windows) or Safari; no Linux and no resolution information (VERIFIED): https://help.discoveryplus.com/hc/en-us/articles/32717633822615-discovery-supported-devices . Warner Bros. Discovery lumps it into a combined figure and stopped regular subscriber reporting after Q4 2025 (REPORTED): https://www.sec.gov/Archives/edgar/data/1437107/000143710726000055/a992wbd1q26earningsshare.htm .

**Criterion Channel.** The official help page was not readable (403 or redirect). Forum posts say it plays on Chrome and Firefox, that Firefox may struggle above 720p, and that Chrome OS/Linux is blocked (REPORTED, old): https://www.criterionforum.org/forum/viewtopic.php?t=16082&start=275 . Treat Linux as a probable block until tested.

**Kanopy.** Official page: use Windows, Mac or Chromebook with the latest Chrome, Safari, Firefox or Edge; Linux is not mentioned (VERIFIED by omission): http://help.kanopy.com/en-us/4148.htm . Needs a participating library card, and several libraries cut Kanopy credit limits from February 2026 (REPORTED): https://lewisborolibrary.org/2026/02/01/changes-to-hoopla-and-kanopy/ . User count UNKNOWN.

**CuriosityStream.** An old-looking list gives Firefox 54+, Chrome 58+, Chromium 58+, Safari 10.1+, Opera 44+, Edge 16+ (REPORTED, search snippet; the help page was not readable): https://help.curiositystream.com/hc/en-us/articles/209260738-What-platforms-and-devices-do-you-currently-support . Subscription revenue is flat at about $9M a quarter with a falling subscriber count, while licensing (AI training data) is now the growth driver (REPORTED): https://www.sec.gov/Archives/edgar/data/0001776909/000162828026056318/pressrelease2q26qcuri-2026.htm . An earlier snippet claimed "25 million paying subscribers"; that includes bundled partners and is not comparable (REPORTED).

**DAZN.** Official browser matrix: Windows 8.1+ with Chrome 103+, Edge 103+ or Firefox 102+, up to 1080p. Linux: "DAZN does not officially support Linux, but it may work on certain distributions using Google Chrome or Mozilla Firefox", up to 720p (VERIFIED): https://www.dazn.com/en-US/help/articles/16271409386269-which-browsers-does-dazn-support . This is the only service in this batch with a documented Linux resolution ceiling (720p). US subscriber count UNKNOWN; the flexible monthly price rose to $34.99 after September 10, 2026 (REPORTED): https://www.worldboxingnews.com/dazn-us-boxing-price-increase-2026/ .

**NBA League Pass.** The official help page returned 403. User reports say Linux is not supported (people use a Windows VM), Edge gets 1080p, Chrome recently also, and Firefox stays at 720p (REPORTED): https://support.google.com/chrome/thread/243389811/nba-pass-desktop-can-t-select-1080p?hl=en . Subscriber count UNKNOWN. Seasonal and sports-fan only.

**MLB.tv.** Search snippet of the official requirements: Edge, Firefox, Chrome and Safari on Windows 10 and 11; Linux not listed (REPORTED): https://support.mlb.com/s/article/Troubleshooting-System-Requirements?language=en_US (fetch failed). Since 2026 ESPN sells and distributes the out-of-market package, with a $135 seasonal price for ESPN Unlimited subscribers (REPORTED): https://www.sportsmediawatch.com/2026/02/smw-faq-mlb-tv-viewership-changing-2026/ . That makes MLB.tv a natural sub-feature of an ESPN entry rather than its own. Subscriber count UNKNOWN.

**Still UNKNOWN after the second pass.** 10-foot web UI for all 14; max resolution on Windows Chrome/Firefox for all but DAZN and NBA; US subscribers for DAZN, NBA, MLB, Kanopy, Criterion, Shudder, Apple Music; AMC+ browser list; Linux Firefox behavior for Starz, AMC+, BritBox, Discovery+, Shudder, Apple Music, Spotify (needs a hands-on test).

## Recommendation: early-tier "others"

1. **Jellyfin.** DRM-free, Linux-friendly (Firefox and Firefox ESR officially supported), and it already ships a TV layout, so the overlay only has to fill gaps. Best HTPC fit and the lowest difficulty.
2. **Plex (web).** DRM-free, a large audience, and the web app has no 10-foot layout, so the overlay adds the most value here. Pair it with a "launch Plex HTPC instead" option if installed.
3. **Tubi.** Biggest free audience (110M MAU, 2.3% of US TV time), no subscription friction, and a plain browsable catalog.
4. **Pluto TV.** Second big free service, but its live channel grid needs careful focus handling. Include it, and expect more overlay work than Tubi.
5. **Twitch.** Large US audience, no DRM, so it works identically on all four targets. Difficulty is medium because of chat and directory-heavy pages.
6. **Optional: The Roku Channel or ESPN.** Only after a follow-up check of the Linux Firefox matrix (both UNKNOWN today).

Defer Fubo, Sling and Philo to Tier 3: they are live-TV guides (a hard overlay target), and Sling and Philo are effectively Windows-only on your target list. Exclude Freevee (dead).

**Second-pass result (2026-09-28): the list above stands.** None of the 14 newly triaged services beats Jellyfin, Plex, Tubi, Pluto TV or Twitch on weight plus Linux safety. One addition is worth considering:

- **Spotify (web) becomes an optional Tier 2 audio entry.** It is by far the largest of the 14 (777M MAU globally), it is audio only (no resolution ceiling), and its official page supports Firefox with a one-click DRM enable. It should only be committed after a hands-on Linux Firefox test, because older reports of failure exist. It also suits an HTPC (music in the background), which video services do not.
- **Tier 3:** DAZN (documented Linux 720p ceiling, boxing niche), NBA League Pass and MLB.tv (sports, seasonal, Linux reportedly unsupported; treat MLB.tv as part of ESPN), Starz, AMC+, BritBox, Discovery+, Shudder, Criterion, Kanopy (all DRM video with Linux unconfirmed or blocked; several sit in the new $29.99 Prime Video bundle, so they matter more as a group than each alone), Apple Music web and Pandora (audio, Linux doubtful, Pandora shrinking).
- **Exclude:** CuriosityStream (flat, shrinking subscriber base, weak evidence of any benefit).

Adding Linux Chrome and Edge as targets does not change the tiers: it helps DRM services that only failed on Firefox (Spotify, Starz, Sling) but does not rescue those whose OS is unsupported or capped (NBA, Criterion, DAZN at 720p, Kanopy, Discovery+, MLB.tv). The DRM-free picks (Jellyfin, Plex, Twitch) are safe on all six targets; Tubi and Pluto TV are listed for Chrome, Edge and Firefox families and reported to work on Linux, but need a per-browser Linux smoke test. ESPN and The Roku Channel have no Linux statement at all, so the optional sixth pick stays conditional on that test. The Roku Channel and ESPN stay as the optional sixth pick, still pending a Linux Firefox check. The cross-cutting risk below is reinforced: every DRM service in this batch either caps Linux at 720p (DAZN, NBA on Firefox), omits Linux (Discovery+, Kanopy, MLB.tv, NBA) or reportedly blocks it (Criterion, Pandora).

**Biggest cross-cutting risk:** Linux Firefox uses Widevine L3, so any DRM service that refuses L3 or the Linux user agent will not play there. Only the DRM-free entries (Jellyfin, Plex, Twitch) are safe on that target without per-service testing.
