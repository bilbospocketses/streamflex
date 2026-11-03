# The providers' terms of service, read against what the overlay does

Date: 2026-11-03. Read-only research; nothing was signed up for or changed. This is a reading of the terms' text and of
what has happened in practice. It is not legal advice.
Tags: VERIFIED = the live page was loaded and the quote copied from its text. REPORTED = a search summary or a secondary
source, not re-checked. UNKNOWN = not established.

## 0. Why this note exists

The 2026-09-28 research (`SERVICES.md` §1b) recorded one-line terms risks per service. Several looked as if they reached
an overlay that "simply reads what is offered by the page and makes it accessible to the d-pad". This pass reads each
provider's current US terms in full, quotes the clauses, and checks how such clauses have been used. Several pages
are newer than the September reading (Disney+, Prime Video, Amazon, HBO Max US, Apple, Tubi, Twitch, Spotify, Emby).

What the overlay does on a provider's page (spec `design/specs/2026-10-27-streaming-overlay-design.md`): an
extension content script in the isolated world reads the page to find focusable controls; the overlay draws its own
layer (focus ring, toasts, keyboard, menu) in a closed shadow root; it moves focus with `focus()`; the user's remote
presses arrive as real OS keystrokes from the launcher (never faked events). It never blocks or skips ads, never
presses anything on its own, never touches DRM or the stream, and never scrapes or downloads.

## 1. The clauses, by family

**"Insert code" and "modify the service".** Read literally, a content script is code running in the provider's page,
even when it only reads. In context, these lists are aimed at tampering, malware and cheating.
- Netflix (Terms of Use, last updated 2026-04-10, VERIFIED) §1.8: "(vi) insert any code or product or manipulate the
  content of the Netflix service in any way"; "(iii) circumvent, remove, alter, deactivate, degrade, block, obscure or
  thwart any of the content protections or other elements of the Netflix service, including the graphical user
  interface, any advertising or advertising features".
- Peacock (updated 2025-12-03, VERIFIED) §5.1: "You may not insert any code into the Peacock Service or Upload a
  software virus or any other computer code or materials that may (i) disrupt, damage, or limit the functioning of
  the Peacock Service ... or (ii) enable or facilitate unauthorized access".
- YouTube (2023-12-15, VERIFIED): you may not "alter, modify or otherwise use any part of the Service or any Content
  except: (a) as expressly authorized by the Service" or "otherwise interfere with any part of the Service".
- HBO Max US (2026-06-24, VERIFIED) §4.3: "modify ... frame ... or in any way exploit any part of the Platform, or
  attempt to interfere with the operation of the Platform"; "frame any portion of the Platform".
- MGM+ (2023-11-02, VERIFIED) §6: "modifies, disrupts, impairs, alters or interferes with the use, features,
  functions, operation or maintenance of the Applications (including ... uses that can damage, disable, overburden,
  or impair the Applications)".
- Twitch (2026-08-12, VERIFIED) §10(viii): "modify, adapt, translate, or create derivative works based upon the
  Twitch Services". Spotify User Guidelines (VERIFIED): "deleting or altering any part of the Services or Content
  except as expressly permitted". Disney+/ESPN/Hulu (one agreement, updated 2026-10-22, VERIFIED) §3(c)(vi): "bypass,
  modify, defeat, tamper with or circumvent any of the functions or protections of the Services".

**"Enhance or alter the video player"** (one template, shared by Paramount+, Pluto TV and Tubi). The first sentence is
broad; the second says what it is aimed at.
- Paramount+ (2026-09-15, VERIFIED) §9.6: "you may not modify, interfere with, enhance, remove, or otherwise alter in
  any way: (a) any portion of the video player on the Service ... This restriction includes, without limitation,
  disabling, modifying, reverse engineering, interfering with or otherwise circumventing the Video Player in any
  manner that enables users to view Content without: (i) visibly displaying both the Video Player and all
  surrounding elements (including the graphical user interface, any advertising, copyright notices, and trademarks)
  ...; and (ii) having full access to all functionality of the Video Player".
- Pluto TV (Paramount's terms, 2024-11-07, VERIFIED) §5.3 and Tubi (2026-03-03, VERIFIED, "The Video Player") carry
  the same first sentence. Peacock has the same second sentence in its own words.
- The only thing the spec wrote to a player was `disablePictureInPicture` on the video element. That is a literal
  "alter ... any portion of the video player", and it is dropped (§3).

**"Scripts", "automated means" and "monitoring".** Nearly every provider. The surrounding text is about scraping, bots,
data mining and AI training.
- Disney+/ESPN/Hulu §3(c)(viii): "access, monitor, copy, or extract the Services using a robot, spider, script or other
  automated means, including, for the avoidance of doubt, for the purposes of creating or developing any AI Tool, data
  mining or web scraping".
- Apple Media Services (2026-09-14, VERIFIED) §F: "You may not use any software, device, automated process, or any
  similar or equivalent manual process to scrape, copy, or perform measurement, analysis, or monitoring of, any
  portion of the Content or Services." Crunchyroll (undated, VERIFIED) §5: "robot, spider, scraper, deep-link, mod,
  hack, exploit, cheat utility, trainer, or other automated data gathering or extraction tool ... to access,
  acquire, modify, copy, monitor, or otherwise interfere".
- Paramount+ §10.1(h): "use any robot, bot, scraper ... or other manual or automatic device, method, system or process
  to access, retrieve, index, "data mine", or in any way reproduce or circumvent the navigational structure or
  presentation of the Service (including any geo-filtering mechanisms, or any digital rights management measures)".
  Its examples are geo-filtering and DRM, but "navigational structure" is the nearest wording to a navigation layer.

**"Only our interface or software".** The strongest wording.
- Apple §F: "You may access our Services only using Apple's software, and may not modify or use modified versions of
  such software." (Apple TV+ on the web only runs in third-party browsers, so this cannot literally mean no other
  software.)
- Pluto TV §9: "Access the Services using any interface other than ours". Peacock §5.1: "access, search and/or collect
  data from ... the Peacock Service by any means (whether automated or manual) other than through our currently
  available, published interfaces". YouTube TV (2026-02-11, VERIFIED) §1: delivered "via authorized YouTube players
  and/or players on permitted devices".
- The overlay's position: the subscriber uses the provider's own web interface in an ordinary browser.

**"Surround with third-party branding" and false association.**
- Peacock §5.1: "you may not otherwise surround or obfuscate the Peacock Service or Content with any third-party
  content, materials, or branding". Paramount+ §10.1(d): "display the Service or Content in connection with an
  unauthorized logo or mark, or do anything that could falsely suggest a relationship between us ... and any third
  party".

**Commercial use.** Netflix §1.8(i), Disney+ §3(c)(iv) ("build a business utilizing the Content or Services"),
Paramount+ §10.1(i) ("build a business or other enterprise utilizing the Service ... whether for profit or not"),
Peacock §5.1, MGM+ §6, Twitch §7(a). These bind the subscriber who accepted them.

**Ad blocking.** Disney+ §3(c)(ix), Paramount+ §5.1(d), Spotify, YouTube TV. The overlay never blocks ads. YouTube TV's
recording clause bars technology to "automatically tune away from, or to skip or delete (other than manual
scrubbing), advertising", which matches the overlay's rule exactly.

**Low or no exposure.** Prime Video Terms (2026-08-26, VERIFIED) bar only DRM circumvention and commercial use;
Amazon's Conditions of Use (2026-08-14, VERIFIED) bar "data mining, robots, or similar data gathering and extraction
tools". Plex (2025-10-30, VERIFIED) contemplates third-party client apps; Emby (2026-08-16, VERIFIED) has no
automation or overlay clause; Jellyfin is self-hosted open source.

**Accessibility.** HBO Max's accessibility statement (VERIFIED): "Our platforms are designed to work with commonly used
assistive technologies provided by operating systems and devices, including screen readers and keyboard navigation."
That is how the overlay drives a page: real keystrokes and focus. Netflix's help center lists its keyboard shortcuts
(REPORTED). The others carry only caption notices or nothing.

## 2. How such clauses have been used

- No reported cease-and-desist, store takedown, lawsuit or account ban against an extension that only changes or adds
  to the UI was found for Netflix, Disney, Hulu, Amazon, Paramount, Max, Peacock or Apple (a thin search: UNKNOWN,
  not "none exist"). Teleparty has run for years across Netflix, Disney+, Hulu, HBO Max, Prime Video and YouTube,
  names them in its listing, and has a premium tier (REPORTED).
- The trouble that did happen concerned ads, DRM, scraping or content reuse: YouTube's campaign against ad blockers
  (2023 on), Streamus (a cease-and-desist and a revoked API key over reusing YouTube content, 2015), Bypass Paywalls
  Clean (a DMCA anti-circumvention takedown, 2024), Netflix's suits against piracy boxes (TickBox, Dragon Box).
  Netflix's help tells users to disable the "Netflix 1080p" extension, which touches playback quality (REPORTED).
- US frame: Van Buren v. United States (2021, VERIFIED) read "exceeds authorized access" narrowly; it leaves open
  whether contract limits count (footnote 8), so a terms breach points toward a contract matter without settling it.
  The terms bind the subscriber; a developer who never accepted them is not a party, and the theory aimed at
  developers is tortious interference (pleaded in Craigslist v. Papp, outcome not found). DMCA §1201 is about
  circumventing access controls, which the overlay does not do.
- Germany: Axel Springer v. Eyeo (BGH I ZR 131/23, 31 July 2025, partly VERIFIED) remanded a copyright claim that an
  ad blocker modifies a computer program; nothing final.
- Accessibility law: the CVAA's user-interface rule (47 CFR 79.107, VERIFIED) binds makers of video apparatus, not
  streaming websites, and puts no duty on a third-party layer. The EU Accessibility Act (from 2025-06-28, REPORTED)
  covers audiovisual services' compatibility with assistive technology and alternative input; a goodwill argument,
  not a shield.

## 3. What the design takes from this (decided together, 2026-11-03)

1. **No write to any provider's player.** `disablePictureInPicture` is dropped from the spec.
2. **No write to the provider's page at all.** The overlay reads, moves focus and draws inside its own layer; it sets
   no attribute, style or DOM node on the provider's elements. A test enforces it.
3. **Our layers never hide the provider's player UI, advertising or notices for long.** The focus ring outlines and
   does not cover; the keyboard and menu are temporary and close away; nothing ever covers an ad.
4. **Provider names and marks.** README and the overlay's About text say plainly that StreamFlex is not affiliated
   with or endorsed by the streaming services it opens. The tile icons stay as `assets/icons/library/brands/NOTICE.md`
   handles them (each owner credited; removal on request).

The remaining risks are the subscriber's terms (an account action, rare), a provider blocking the extension
technically, a store takedown request, or a letter to the project. A lawyer's reading of these clauses comes before
the overlay ships.
