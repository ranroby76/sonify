<!-- D:\workspace\Fanan_web_hub\src\sonify_guide.md -->

# Getting Started

## Welcome

Sonify is Fanan's music studio: a DAW for recording, writing, arranging and mixing songs, with Fanan AI built in to do the work when you ask. It is split into studios, picked in the middle of the top bar:

- **Studio**: the DAW, where the song is arranged, edited and mixed (**Ctrl+1**).
- **Analog Board**: mix the song on an analog desk (**Ctrl+2**).
- **AI Tools**: Sonify's AI musical tools, each in a tab: Stem Splitter, Audio to MIDI, MIDI Generators, Tempo & Pitch and the Mastering Assistant (**Ctrl+3**).
- **Song Builder Blocks**: build the song from blocks of clips (**Ctrl+4**).
- **Plugins Creator Studio**: new instruments, effects and MIDI devices, made by asking Fanan AI (**Ctrl+5**).
- **Live Show Studio**: a playlist and its player, singers and players, podcasts and live streams (**Ctrl+6**).
- **Sampling Studio**: an instrument sampled note by note into an SFZ for the Sonify Sampler (**Ctrl+7**).
- **Web Browser**: the internet in tabs, while the song plays (**Ctrl+8**).
- **Guide**: this user guide (**Ctrl+9**).

Fanan AI, the assistant, is a click away in every studio: the **AI Assistant** button at the lower right opens it in a window over the studio. Ask in words, and it builds tracks, writes parts and mixes in your song.

## The Screen

### Top Bar

- On the left, the internet switch (**Internet on** or **Internet off**). Click it to block the internet: then no plugin, web page, Fanan AI or Fanan content can reach the network. Sonify remembers your choice.
- Next to it, the account button: **Sign in**, or your email's first letter once you're signed in (hover over it to see your credits).
- In the middle, the studios. Click one, press **Ctrl+1** to **Ctrl+9**, or **Ctrl+Tab** and **Ctrl+Shift+Tab** for the next and previous one.
- On the right, the Fanan and Sonify logos.

### Transport

The transport sits at the bottom of every studio, so the song plays and records wherever you are. It holds the **MIDI** light (lit when a MIDI keyboard plays), the **Performance** meter, the audio device button (click it for **Audio Setup**), the time display (click it for bars or seconds), **Return to zero**, **Stop**, **Play**, **Record** and **Loop**, the **L** and **R** loop locators (click one to type a position, like 5.3), **Timing**, **Tempo** with **Tap**, the **Metronome** and its volume, the **Main** meter, the buttons for the **Editor**, **Audio Editor**, **Console** and **Browser**, and **AI Assistant** (in a narrow window, its sparkle alone).

### In the Studio

Only the Studio has the menus (**File**, **Edit**, **Song**, **Track**, **Event**, **Transport**, **View**). Under them come a toolbar, the tracks and arrangement, and the Browser on the right (**Instruments**, **Effects**, **Files** and **MIDI**); the Editor (**F2**) and the Console (**F3**) open below.

> Tip: Hover over any button to see what it does and its key.

## Songs

### The Start Page

After Sonify starts, the Start page greets you with **New...** and **Open...**, your **Recent Files** (with a **Search** box: click one to open it), and under them **Audio**: your audio device, sample rate and block size, with **Configure Audio Device...**. The round swatches by **Colour** pick the colour that leads Sonify's look (green, yellow, blue, purple, red or orange): buttons, selections, the loop and the Start page take it at once, and Sonify remembers it. Click its x, press **Esc** or pick a studio to go to the song. **File > Start Page** brings it back.

### Making and Saving Songs

1. Click **New...**, or choose **File > New Song...** (**Ctrl+N**).
2. Type a name, choose the folder with the ... button, and set the tempo and time signature.
3. Click **OK**. The song gets a folder of its own, is saved there at once, and starts with one instrument track.

- **File > Save** (**Ctrl+S**) saves the song; one never saved asks where to go.
- **File > Save As...** (**Ctrl+Alt+S** in the default keyboard mapping) saves it under a new name or place.
- **File > Open...** (**Ctrl+O**) opens a song; **File > Close Project** closes it and brings back the Start page.

With unsaved changes, New, Open, Close Project and quitting first ask **Save**, **Don't Save** or **Cancel**. A saved song's recordings go into the Media folder beside it.

### Where Things Go

New songs, recordings of unsaved songs, mixdowns and autosaves go into your user data folder: Documents\Sonify, unless you choose another in **File > Settings... > Locations > User Data**. While a song has unsaved changes, Autosave keeps a copy named "(Autosave)" in the Autosave folder there, every 5 minutes. It never replaces your song: open it with **File > Open...** if you need it.

### Other DAWs' Projects (DAWproject)

DAWproject is the open project format of Bitwig Studio, Studio One, Cubase, Cubasis, VST Live and n-Track (Reaper reads it through ProjectConverter).

**File > Import DAWproject...** opens one as a new song, in a folder of its own like **New Song** (your song is asked about first, as with **Open**). Its audio goes into the song's Media folder. In it come:
- the tempo and time signature, with their changes;
- instrument and audio tracks, FX channels with the sends to them, and group tracks as buses; each one's name, colour, volume, pan, mute and solo; Main's volume and mute;
- MIDI clips with their notes (looped clips played out), CC, pitch bend and channel pressure; audio clips with their part of the file and their fades;
- automation of volume, pan and the VST3 plugins' knobs;
- VST3 plugins you have in the Browser, with their settings.

A plugin that can't play here (not installed, a CLAP, AU or VST2 plugin, or the other DAW's own device) stays in the song as a placeholder: its name shows greyed and in italics in its Console strip's inserts, and it keeps its settings. Saving keeps it; exporting puts it back where it was. When the song opens, a message lists what didn't come in as it was (time-stretched audio plays at its own speed, for one; markers and the clip launcher's scenes are left out).

**File > Export DAWproject...** writes the song as one file: tracks, buses (as group tracks), FX channels and Main, VST3 plugins with their settings, the placeholders an import kept, the clips with their audio inside the file, the tempo and time signature changes and the automation. Sonify's own instruments and effects (and WCLAP plugins), which no other DAW plays, aren't in it: the message when it's done lists them, and so do the tracks' comments in the file. Audio clips' own volume, Silence Areas and clip effects aren't in it either. A file holds up to 4 GB of audio.

## Audio and MIDI

### Choosing the Audio Device

1. Open **File > Settings... > Audio Setup** (or click the transport's audio device button).
2. Choose the **Audio driver**, then the **Device**. An ASIO driver is one device both ways, and **Control Panel** opens the driver's own window; Windows Audio has an **Output device** and an **Input device**.
3. Choose the **Sample rate** and **Block size**; the delay in and out shows beside it (a smaller block, less delay).
4. Tick the **Input channels** you record from.
5. Click **Test** to hear a test tone on Main's outputs.

Changes apply at once. **Release audio device in background** lets other apps use the device while Sonify is in the background and not playing, recording or exporting.

### Song Inputs and Outputs

**Song > Song Setup...** (or **Audio I/O Setup...** in Audio Setup) holds the song's **Record Format** (16 Bit, 24 Bit or 32 Bit Float) and, under **Audio I/O Setup**, its **Inputs** (the mono and stereo inputs audio tracks record from: **Add (Mono)** or **Add (Stereo)**, then click a channel to place it) and **Outputs** (Main, and Sub outputs such as a headphone mix; **Audition** is where the preview and the metronome play). **Make Default** keeps them for new songs. Nothing changes until **OK**.

### MIDI Keyboards

Sonify listens to every MIDI input it finds, as soon as it appears. **File > Settings... > MIDI Setup** lists them under **MIDI Inputs** (untick one to ignore it); add your keyboards under **External Devices** to give them names, channels and filters (see the Settings and More tab). An instrument track picks what it hears with the MIDI in button on its header.

MIDI is heard only by the studio in front, so nothing plays behind it:
- The **Studio**, **Analog Board**, **Song Builder Blocks** and **Live Show Studio** share the song: the tracks hear your keyboards as they choose.
- The **Plugins Creator Studio** plays the plugin being made, from the inputs ticked in its **MIDI In** list (at the top right).
- The **Sampling Studio** plays its nodes through the **MIDI In** node, from the inputs ticked in its list.
- **AI Tools**, the **Web Browser** and the **Guide** hear no MIDI.

A note you hold as you switch studios still lets go where it was playing. Control surfaces and MIDI Link work in every studio.

## Plugins

### At Startup

Each time Sonify starts, it asks **Load your third-party plugins?**

- **Yes, load my third-party plugins** checks your new and changed VST3, CLAP and WCLAP plugins on the **Checking your plugins** page. **Skip** goes on at once; the rest are read at the next start.
- **No, I'll use only Sonify's internal plugins** skips the check: only Sonify's own plugins are offered until you restart and choose Yes.

Each plugin is read by a separate helper, so a faulty one can't take Sonify down. Failures are listed under **Couldn't be read**; click **Continue** to go on.

### Plugin Folders

Sonify finds plugins in its Plugin Folders and their sub-folders; the system's VST3 and CLAP folders are added the first time. **File > Plugin Folders...** (or the Browser's folder button) lets you **Add Folder...**, **Add the Usual Folders**, **Change...** or remove one (x) and **Rescan All**. **File > Rescan Plugins** reads them again. **File > Add WCLAP Plugins to Browser...** (or + in the Browser) adds single files.

For your safety, a VST3 or CLAP plugin only loads from your Plugin Folders, the files added by hand, or a file you pick yourself. A song, a container or a sampling preset from someone else that names a plugin anywhere else (a network drive, say) doesn't load it, and says so: if you trust it, add it, then open the song again.

### What Sonify Hosts

- VST3 plugins (their windows open separately), CLAP plugins and WCLAP plugins (their windows float inside Sonify). Plugins with no window get Sonify's own controls.
- Your plugins made in the Plugins Creator Studio, marked **Made** in the Browser.
- Sonify's built-ins, always there: **Cayman** (new instrument tracks start with it), **Sonify EQ**, **Sonify Compressor** (with a sidechain input), **Sonify Limiter**, **Sonify Mastering** (set up by **Song > Mastering Assistant...**), Grex's **Sonify Chorus**, **Sonify Phaser**, **Sonify Auto-Wah**, **Sonify Reverb**, **Sonify Delay**, **Sonify Sweetener** and **Sonify Ducker**, and the system tools **Node Rack** and **Node Rack FX** (several plugins wired together in one slot), **Playlist Player** (plays songs and videos live), **Sonify Sampler** (plays SFZ instruments, like the Sampling Studio's) and **Tomahawk** (Grex's CLASSIC and ORIENTAL sounds, played as Grex plays them).

Drag a plugin from the Browser onto a track, or below the tracks for a new track. Double-click an instrument for a new track, or an effect to add it to the selected channel's inserts.

## Your First Song

1. On the Start page, click **New...**, name the song and click **OK**. It starts with an instrument track playing Cayman.
2. For another sound, drag an instrument from the Browser's **Instruments** tab onto the track. The keyboard icon on the track header opens its window.
3. To play it from your MIDI keyboard, the track's **Monitor** button must be on. Loading the instrument turned it on (the first instrument a track gets does); otherwise click it.
4. To record, click the track's **Record arm** button, turn on the **Metronome** (**C**) if you like, and click **Record**: Sonify plays and records. Press **Space** to stop.
5. To draw notes instead, pick the **Paint** tool (**5**) and drag in the track's lane to draw a clip. Double-click the clip to open the Editor, pick its **Paint** tool (**5**) and click to draw notes.
6. Press **Home**, then **Space** to play it.
7. Press **Ctrl+S** to save.

Add tracks with **Track > Add Instrument Track** or **Track > Add Audio Track**.

> Tip: Or ask Fanan AI: "Add a synth on a new track and write an 8-bar chord progression in A minor". **Edit > Undo** takes back everything it did.

## Fanan Account

Your Fanan account holds your Fanan AI credits: the same account and credits in every Fanan app, on every computer signed in to it.

### Signing In

1. Click **Sign in** in the top bar.
2. Type your email and click **Sign in** (or press **Enter**).
3. Your web browser opens to finish signing in, or to make your account. No password is typed into Sonify.
4. Sonify signs in by itself once you're done (**Open the browser again** reopens the page; **Cancel** stops waiting).

This computer stays signed in until you click **Sign out** in the account window.

### Credits

The account window shows your credits and **Buy credits**, which opens Fanan's credits page in your browser; new credits show within seconds of paying. The AI Assistant shows them by **Send**, with **Buy credits** too. Credits pay for Fanan AI requests, **Build with AI** in the Plugins Creator Studio, **Ask Fanan AI** in the Sampling Studio's Loop Editor, and the Guide's **Ask anything about Sonify** panel.

### What Needs the Internet

Signing in, buying credits, Fanan AI and the Web Browser need the internet, with the top bar's switch on **Internet on**; the first **Split into Stems** downloads what it needs, once. When there's no internet or Fanan's server doesn't answer, click **Try again** in the account window. Recording, editing and mixing work offline.

## Getting Help

The **Guide** studio (**Ctrl+9**) holds this guide: a tab for each part, from Getting Started to Settings and More, and a button for every chapter. **Search the guide**, at the top, finds the chapters that have the words you type; the chapter you read last opens again next time.

The **Ask anything about Sonify** panel, on the right, answers your questions about using Sonify, with Fanan AI: type a question and press **Enter** or click **Ask**. It answers from this guide, in the language you write in, and its links open the chapters to read. It only answers: to have the work done in your song, ask the AI Assistant (the button at the lower right). Like Fanan AI, it needs you signed in with the internet on, and each question uses credits; **New chat** starts afresh.

**Translate this page to**, above the chapter's text, shows the open chapter in another language: pick the language in the list (the languages Android offers) and click **Translate**. Fanan AI translates that chapter only, keeping the names of Sonify's buttons and menus in English so you can find them on screen, and it uses a few credits. A chapter you have translated shows again, in that language, whenever you open it, without using credits. **Show original** brings the English back, and **Save to PDF** prints the translated page to a PDF document.

- Hover over a button to see its tip and key; the menus show the keys too.
- The AI Assistant answers questions as well as doing the work.
- For Fanan's support, the account window shows "This computer:" with a code to give them.

### New versions

When a new Sonify is out, a small window floats over the lower right of any studio a few seconds after Sonify starts (Sonify looks once a day while it stays open, and only with the internet on). It names the new version and what's new; drag it by its top.
- **Get it** opens Sonify's page on fananteam.site, where you download the new version.
- **Later** (or ✕) closes it until Sonify starts again.
- **Skip this version** keeps it quiet until a later version comes out.

An **Important update** (a security fix, say) has an orange edge and no Skip: it shows each time Sonify starts until you update.

# Studio

## Overview

The Studio is Sonify's DAW, where you record, arrange, edit and mix a song. Open it with the **Studio** tab in the top bar, or **Ctrl+1**.

### The parts

- Menu bar and toolbar (tools, **Automation View**, the **Quantize** panel button and grid, **Timebase**, **Snap**, **Autoscroll**, **MIDI Link**), in the middle of its line.
- Track list (left) and arrangement (middle): a header and a lane for each track.
- Browser (right): instruments, effects, files and MIDI tools.
- Bottom panel: the Editor, the Audio Editor or the Console, sharing one space.
- Transport bar (bottom): play, record, loop, tempo, metronome and the panel buttons.
- Above the tracks, when shown: the Master Song Channel and the Video lane.

### Showing and hiding panels

Use the buttons at the right of the transport bar, the **View** menu, or **F2** (Editor), **F3** (Console) and **F5** (Browser). The Audio Editor has a button next to the Editor's. The last button, **AI Assistant**, opens Fanan AI in a window over the studio (see the AI Assistant tab).

### Keyboard shortcuts

This guide gives the default keys (Studio One's). For another DAW's keys, choose it in **File > Settings...**, tab **Keyboard Shortcuts**; menus and button tips show the keys in use. Clip keys work while the arrangement has focus, note keys while the Editor has: click in the area first.

### MIDI Link

Move a knob or pad on a MIDI controller and it shows as a chip in the MIDI Link strip. Drag the chip onto a control, or click **Learn**, click a control and move what should move it. Click **MIDI Link** for its window: the links, the **Plugin knob** slot and **Focus** (controls that follow the selected track). See MIDI Link in the Settings and More tab.

## Tracks

### Track types

- Instrument tracks play MIDI clips and live notes through an instrument (the built-in **Cayman** until you choose another).
- Audio tracks play audio clips, and can monitor and record an input.
- Choir tracks are audio tracks whose vocal also sings as a choir (see Choir tracks).
- Bus channels and FX channels have no clips. They live in the Console.

### Adding tracks

- Click **+** at the top of the track list and choose **Instrument Track**, **Audio Track** or **Session Players...**.
- The **Track** menu has **Add Tracks...** (**T**), **Add Instrument Track**, **Add Audio Track**, **Add Bus Channel**, **Add Bus for Selected Channels**, **Duplicate Selected Tracks** and **Remove Selected Tracks**.
- Right-click the empty space under the tracks for the track menu (below).
- Drag an instrument from the Browser into the empty space under the tracks for a new track, or onto a track to replace its instrument.

### Add Tracks

**Add Tracks...** (**T**) adds several tracks at once, as one Undo step, and selects them.

1. Type a **Name** (several tracks get numbers after it), or leave it empty for the usual names.
2. Set the **Count** (up to 64) and the **Type**: **Instrument**, **Audio**, **Choir**, **Bus Channel** or **FX Channel**.
3. For audio and choir tracks, pick the **Format** (**Mono** or **Stereo**) and the **Input**; tick **Ascending** to give each next track the next input.
4. For instrument tracks, pick the **Instrument** (**Cayman** or any instrument in the Browser).
5. Click **OK** (or press **Enter**).

### Selecting and moving

Click a header to select a track; **Ctrl**+click adds or removes one, **Shift**+click adds a range. Selected tracks act as one: Mute, Solo, Record and Monitor switch them all, and their faders and pans move together. Drag a header by its number, name or colour strip to move tracks.

### The track header

- **M** mutes, **S** solos.
- The round button arms the track for recording; its lane turns red.
- The speaker is **Monitor**: an instrument track plays from your MIDI keyboard, an audio track lets you hear its input. New tracks start with it off. The first instrument plugin loaded on an instrument track turns it on, once; after that it stays as you set it.
- Double-click the name to rename. An instrument track takes its instrument's name until you name it.
- The keyboard icon opens the instrument's window.
- Double-click **Vol** or **Pan** to reset it. Double-click its value (such as **-3.5 dB** or **25L**) to type one: volume in dB (-72 to +10), pan from -100 (left) to 100 (right), up to two decimals. **Enter** sets it, **Esc** keeps the old one.
- Instrument tracks: the instrument selector, and the MIDI in button to tick the MIDI devices and tracks it hears. Its list shows keyboards first, then controllers, then virtual MIDI cables (such as loopMIDI). New tracks hear the default device (**File > Settings...**, tab **MIDI Setup**).
- Audio tracks: the input selector, and the Mono/Stereo button (one circle or two). Mono records the input as one sound; Stereo puts each input channel on its own side.

### Right-click menu

Right-click a header for **Cut Track**, **Copy Track**, **Paste Track** (the tracks you cut or copied, under the one you right-clicked), **Remove Track**, the two duplicates, **Color...**, **Rename...** (several tracks get numbered names, like "Vocals 1", "Vocals 2"), **Select All Tracks** and **Add Bus for Selected Channels**. They act on the whole selection. The duplicates:
- Instrument tracks: **Duplicate Track** copies the notes and settings without the instrument (Cayman plays it until you pick one); **Duplicate Track + Instrument** copies everything.
- Audio tracks: **Duplicate Track** makes a new track with the same settings and effects, without the clips; **Duplicate Track Fully** copies the clips too.

Select several tracks with **Shift+click** (every track from the last one clicked) or **Ctrl+click** (one more, or one less). Dragging the bottom edge of a selected track sizes every selected track the same; double-click the edge to put them all back. An instrument track's menu also has **Show as Score...** (see Score); an audio track's has **Load Media...** (see Importing audio files). The track menu follows them; right-click the empty space under the tracks for the track menu alone:

- **Add Tracks...** (**T**): the Add Tracks window (above).
- **Add Audio Track (mono)**, **Add Audio Track (stereo)**, **Add Instrument Track** and **Add Choir Track**: one new track.
- **Add Tracks for all Inputs**: an audio track for each of the song's inputs (**Song > Song Setup...**, tab **Audio I/O Setup**), named after it, mono or stereo as the input is.
- **Collapse All Tracks**: every track at the small size; **Expand All Tracks** puts them back as they were, heights of their own included.
- **Store Track Preset...**: the selected tracks, with their instruments, inserts and MIDI FX (and their settings), their name, colour, fader, pan and mono or stereo, kept in a file. Clips and envelopes stay in the song.
- **Load Track Preset...**: a preset's tracks added to the song, as one Undo step.

> Tip: Track presets go in the Track Presets folder of your user data folder, so they're there in every song.

### Choir tracks

A choir track turns a vocal into a choir: put a vocal on it (record it, or drag in an audio file), and its voices sing it as the song plays, in time with it. Add one with **Add Choir Track** (right-click the empty space under the tracks) or **Add Tracks...** with the **Type** **Choir**. It starts with four voices: two women and two men.

Click the singers icon on its header for the **Choir** window (drag it by its title to move it):

- **Lead voice**: whether the vocal is **A man**'s or **A woman**'s. Women's voices are made from a man's by raising their resonances, and the other way round.
- **Chords from**: an instrument track whose notes give the chords, for voices on **Chord** parts.
- **Lead**: the vocal itself (all the way down: **Off**, the choir alone). **Choir**: the voices together. **Spread**: how wide they sit. **Hall**: their room.
- Humanize: **Timing** (each singer a little late or early), **Pitch** (a little off the note, drifting) and **Vibrato** (one of each singer's own).
- A row for each voice, up to 16:
  - **Man** or **Woman**. Switching also moves the voice an octave, so a woman's voice sits an octave above a man's lead.
  - **Character**: darker to the left, brighter to the right.
  - **Part**:
    - **Scale**: steps of the song's key from the note sung, such as a 3rd above.
    - **Chord**: chord notes from it. With no chord track, it sings thirds of the key.
    - **Semitones**: a fixed interval.
  - **Octave**.
  - **Level** and **Pan**: double-click either to reset it.
  - **M** and **S**: mute or solo the voice.
  - **x**: remove the voice.
- **+ Add Voice** adds one more.
- **Presets**: a whole choir, laid out around the lead voice (**Double Track**, **Thirds**, **Women's Trio**, **Mixed Choir (SATB)**, **Gospel**, **Big Choir** and more). **Save Preset...** keeps the choir as one of yours on this computer. Pick one of yours to load it, or **Delete** to forget it.

Changes are heard at once and saved with the song. They aren't Undo steps, as a plugin's knobs aren't. The key comes from the Master Song Channel. The voices follow the note sung, so a clean, single vocal works best.

While you monitor or record its input, you hear your voice alone: the choir sings the clips. Its inserts, fader and freezing work as on any audio track.

### Track size

Choose **Small**, **Normal** or **Large** under **Track size** at the bottom of the track list. Drag a track's bottom edge for a height of its own; double-click the edge to go back.

### Freeze

Freezing renders a track's sound and plays that instead, so its plugins rest. Click the snowflake: it fills as it renders (click again to stop). A frozen track's clips, arm, monitor, instrument and input are locked, but its fader, pan, mute and solo work. Click the snowflake to unfreeze. A change to what it plays (its clips, plugins or their settings, the tempo or key) unfreezes it by itself. Freezing is not an Undo step.

## Arrangement

### Moving around

- Click or drag in the ruler to move the song position.
- Zoom with **Ctrl**+mouse wheel, the slider under the lanes, **E** and **W**, or **Ctrl+=** and **Ctrl+-**. **Ctrl+0** is the default zoom, **Alt+Z** shows the whole song, and **View > Zoom to Loop** (or the button left of the slider) fits the loop.
- Zoom the tracks' height with **Ctrl+Shift**+mouse wheel, the height slider under the lanes (left of the length slider) with its **-** and **+**, or **Shift+E** and **Shift+W**. It makes every track taller or shorter at once, its own height and the **Track size** alike; short tracks show less in their header. Double-click the slider, or **View > Default Track Heights**, for 100%. The song keeps it.
- **Autoscroll** keeps the playhead in view while playing. Scroll away by hand while it plays and the view stays where you put it, until the playhead comes into view again or the song starts again (the wrench's **Scrolling pauses Autoscroll**). **Timebase** makes the ruler count Bars or Seconds.

### Second ruler

The ruler button at the top of the track list (beside the Master Song Channel's) shows a second ruler under the ruler, in a timebase of its own: Seconds, Samples, Bars or Frames. Right-click it to change its timebase.

### Track list options

The wrench at the top left of the track list opens Studio One's options menu. It stays open while you switch them; Sonify remembers them.

- **Selection**: **Solo follows Selection** (the selected tracks are the soloed ones), **Instrument Input follows Selection** (the instrument track you select hears your MIDI devices, the others stop unless armed), **Audio Input follows Selection** (the same for audio tracks' inputs).
- **Editing**: **Locate when clicked in empty space**, **Cursor follows Edit Position** (a clip clicked or moved takes the song position to its start), **Automation follows events** (a clip moved takes its track's automation under it along; a copy copies it), **No overlap when editing events** (a clip moved, resized or pasted over others trims them).
- **Visibility**: **Colorize track controls**, **Show channel numbers in tracks**, **Show event names**.
- **Audio**: **Monitoring follows record** (arming a track monitors it).
- **Autoscroll**: Autoscroll itself, and **Scrolling pauses Autoscroll**.

### Grid and snap

**Quantize** sets the grid (1/1 to 1/64). **Snap** picks what dragged things land on: **Adaptive** (the finest grid line on screen), **Quantize** or **Bar**. The magnet before it switches snapping off and on.

### Quantize panel

The button left of **Quantize** in the toolbar opens the Quantize panel. It stays open while you pick notes or clips; its top line says what it will work on.

- **Grid**: **Follow the grid** (the Editor's Quantize in the Editor, the toolbar's for clips), or its own: 1/1 to 1/64, or the triplets 1/4 to 1/32.
- **Strength**: how far notes move toward the grid (100%: right onto it).
- **Swing**: moves every second grid position later, up to a triplet shuffle at 100%.
- **Move**: **Starts** and **Ends** (Ends puts each note's end on the grid too, tidying lengths).
- **Quantize** (or **Q**): the selected notes in the Editor (or all of them), or the notes in the selected MIDI clips. One Undo step.
- **Record Quantize**: notes you record go onto the grid as each take ends, with these settings. The toolbar button shows a red dot while it's on.

Double-click a slider to reset it. The settings stay on this computer.

### Tools

Pick a tool with the toolbar, its key, or the right-click menu.

- **Arrow** (**1**): select, move and resize clips.
- **Range** (**2**): drag over clips for a stretch of time, across several tracks if you like; it shows green. Double-click inside it to cut every clip there at the range's start and end; the parts inside are selected. A click outside it lets it go.
- **Split** (**3**): click where to cut a clip in two.
- **Eraser** (**4**): click or drag over clips to delete them.
- **Paint** (**5**): drag in an empty lane to draw an empty MIDI clip; a click draws one bar. In Automation view it draws the envelope, in the shape picked under **Paint Tool** in the right-click menu (below).
- **Mute** (**6**): click a clip, MIDI or audio, to mute it (it stays in the song, pale and silent), or click again to unmute it; drag over clips to do the same to each. Click a selected clip to mute or unmute the whole selection. Each is one Undo step, and the song keeps it.
- **Listen** (**8**): hold the mouse down on a track to hear it alone from that point; let go and the song stops and goes back where it was.

### The right-click menu

Right-click the empty space of the lanes (or anywhere in Automation view) for Studio One's menu: the tools, the current one ticked, each with its key; **Cut**, **Copy**, **Paste** (on the track and at the place you clicked), **Delete**, **Load Media...** (audio files from the place you clicked, see Importing audio files), **Select All**, **Deselect All**, and **Undo** and **Redo** with the step they take back or redo. Point at **Paint Tool** for its shapes: **Freehand**, **Line**, **Parabola**, **Square**, **Triangle**, **Saw** and **Sine**. A clip's own right-click menu stays as it was.

### Making clips

Draw one with Paint, record, paste, or bring in audio files (below). You can also drag audio files from the Browser's Files tab onto a lane.

### Importing audio files

Drag audio files straight from a folder on your computer (Explorer or Finder) into the arrangement, one file or many at once. Hold them over the lanes a moment: a green line shows the bar they'd start at (on the grid, as Snap is set), the track they'd go on is lit (or a line shows where new tracks go in), and a note says what happens. Then let go.

- On an audio track: the first file goes on it, each other file on a new audio track under it.
- Anywhere else (an instrument track, or below the tracks): each file on a new audio track, named after it.
- Hold **Shift** as you let go: all the files on one track, one after another.
- A folder brings its audio files, in name order. A video becomes the song's video.

Or choose the files with Sonify's file window, the same way:

- Right-click an audio track's header > **Load Media...**: from the song position, onto that track.
- Right-click the empty space of the lanes > **Load Media...**: from where you clicked.
- **File > Import Media Files...**: at the song position, onto the selected audio track.

WAV, AIFF, FLAC, MP3 and Ogg Vorbis go in as they are (and WMA on Windows). M4A, AAC, Opus, ALAC, WMA, AC-3, WavPack, APE and other kinds are first copied into WAV files in the song's Media folder (Sonify says which while it's at it); your original files stay as they were. All of it is one Undo step.

### Editing clips

- Click a clip to select it, **Shift**+click or **Ctrl**+click to add or remove, or drag a box over empty space. **Ctrl+A** selects all, **Ctrl+D** (**Edit > Deselect All**) deselects all.
- Drag clips to move them, onto other tracks too; hold **Alt** to copy.
- Drag a clip's edge to resize it. Trimming hides notes rather than deleting them.
- In the **Edit** menu: **Cut** (**Ctrl+X**), **Copy** (**Ctrl+C**), **Paste at Playhead** (**Ctrl+V**, from the selected track down), **Duplicate** (**D**, right after the selection) and **Delete** (**Delete**).
- **Split at Playhead** (**Alt+X**) cuts the selected clips at the playhead, or the selected track's clip when none is selected.
- Double-click a MIDI clip for the Editor, an audio clip for the Audio Editor.
- On an audio clip, drag the squares at its top corners for fade in and fade out, and the bar at its top middle for its volume (**Shift** for fine steps).
- Right-click a clip for **Cut**, **Copy**, **Paste** (at the playhead, on the selected track), **Delete** and **Duplicate**, then the clip's own: the Audio menu for an audio clip; **Make Block from Selected Clips**, **Go to Song Builder Blocks** and the rest for a MIDI clip.

### Loop range

The thin strip at the top of the ruler is the loop lane. Drag in it to draw a loop, drag its ends or middle to change it, click it to switch looping on or off (bright green is on, dark green off).

### Automation

Press **A** or click **Automation View**. Each track shows an envelope over its clips for the target picked in its header: Volume, Pan or a parameter of its plugins (a dot marks those with an envelope).

The automation button on each track's header (the envelope icon beside the snowflake; its letter is the mode) opens the track's automation menu: its mode (**Off**, **Read**, **Touch**, **Latch**, **Write**), the envelope to show (choosing one turns Automation View on), and **Automation View** itself.

- Arrow: click to add a point, drag points (**Alt** for free placement).
- Paint: draw the envelope. With a shape picked (right-click > **Paint Tool**), drag from where it should start: a Line or Parabola runs to the pointer, and a wave swings between the two heights, four Quantize steps a cycle.
- Eraser, double-click or **Delete**: remove a point.
- The header's **R** (Read) makes the envelope move the control while playing; click it for **Off**.

### Automation Shape Regions

A shape region is a repeating shape over a stretch of an envelope, kept so you can change it again. Right-click the lane where there's none and choose **Add Shape Region**: two bars of a Sine from there (cut short at the next region).
- Right-click it for **Shape** (**Sine**, **Triangle**, **Saw Up**, **Saw Down**, **Square**, **Random**), **Cycle** (**1/32** to **4 bars**), **Edit Shape...** (its shape, cycle, **Low %**, **High %** and **Phase %**; double-clicking the strip opens it too), **Keep as Points** (the region goes, its points stay) and **Remove Shape**.
- With the Arrow, drag the blue strip at the top of the region to move it, or its ends to make it longer or shorter; it snaps as the arrangement does.
- Its points are what plays, drawn as the line. Each change is one Undo step, and the regions are saved with the song.

## Recording

### Getting ready

- Instrument track: check its MIDI in button shows your keyboard.
- Audio track: pick its input and Mono or Stereo. Inputs are set up in **Song > Song Setup...**, tab **Audio I/O Setup**; tab **General** has the **Record Format** (16 Bit, 24 Bit or 32 Bit Float).
- An armed audio track that isn't monitoring shows its input on its meter, to set the level.

### Recording a take

1. Arm the tracks with their round button. With none armed, the selected track records.
2. For a click, switch on the metronome (**C**); the slider beside it sets its volume.
3. Press **Record** (or the * key on the number pad). A stopped song starts playing.
4. Play. A red clip grows on each armed track.
5. Press **Record** again to punch out while the song plays on, or **Space** to stop.

To punch in, press **Record** while the song plays.

### Precount, Preroll, Autopunch, Postroll

The four buttons right of the transport's Record button (the arrow beside them, or a right-click, sets their bars):

- **Precount**: recording from stopped, the metronome counts the bars in first; the time display counts down.
- **Preroll**: recording from stopped, the song plays the bars before where it stands; recording starts where it stood.
- **Autopunch**: records only between the loop's start and end (L and R), shown red on the ruler.
- **Postroll**: with Autopunch, the song plays the bars past the punch out, then stops.

They're kept with the song.

### What you get

- MIDI goes into a new clip on each armed instrument track, with pitch bend and aftertouch.
- Audio becomes a new clip, its file in the song's Media folder (an unsaved song records into the Recordings folder, see **File > Settings...**, tab **Locations**).
- With the loop on, MIDI adds each time round into the same clip; audio keeps the last time round.
- With **Record Quantize** on (the Quantize panel), the recorded notes go onto the grid.
- A take that caught nothing leaves nothing. Each recording is one Undo step.

## Editor

The Editor is the piano roll for one MIDI clip. Double-click a MIDI clip, or select one and press **F2**. Close it with its **x** button or **F2**.

### Tools

Pick a tool with the toolbar, its key, or the right-click menu.

- **Arrow** (**1**): click a note to select it (**Shift** or **Ctrl** adds or removes), drag over empty space for a box, drag notes to move them, drag a note's right edge to change its length. Double-click empty space to draw a note.
- **Range** (**2**): drag across a stretch of time; every note that starts in it is selected, whatever its pitch (**Shift** adds to the selection). Drag a selected note to move them all.
- **Split** (**3**): click a note to cut it in two there, on the grid while Snap is on; a line shows where. Click a selected note to cut every selected note at that time.
- **Eraser** (**4**): click or drag over notes to delete them.
- **Paint** (**5**): click to draw a note, drag to set its length. In the lane under the notes it draws its shape (below).
- **Mute** (**6**): click a note to mute it, or unmute it; drag over notes to do the same to each. A muted note stays in the clip, drawn hollow, but doesn't play. Click a selected note to mute or unmute the whole selection.
- **Listen** (**8**): hold the mouse down to hear the clip's track alone from that point; let go and the song stops and goes back where it was.

### Paint shapes

Right-click, point at **Paint Tool** and pick its shape for the lane under the notes (Velocity, Pitch Bend or Aftertouch): **Freehand**, **Line**, **Parabola**, **Square**, **Triangle**, **Saw** or **Sine**. Drag in the lane: the shape runs from where you pressed to where the pointer is, and the waves swing between those two values, four Quantize steps a cycle (a beat with Quantize at 1/16). Picking a shape picks the Paint tool.

### The toolbar

- **Quantize**: the grid, a new note's length and the steps notes move by. The magnet switches Snap.
- **Velocity**: the velocity of new notes (1 to 127).
- The speaker: hear notes as you select and move them.
- **Quantize** button (**Q**): moves the selected notes, or all, onto the grid, with the Quantize panel's strength, swing and ends.
- **Tools...**: MIDI Tools.
- **Operators**: shows or hides the Operators row (below).

### Operators

Operators (as in Bitwig) decide what each note does every time the music comes round, so a loop keeps changing instead of repeating. Click **Operators** in the toolbar, select notes, and set them in the row:

- **Chance**: how likely each note plays (100% always). It's rolled again on every pass.
- **Vel Spread**: how far each note's velocity may move up or down, rolled again on every pass.
- **Repeats** (ratchets): the note plays several times through its length, evenly (**x2** to **x32**) or every note value (**1/4** to **1/64**, triplets too). **Curve** crowds the repeats towards the note's start (below 0) or its end; **Vel End** takes the last repeat's velocity towards silence (below 0) or the top.
- **Occurrence**: **First pass** or **Not first pass**; **With previous note** or **Without previous note** (the note before it in the clip played, or didn't); **With previous on its key** or **Without previous on its key**.
- **Recurrence**: a cycle of 2 to 8 passes; click the numbered buttons for the passes the note plays on (the 4th of every 4, say).
- **Seed**: what the clip's Chance and Vel Spread roll from. A pass sounds the same each time it plays, in a mixdown too; the die rolls a new seed.

A pass is each time the loop comes round over the clip, and copy by copy along copies of the clip placed back to back (a pattern repeated): with 8 copies of a one-bar clip, Recurrence's 4th of 4 plays in the 4th and the 8th. Playing from the middle counts the same way, so the song always sounds as it will export.

A note shows its operators: a die's dots for Chance (one to five), a line at each repeat, a corner for Occurrence, and a little square for each pass of Recurrence (filled where it plays). Vel Spread shows in the Velocity lane as a faint range behind the bar. Different values among the selected notes show as a dash; setting one sets it for them all.

### Keys and menus

- **Delete** removes the selected notes, **Ctrl+A** selects all, **Ctrl+D** deselects all, **Ctrl+C**, **Ctrl+X** and **Ctrl+V** copy, cut and paste, **D** duplicates.
- **Up** and **Down** transpose (with **Shift**, an octave); **Left** and **Right** move notes by the Quantize step.
- Right-click anywhere in the notes for the menu: the tools (the current one ticked), **Cut**, **Copy**, **Paste** (where you clicked), **Duplicate**, **Delete**, **Select All**, **Deselect All**, **Undo** and **Redo**, each with its key. A note you right-click joins the selection first.
- Paste goes under the mouse, else at the playhead if it's in the clip, else at the clip's start.
- Notes moved past the clip's edges grow it to whole bars.

### The lane under the notes

Pick **Velocity**, **Pitch Bend**, **Aftertouch** or **Chance**. For velocity and chance, drag over the notes' bars (only the selected notes, when some are). For Pitch Bend and Aftertouch, drag to draw the line; with the Eraser, drag to clear it back to rest.

### Zoom and keys

The sliders right of the lane tabs zoom length and height; **Ctrl**+mouse wheel zooms the length too. Click the keys on the left to hear a note, or drag along them for a glissando.

### Drums view

The switch at the left of the toolbar picks **Piano** (a key for each row) or **Drums** (a drum sound for each row). The clip stays as it is; Sonify remembers the view for each track while it runs.

- The rows are the usual kit (Kick, Snare, Clap, Closed Hat, Open Hat, the toms, Crash, Ride), every note the clip plays, and the rows you add. Each shows its General MIDI name and its note (C2 is the Kick). A row lights while its sound plays; click a name to hear it.
- Arrow tool: click an empty step to put a hit there (on the Quantize grid, with the toolbar's Velocity), click a hit to take it away. **Shift** or **Ctrl**+click selects, drag a hit to move it, drag over empty space for a box. **Up** and **Down** move the selected hits a row.
- Paint tool: click or drag along a row for a roll, a hit on each Quantize step.
- The other tools and the lane under the notes work as in the Piano view.
- **+ Sound** (above the names) adds a General MIDI drum sound as a row. Double-click a row's name to rename it (an empty name brings back its usual one). Right-click a name for **Rename**, **Back to its usual name**, **Take this row away** (a row added with + and empty) and the sounds to add.
- The names and added rows belong to the track and are saved with the song.

### Process

**Process** in the toolbar works on the selected notes, or on all of them when none are selected. Each item is one Undo step.
- **Chop**: each note cut **In 2**, **In 3**, **In 4** or **On the Quantize Grid**.
- **Glue**: notes of the same key that touch or overlap become one.
- **Legato**: each note up to the next note's start.
- **Reverse**: backwards in time. **Invert**: upside down, the highest note becomes the lowest.
- **Double Speed** and **Half Speed**: notes past the clip's end grow it to whole bars.
- **Fit to Key**: each note off the song's key moves to its nearest note in the key (key changes are followed).
- **Velocity**: **Crescendo**, **Decrescendo**, **Set to** (the toolbar's Velocity) or **Compress** (halfway to their average).
- **Remove Doubles**: of notes on one key starting together, the longest stays.

Invert and Fit to Key are greyed in the Drums view.

### Scale and Fold

**Scale** lights the song's key (where the clip starts) across the rows, the root brightest. The menu next to it folds the rows: **All keys**, **Fold to key** (only the key's rows, and any note off it the clip plays) or **Fold to notes** (only the rows the clip plays). While folded, dragging a note moves it row by row through the rows shown; **Up** and **Down** still move a semitone. Both only change what you see, and neither shows in the Drums view.

## Audio Editor

The Audio Editor shows one audio clip big, on the song's bars. Double-click an audio clip, or click the Audio Editor button in the transport bar to see the selected one.

- **Clip tempo**: the clip's own tempo; type it or click **Detect**.
- **Sync Tempo** makes a stretched copy whose beats land on the song's; the Follow Tempo button keeps it stretched to the song's tempo as it changes.
- **Pitch**: semitones up or down, keeping its length. **Volume**: the clip's own level.
- **Audio**: Reverse, Normalize, Audio to MIDI, Back to the Original.
- Click the waveform to move the song position; **Ctrl**+mouse wheel zooms.

The **Event** menu has the same commands. Each change is one Undo step.

**Stretcher**, **Markers** and **Slice** in the toolbar are three modes; turning one on turns the others off.

### Warp Markers

Click **Markers**, then double-click the waveform to put a marker. With **Snap to hit** on (it is at first), a marker goes to the nearest hit (a drum's attack) close by; off, it stays where you double-click.
- Drag a marker to move that moment in time: only what's between the markers either side of it stretches or squeezes (up to 8 times), nothing else moves. It snaps as the arrangement does, and shows how much each side stretches as you drag.
- Double-click or right-click a marker to take it away.
- A marker that moved audio is filled, belongs to the clip and is saved with the song; each move or removal is one Undo step. One only put down is hollow, and stays while Sonify runs.
- The clip's original is kept: Pitch and Reverse keep the markers, while **Sync Tempo**, Follow Tempo and **Back to the Original** start again from the original, without them. A clip that follows the song's tempo needs Follow off first.

### Slice to Sampler

Click **Slice** (or **Audio > Slice to Sampler...**, also on an audio clip's right-click menu). The clip shows its cuts:
- **Cut at**: **Hits** (with **Low**, **Medium** or **High sensitivity**), **Warp markers** or **The grid** (**Every bar**, **1/2**, **1/4**, **1/8**, **1/16**).
- Drag a cut to move it, double-click to add one, double-click or right-click a cut to take it away. Changing **Cut at** starts the cuts afresh.
- The count shows the slices and their keys, from C2 (note 36) up: 92 slices at most.

Click **Slice to Sampler** (**Cancel** or **Esc** leaves). Each slice becomes a key of a Sonify Sampler on a new track under the clip's, named after it plus "Slices", with a MIDI clip that plays them as the clip did. The slices (WAV files) and their SFZ go into Samples\Slices in your user data folder. The audio clip stays as it is. It's one Undo step (the files stay on disk).

## Score

The Score shows an instrument track's notes as sheet music, as Studio One does. It only shows them: notes are written and changed in the Editor, and the Score follows.

### Opening it

- Right-click an instrument track's header > **Show as Score...**
- Right-click a MIDI clip > **Show Track as Score...**
- **View > Score...** shows the selected track.

The whole track is shown, all its clips, from the bar of its first note to the bar of its last. Muted notes and muted clips are left out. The title is the song's name, with the track's name under it. The key, time signature and tempo come from the song (the Master Song Channel), and change where the song does.

### The controls

- **Track**: another instrument track.
- **Notation**: **Auto** picks for you: drums for a drum track, one staff in treble or bass clef, or two staves for a piano part that spans both. Or choose **Piano (two staves)**, **Treble clef**, **Bass clef** or **Drums**.
- **Split at**: with two staves, the notes from this one up go on the upper staff (C4, middle C, at first).
- **Quantize**: the grid the notes are read on, 1/4 to 1/32. Notes played by hand land on it, so they read cleanly. Your notes in the song don't change.
- **-** and **+** zoom the score.

Notes that start and end together make a chord; notes that overlap go on a second voice, stems up and down. Long notes are tied over beats and bar lines. On a drum staff the hands go stems up and the kick and hi-hat pedal stems down, with x noteheads for cymbals.

### Exporting

- **Export PDF...**: the score in A4 pages, to print or send.
- **Export MusicXML...**: a file MuseScore, Sibelius, Finale and Dorico open, to go on writing there.

Sonify asks where to save; it suggests the song's folder, or the Scores folder of your user data folder when the song isn't saved yet. Once saved, click **Show in folder**.

## Transport and Tempo

### The transport bar

- The MIDI light, the **Performance** meter, and the audio device button (click it for Audio Setup).
- The time display: click to switch between Bars and Seconds.
- Return to Zero, Back One Bar, Forward One Bar, Stop, Play, Record and Loop. Clicking Stop while stopped returns to zero. Return to Zero and the bar steps bring the clips' view along when the playhead leaves it. Back One Bar from inside a bar goes to its start, from a bar's start to the bar before.
- **L** and **R**, the loop range: click one to type a position like 5, 5.3 or 5.3.2.
- **Timing**, **Tempo**, **Tap**, the metronome and its volume, the Main meter and the panel buttons.

**Space** starts and stops, **Home** returns to zero, **Num -** and **Num +** step back and forward a bar, the * key on the number pad records, **Num /** switches the loop and **C** the metronome. They work in every studio. The **Transport** menu has them, plus **Autoscroll**.

### Tempo and time signature

Type a tempo (20 to 250 BPM) in **Tempo**, or click **Tap** in time. Set beats per bar and the beat unit under **Timing**. Both show and set the ones where the song is now.

### The Master Song Channel

For changes along the song, open the Master Song Channel (**View > Master Song Channel**, or the button at the top of the track list). It sits over the tracks, with one lane along the song.

The tabs over its header pick what the lane shows: **Signature**, **Key**, **Chords**, **Tempo** or **Volume**, one at a time with the whole height for it. Under the tabs, the header shows what holds where the song is now. Drag the channel's bottom edge to make it taller or shorter (a taller Tempo lane gives finer steps); double-click the edge for the usual height. The song keeps the tab and the height.

- **Signature**: double-click empty space to add a time signature at that bar, double-click one to change it, drag it to another bar. Every other stretch is shaded, so you can see where each one holds.
- **Key**: key and scale changes, the same way. MIDI tools follow them.
- **Chords**: the chord track, each chord holding until the next. Double-click to add a chord or change one (type it: Am7, C/E, F#m7b5, Bbmaj7), drag to move it (**Alt** for free placement), click to hear it on the selected track. Right-click for **Detect Chords from the Selected MIDI Clips** (also **Detect Chords** on a MIDI clip's right-click menu: a chord a beat at most), **Chord Pads...** and **Clear the Chord Track**. The Chord Pads play the eight chords of the key where the song is; with **Write** ticked, a pad also puts its chord there. The Score shows the chords over the music, and **Follow Chords** (MIDI FX) plays along with them.
- **Tempo**: the lane is a scale from 20 to 250 BPM, with lines to read it by. Click to add a change (its height is its tempo), drag changes up and down in whole BPM (**Alt** for free placement; hold **Shift** to move a tenth as far, in hundredths of a BPM), double-click one to type its tempo.
- For a ramp, double-click a change and tick **Glide to the next tempo**, or right-click > **Glide to the Next Tempo**.
- **Volume**: Main's volume along the song. Click to add points and drag them; Main's fader follows them while playing.
- Remove with **Delete**, the Eraser or right-click > **Remove**. The first time signature, key and tempo stay.

Each tab can have a key of its own: **File > Settings...**, tab **Keyboard Shortcuts**, under View.

Each change is one Undo step. Audio clips keep their speed when the tempo changes, unless they follow the song's tempo.

## Undo

- **Edit > Undo** (**Ctrl+Z**) and **Edit > Redo** (**Ctrl+Y** or **Ctrl+Shift+Z**). The menu names the step, like "Undo Move Clips". Both work in every studio.
- Undo covers editing the song: clips, notes, recordings, tracks added, removed, duplicated, moved or renamed, plugins and MIDI FX, tempo, time signature and key changes, automation, Song Builder Blocks and the video.
- Mixing (faders, pan, mute, solo, bypass), plugin settings and freezing are not Undo steps.
- Each Fanan AI request is one step: one Undo takes back everything it did.

## Console

The Console is the mixer: a strip for every track, bus and FX channel, one for each Sub output, and Main on the right. Open it with **View > Console**, the Console button at the right of the transport bar, or **F3** (in the default key set).

### Showing some channels
The buttons down the Console's left side choose which strips show: **Instrument tracks**, **Audio tracks**, **Buses** and **FX channels**. Click one to show only that kind; click it again for all of them. **Ctrl+click** adds a kind or takes it out, and the top button (**All**) shows every strip. Main and the Sub outputs always show. Sonify remembers the choice for next time.

### A channel strip
- The menu at the top picks where it plays: **Main**, a Sub output, or a bus.
- **+ Insert** adds an effect. Click an insert to open its window; click its light to bypass it.
- Drag the fader (**Shift** for fine moves), scroll over it, or click it and use the arrow keys. Double-click the fader for 0 dB, the pan slider for centre.
- The meter shows the level after the fader. The clip light means it went over 0 dB; click it to reset.
- Double-click the name to rename. Drag a strip by its name to move it.
- **Ø** beside the output menu inverts the channel's polarity (also on an audio track's header, and in the track's right-click menu as **Invert Polarity**).

Right-click the Console where there's no strip for **Add Bus Channel** and **Add FX Channel**. Main's arrow shows its inserts beside it. Drag the top edge of the Console (or the Editor, or the Audio Editor) to make it taller, up to the toolbar; double-click the edge for its usual height.

**Ctrl+click** and **Shift+click** select several strips; their buttons, faders and pans then move together, each fader and pan by as much as the one you move, from wherever it is (one at -4 dB moved to -1 takes another from -3 to 0). Their sends to the same FX channel move together the same way, and choosing an output on one sends them all there. Right-click a strip for the same menu as a track header (**Cut**, **Copy**, **Paste**, **Remove**, the duplicates, **Color...**, **Rename...**, **Add Bus for Selected Channels**).

### Inserts and sends
Click the arrow at the bottom of a strip (**Show inserts and sends**) to open its panel. Under **Inserts**, + adds an effect; each insert has an on/off light, its name (click to open), a window button and x to remove. Drag inserts to reorder them. A channel holds up to 16. Greyed names in italics after the inserts are placeholders from a DAWproject: plugins that can't play here, kept for when the song goes back out (see Other DAWs' Projects).

Under **Sends**, + sends the channel, after its fader, to an FX channel, a **New FX Channel**, or a plugin's sidechain. Each send has a light, a level slider (double-click for 0 dB) and x.

Some plugins hold the sound back a little to do their work (a look-ahead limiter, a linear-phase EQ). Sonify lines everything up for them by itself: every track, clip effect, FX channel and bus is held back as much as the slowest, so they all meet in time, and the metronome, recordings, mixdowns, stems and freezes stay in step with the song. Nothing to set. A bypassed insert holds nothing back. Up to about two thirds of a second at 48 kHz is lined up.

### FX channels and buses
- An FX channel plays what tracks send to it, through its own inserts and fader. **Remove** in its panel deletes it.
- A bus is a channel tracks play into instead of Main: **Track > Add Bus Channel**, or select channels and **Track > Add Bus for Selected Channels**. Then pick it in a track's output menu, under **Buses**. The number button on a bus selects what plays into it.

### Sidechain
To duck one sound under another, add **Sonify Compressor** to the channel to duck and set its **Sidechain** to **On** (other plugins with a sidechain input work too). On the channel that should drive it, such as the kick, add a send and pick the plugin under **Sidechain**.

### Main and Sub outputs
Main has inserts (effects only), a fader, **M** (the metronome still plays) and a star that opens the Mastering Assistant. The numbers above its fader hold the highest peak; click to reset. The button at the top (such as **Out 1+2**) opens **Song Setup > Audio I/O Setup > Outputs**, where you add Sub outputs; each gets its own strip.

> Tip: While the Analog Board mixes the song, the Console is dimmed and silent. **Mix on the Console** switches back; **Open the board** goes to it.

## Browser

The Browser on the right holds what you add to a song. Toggle it with **View > Browser**, the Browser button in the transport bar, or **F5** (default keys). Tabs: **Instruments**, **Effects**, **Files** and **MIDI**.

### Instruments and Effects
- **Sort by:** **Flat**, **Folder**, **Vendor** or **Type**, with **Favorites** and **Recent** first. **Search** finds names, vendors and types.
- Drag an item onto a track or Console strip (an instrument replaces the instrument, an effect joins the inserts), or below the tracks for a new track.
- Double-click an instrument for a new track with it; double-click an effect to add it to the selected track (or to Main, after clicking Main in the Console).
- Right-click for **Add to Favorites**, **Show in Explorer** and **Remove from Browser**. A greyed item's file is missing.

Sonify loads VST3, CLAP and WCLAP plugins. The folder button (**Plugin Folders**) sets where the Browser looks, the round arrow (**Rescan**) looks again, and + adds plugin files by hand; the File menu has all three. At each start, answering No to **Load your third-party plugins?** offers only Sonify's own plugins until the next start.

### Files
Pick a place (**Home**, **Music**, **Song Media**, **Recordings**, a drive...) from the menu at the top; double-click folders to open them, and use the arrow to go up. Click a file to see its format, then the play button or **Preview** to hear it. Double-click it (or **Import**) to put it on the selected audio track at the playhead (or a new audio track), or drag it onto the arrangement. A video file becomes the song's video.

**Open** chooses your files folder. The song keeps it (save the song), and it becomes the folder new songs open. A song opens on its own files folder; a song without one, and a new song, open the folder for new songs.

### MIDI
**MIDI** holds the MIDI tools (see MIDI Tools). AI Tools' **MIDI Generators** tab has them too.

## Plugins and Effects

### Instruments
A new instrument track plays the built-in **Cayman**. Pick another from the instrument menu on the track header, or drag one from the Browser onto the track. The keyboard icon on the track header opens the instrument's window.

**Cayman** has a window of its own too (the keyboard icon, or **Open** in the Inspector): two oscillators (**Classic**, **Sine**, **Triangle**, **Saw**, **Square**, **Pulse**; the second tuned and detuned against the first), a sub and noise, a filter (**Low Pass**, **High Pass**, **Band Pass**, off at first) with its own envelope and key tracking, the amp envelope and velocity, an LFO (vibrato, which the mod wheel adds to, the filter, tremolo), **Poly**, **Mono** or **Legato** with glide, the pitch bend's range, drive and chorus. It starts as the tone it always had.

### Effects
Add effects with **+ Insert** on a Console strip, or drag them from the Browser. They play top to bottom.

### Plugin windows
Plugin windows float over Sonify: drag by the title, resize from the corner, close with x. A plugin without its own screen gets sliders for its settings. VST3 editors open in a separate window.

The strip at the top of a plugin's window has its **ON/OFF** and **PRESETS**:
- **Save Preset...** and **Open Preset...** keep everything the plugin is set to in one .sonifypreset file, for any plugin, a VST3 or CLAP from another maker too. They're kept in Documents\Sonify\Presets\<plugin>, and open on any computer with Sonify.
- **Save as VST3 Preset...** and **Open VST3 Preset...** use the VST3 standard (.vstpreset), as Cubase, Studio One, Fender Studio, Reaper and Bitwig save them: presets move between Sonify and those DAWs. VST3 plugins only.
- **Show Presets Folder** opens the plugin's folder of presets.

### Sonify's own effects
Always in the **Effects** tab:
- **Sonify EQ**: low and high cut, two shelves, two mid bands.
- **Sonify Compressor**: threshold, ratio, attack, release, knee, makeup, mix, Peak or RMS detection, and a **Sidechain** switch.
- **Sonify Limiter**: gain into a true-peak limiter.
- **Sonify Mastering**: a whole mastering chain, set up by the Mastering Assistant.
- From Grex: **Sonify Chorus** (a bucket-brigade chorus, voiced like a Juno), **Sonify Phaser**, **Sonify Auto-Wah** (a band-pass the playing and an LFO sweep), **Sonify Reverb** (a hall with early reflections; Dry and Wet apart), **Sonify Delay** (ping-pong, a note value in time with the song or free milliseconds), **Sonify Sweetener** (**Soften**, **Peak**, **Tame**, **Round**) and **Sonify Ducker** (two bands that dip where its sidechain plays: send a vocal to it to carve room in the guitars).

Each opens a designed window (double-click it in the Console or the Inspector): its knobs, its meters, and an EQ curve where it has one. The window resizes from its corner, keeping its shape. Percentages move in whole steps (0 to 100%) and other values show at most two decimals.

Each window (and Cayman's) has a preset manager: the arrows step through the presets, a click on the name lists them all, under **Factory** (at least 12 each, starting with **Default**), **My presets** and any banks you loaded. The same menu has **Save** and **Save as...** (your own presets, kept in `<user data>\Presets\<plugin>`), **Load a preset file...**, **Save bank...** (every preset in the list into one `.sonifybank` file), **Load bank...** and **Delete**. A * after the name means the knobs moved since it was chosen; the song remembers the preset.

## System Tools

Sonify's system tools are in the Browser: **Node Rack**, **Playlist Player**, **Sonify Sampler** and **Tomahawk** with the instruments, **Node Rack FX** with the effects.

### Node Rack and Node Rack FX
A Node Rack wires a whole patch of plugins into one slot, synced to the song's tempo and time signature: **Node Rack** as a track's instrument, **Node Rack FX** as an insert. Open it to get its window: **Rack** (the node canvas), **Mixer** (a fader and meter for the output and each plugin) and **Manual**.
- Add a node: drag a plugin from the Browser onto the Rack, click **Browse...** for a plugin file, drop a plugin file from your folders, or right-click the empty canvas: **System Tools** (**Connector**, **Container**, **Latcher**, **Step Seq**, **Transient Splitter** and more), your instruments and effects, or **Saved Patches**.
- Drag from pin to pin to connect: blue audio, green sidechain, red MIDI. Double-click a cable to delete it.
- Node buttons: **E** opens it (or enters a Container; **Back** leaves), **CH** MIDI channel, **M** mute, **P** pass-through, **T** own tempo and time signature, **X** delete.
- Drag the canvas to pan; **Ctrl** + mouse wheel zooms.
- Click the name in the header to rename; **Save Patch** and **Load Patch** keep it as a file, **Reset** empties it, **PANIC** lets every note go.
- **Macros** (above the Rack page's canvas): eight knobs, each driving any controls in the rack. Click a macro's **Learn**, then move a control in the window of a plugin in the rack; or use its ▾ menu: **Assign a Control** (a plugin's knobs, its **Level** in the Mixer, or the **Rack Output**), and for each control it drives **Range and Invert...** (where the macro at 0 and at 100 puts it, and **Invert**) or **Take Away**. Double-click a macro's name to rename it. The macros are the rack's own knobs: automate them, link them with MIDI Link, or ask Fanan AI. They're saved with the song and in patches.

### Playlist Player
Plays a list of songs and videos live. **Add Tracks** fills it, **PLAY** / **PAUSE** and **STOP** play it, **Auto-Play** goes down the list, and **Show Controls** gives each song its volume, pitch, speed, wait and fade. **Save** and **Load** keep playlists. Note 15 plays or pauses, note 16 stops.

### Sonify Sampler
Plays SFZ sampled instruments. Click **Load SFZ...**, or **Latest** for the one you last made in the Sampling Studio. Shape it with Poly or Mono, glide, envelopes, a filter and three LFOs; keep it with **Save Preset...** or **Save SFZ**.

### Tomahawk
Plays Grex's sound library with Grex's own engine, so every sound is exactly as it is in Grex: two packs, **CLASSIC** and **ORIENTAL**, each in categories. Everything is in one window, Grex's instrument editor, with Grex's keyboard along the bottom to hear the sound.
- **The library**: **GET THE LIBRARY** (top right) downloads it (one zip, about 12 GB). Unzip it anywhere, then click **LOCATE LIBRARY** and choose the **Tomahawk_Sampler** folder (or the folder it's in). Sonify remembers it, for every Tomahawk.
- **SOUND ENGINE**: pick a sound with **PACK**, **CATEGORY**, then **INSTRUMENT** (eight to a page; the numbered **PAGE** buttons for more). It plays with its own settings, its preset. Its pack's picture shows beside the lists. Under the lists, Grex's **LOW VELOCITY RESPONSE** (lifts soft notes) and **PITCH BEND RANGE** (1 to 12 semitones).
- **SYNTHESIS**: the amp envelope and gain, the filter, the velocity curve, Poly or Mono and glide, the octave. Double-click the **GAIN** handle for the sound's **base gain** (in dB: what GAIN's middle, 100, means), or the **OCTAVE** slider for its **base octave** (where OCTAVE's 0 plays, -3 to +3). Both are saved in the sound's preset (yours if you saved one, else the factory one).
- **MODULATION**: the amp LFO, the filter LFO and attack glide.
- **EFFECTS**: one button per effect in a row (**EQ**, **CHORUS**, **WAH**, **PHASER**, **DELAY**, **REVERB**, **PAN**, **SWEETEN**); click one to set it below. Each effect is the sound's own.
- **SAVE PRESET** (top) keeps the sound's settings as its own: it comes with them every time it's picked, in every song. Presets live in the **Tomahawk_classic_instruments_presets** and **Tomahawk_oriental_instruments_presets** folders beside Sonify.exe. When Sonify can't write there (installed in Program Files), it saves into the same folders in `AppData\Roaming\Fanan\Sonify\Tomahawk`, and those win. To use a preset someone shared, drop its .ins file into the right category folder in either place (it's picked up the next time a Tomahawk's window opens).
- The song keeps the sound and every setting. The Tomahawk's **Volume**, **Bend Range** and **Low Velocity Response** are automated like any plugin's. The sustain pedal holds notes. A mixdown waits while a sound is read.

## MIDI Tools

### MIDI FX
MIDI FX change an instrument track's notes before the instrument hears them. On an instrument strip in the Console, click **+ MIDI FX** and pick from **Built in**, **Magenta** or **My Devices** (made in the Plugins Creator Studio). Click a device to set it; its light switches it off. In the strip's panel, x removes one and dragging reorders them. Up to 8 per track.

Built in: **Arpeggiator**, **Chord**, **Scale**, **Pitch**, **Velocity**, **Velocity Curve** (Grex's: drag its graph up to play harder, down to play softer; double-click for a straight line), **Note Length**, **Echo**, **Note Repeat**, **Strum**, **Humanize**, **Random**, **Range**, **Euclidean**, **Harmonize**, **Transpose Map** and **Follow Chords**. Those that work in a key follow the song's key.

- **Harmonize** moves each note to the chord another instrument track is playing: pick the track in **Follows**. **Fit** goes to the nearest chord note, or up or down to one; **No chord there** keeps the last chord, plays the note as it is, or leaves it out. The followed track plays first, so a chord change lands right on time. (Magenta's Harmonize, below, writes a harmony instead.)
- **Transpose Map** turns each of the twelve notes into another, in every octave (every D an F#, say), or leaves it out. A note moves to the nearest note of its new name.
- **Follow Chords** moves each note to the chord track's chord where it plays (Master Song Channel > Chords); the notes in the clips stay as they are. **Fit**: **Nearest**, **Up** or **Down**. **Part**: any chord note, or **Bass** (the chord's root, or its bass). The quickest way: right-click an instrument track's header or strip and tick **Follow Chords**.

The Browser's **MIDI** tab lists every tool: drag one onto an instrument track, or double-click it for the selected track.

### Applying a tool to notes
1. Select notes in the Editor, or MIDI clips.
2. Choose **Event > MIDI Tools...**, the Editor's **Tools...**, or **Apply...** in the MIDI tab.
3. Pick the **Tool**, set it, and click **Apply**. **Edit > Undo** takes it back.

AI Tools' **MIDI Generators** tab does the same on a clip you pick from a list.

### Magenta tools
**Continue Melody**, **Continue Drums**, **Improvise**, **Melody Maker**, **Drum Maker**, **Groove**, **Drumify** and **Harmonize** run on your computer. **Install** each in the MIDI tab or in AI Tools' MIDI Generators (a one-time download, no credits). When you apply one, set **Do** (**New**, **Vary** or **Morph**), **Surprise** and whatever else it offers; Morph needs two MIDI clips selected. Added as MIDI FX, a Magenta tool hears what you play and answers.

## Audio Tools

Select an audio clip, then use the **Event** menu or right-click the clip:
- **Sync Tempo**: a stretched copy on the song's beat. **Follow Song Tempo** keeps it there when the tempo changes.
- **Pitch...**, **Reverse**, **Normalize** and **Back to the Original**.
- **Audio to MIDI...**: **Notes** (any instrument or voice) or **Drums** (kick, snare, hi-hat), as a MIDI clip on a new track under it.

The original is always kept; each change is one undo step. AI Tools has the same in its **Tempo & Pitch** and **Audio to MIDI** tabs.

### Split into Stems
1. Select an audio clip and choose **Event > Split into Stems...**.
2. Choose which of **Vocals**, **Drums**, **Bass**, **Guitar**, **Piano** and **Other** you want, and whether to **Mute the original track**. **Other** holds everything the others ticked don't.
3. Click **Split**. **Hide** lets you keep working; **Stop** cancels.

Each stem goes on a new audio track under the clip. It runs on your computer and takes a few minutes for a song. The first time it downloads 98 MB, and Guitar and Piano a second model the first time they're ticked (110 MB), so you need the internet then. AI Tools' **Stem Splitter** tab splits stems too, and can turn the stems into MIDI and find the chords, key and tempo in the same go.

## Session Players

A drummer, a bass player and a keys player play your chords in a style, each on a new track. Open **Track > Add Session Players...** (or the + over the track list).

1. Type the chords, like C | Am | F | G: | between bars, two chords share a bar, % repeats the bar. Or pick a **From Track** to read its chords (a recording is listened to for its tempo too; **Half Tempo** or **Double Tempo** fixes a wrong guess).
2. Pick a **Style**: Pop, Rock, Ballad, Funk, House or Hip-Hop.
3. Set **Intensity** (Calm to Busy), **Start at Bar** and how many times to play.
4. Tick the **Players** and pick each one's instrument: **Automatic** (the best fit you have installed), **Cayman**, or any installed instrument.
5. Click **Add Players**. One undo removes them.

If a recording has its own tempo, adding the players sets the song to it and lines the recording up with the bars. The drums use General MIDI notes, so any GM drum plugin plays them.

## Video

A song can have one video.

- **Song > Import Video...** (also in the File menu), or drag a video from the Files tab onto the arrangement. Its picture shows in the Video lane over the tracks; its sound goes on its own audio track.
- Drag the picture strip to move it (**Alt** for no snap). Its sound clip doesn't follow; move both.
- Double-click the strip, click the window button on the Video header, or use **Song > Video Window** to see the picture where the song is. In that window, double-click or press **F** for full screen, **Escape** to come back.
- Right-click the strip (or click ... on the Video header) for **Move to the Song's Start**, **Export Video...** and **Remove Video**.

**Song > Export Video...** writes a new MP4: the picture with the song's mix as its sound. You choose where it's saved (the song's Mixdown folder is suggested); a progress window follows, with **Cancel**.

## Export and Mixdown

### Export Mixdown
**Song > Export Mixdown...** (**Ctrl+E**, default keys) renders Main to a file, faster than real time.
- **Location**: the song's Mixdown folder; ... picks another.
- **Format**: **WAV**, **FLAC (lossless)**, **MP3** or **OGG Vorbis**.
- **Quality**: 16 Bit, 24 Bit or 32 Bit Float (FLAC up to 24 bit); 128 to 320 kbps for MP3 and OGG.
- **Sample Rate**: the device's or another (MP3 up to 48 kHz).
- **Range**: **Song** or **Loop**. **Tail**: seconds for effects to ring out.

Click **Export**; a progress window follows, with **Cancel**, then **Show in Explorer**. Channels playing into a Sub output aren't in the mixdown. Your choices are kept until Sonify closes.

### Export Stems
**Song > Export Stems...** writes each ticked track, bus and FX channel to its own file, after its inserts, fader and pan, in a folder named after the export plus "Stems" (**All**, **None**, **Main's mix too**). Solos are ignored; muted channels export silent.

> Tip: For a podcast-ready file, use **Podcast...** in the Live Show Studio's **Live & podcasts** tab.

## Mastering

The Mastering Assistant listens to the whole song, then puts **Sonify Mastering** last on Main: EQ, glue compression and a true-peak limiter that keeps peaks under -1 dBTP.

1. Choose **Song > Mastering Assistant...**, the star on Main in the Console, or AI Tools' **Mastering Assistant** tab.
2. Pick the **Loudness**: **Dynamic** (-16 LUFS), **Streaming** (-14) or **Loud** (-10).
3. Pick the **Character**: **Clean**, **Warm** or **Bright**.
4. Click **Master**. The song plays silently, faster than real time.

You then see the loudness and peak before and after. Switch **Master** and **Original** to compare; **Same loudness** matches their levels. Main's fader goes to 0 dB. **Controls...** opens Sonify Mastering to fine-tune it, **Master Again...** starts over with new choices, and **Remove** takes it out (**Edit > Undo** brings it back). The mastering is kept with the song.

# Analog Board

## Overview

The Analog Board is a second mixer laid out like an analog desk: a channel strip for every instrument and audio track, in the song's order, and the master section on the right. Open it with the **Analog Board** tab in the top bar, **Ctrl+2**, or **View > Analog Board**.

### Which Mixer Mixes the Song

Only one mixer mixes the song at a time. Click **Mix on the board** at the top to switch the board on (its lamp lights); click it again to go back to the Console. The note beside it says which mixer is mixing.

While the board mixes, each track's dry sound (before any insert) comes into its strip, and the Console's inserts, faders, sends, FX channels, buses and Main are left out. The Console is dimmed, with **Open the board** and **Mix on the Console** buttons.

- The first time the board takes a song, its strips start at the Console's faders, pans, mutes and solos.
- **Copy Console levels** does that again at any time (the master takes Main's fader).
- **Reset board...** puts every strip and the master back where they start.
- The meters only move while the board mixes. You can't switch mixers during a mixdown.
- The board, and which mixer mixes, are saved with the song.

## Channel Strips

Each strip runs top to bottom like a desk channel, with the track's number and name on a strip of tape at the bottom. Hover over a knob to read its value.

### Preamp and Filter

- **Trim** sets the level coming in. **Drive** adds soft, tube-like saturation to the loud parts.
- The phase button (the crossed circle) flips the polarity.
- **HPF** is a high-pass filter from 20 to 400 Hz (off all the way down).
- **EQ** switches the EQ in or out. It starts in.

### EQ

Four bands, each a gain knob (up to 15 dB boost or cut) with a **Freq** knob: **HF** (high shelf, 1.5 to 16 kHz), **HMF** (bell, 600 Hz to 7 kHz), **LMF** (bell, 200 Hz to 2.5 kHz) and **LF** (low shelf, 30 to 450 Hz).

### Dynamics

- **Comp** switches the compressor in (it starts out). Set **Thresh**, **Ratio**, **Release** and **Gain** (make-up).
- **Fast** gives a fast attack for sharp peaks.
- Five lights show the gain reduction: 1, 3, 6, 10 and 15 dB.

### Dynamic EQ

Two EQ bells, after the compressor, that dip only while their key is loud in their own bands. Both start out.

- **1** (teal knobs) and **2** (purple knobs) switch each bell in.
- Each bell has **Freq** (40 Hz to 16 kHz) and **Q** (a higher Q is narrower), **Thresh** (where it starts to dip) and **Range** (how deep it can go, up to 24 dB).
- **Release** sets how fast both bells come back.
- **Key** picks what both bells listen to: **Own sound** (the strip's own sound, to tame a harsh band only when it rings), or another track's strip. For example, key a guitar to the vocal with bell 1 at 2.5 kHz and bell 2 at 300 Hz, so the guitar makes room only while the vocal sings; or a bass's 80 Hz to the kick.
- The key is that strip's sound after its compressor and before its fader; a muted key doesn't dip anything.
- Five teal lights show how far the deeper bell dips: 1, 3, 6, 10 and 15 dB. The dips are in the sends too.

### Sends, Pan and Fader

- **Plate** and **Echo** send the sound, after the fader, to the master's Plate reverb and Tape Echo. Both start off.
- **Pan**, **Mute** and **Solo** work as on the Console, among the board's strips.
- The fader goes up to +10 dB, with a stereo LED meter and a clip light beside it.

Right-click a strip's name tape and choose **Reset this strip** to reset just that strip.

## Master Section

Two VU meters, the **Bus comp** reduction lights, four units and the **Master** fader, which sets the level going out of Main's outputs.

### Plate

A plate reverb: **Decay** (length), **Tone** (brightness), **Pre-delay** (up to 100 ms) and **Return** (how much comes back into the mix).

### Tape Echo

A tape loop whose repeats get darker each time round. With **Sync** on, **Time** is a note value (1/32 to 1/2) at the song's tempo; off, it's in milliseconds. Then **Feedback** (repeats), **Tone**, **Wow** (the tape's wobble) and **Return**.

### Bus Compressor

Glues the whole mix. **In** switches it in. Set **Thresh**, **Ratio** (2:1, 4:1, 10:1), **Attack** (0.1 to 30 ms), **Release** (0.1 to 1.2 s, or Auto, which follows the music), **Gain** and **Mix** (below 100% for parallel compression).

### Tape Machine

The mix recorded to tape and played back. **In** switches it in. **Drive** rounds off the peaks, **Speed** is 15 ips (deeper low bump, softer top) or 30 ips (tighter, more open), **Wow** adds wow and flutter, and **Hiss** adds tape hiss.

## Mastering Section

The Mastering section is the last thing on Main, after its fader, whichever mixer mixes the song: the Console, or the Analog Board after its **Master** fader. Open its window with **Mastering...** at the top of the board, the **MS** button on Main's strip in the Console, or **Song > Mastering Section...**. Drag the window by its top; **Esc** or **x** closes it. It's saved with the song.

- **In** switches it in or out (it starts out). The sound fades out and back in over a few milliseconds, never a click. While it's in, **MS** on Main's strip and **Mastering...** on the board light up.
- **Mode** is what Fanan AI's mastering works for: **Mix + Master** or **Master only** (one finished track). **Auto** decides by itself, and shows what it chose: Master only when one audio track has clips and no other track does.
- The meters show what leaves Main, in or out: loudness in LUFS (**Momentary**, **Short-term**, and **Integrated** from when the song starts playing) and the highest **True peak** (red above the ceiling). **Reset meters** starts the integrated loudness and the true peak again.
- **Reset...** puts its controls back where they start (In and Mode stay as they are).

### Its Chain

- **EQ**: **Low Cut** (off all the way down), **Low** (a shelf, with its **Freq**), **Mid** (a bell) and **High** (a shelf).
- **Multiband**: the sound split in three at **Lo/Mid** and **Mid/Hi**. **Comp** switches the three compressors in (they start out); **Attack** and **Release** are theirs together. Each band (**Low**, **Mid**, **High**) has its own **Thresh**, **Ratio**, **Gain** and stereo **Width** (0% mono, 100% as it is, up to 200%; turning the Low band's down keeps the bass in the middle). Each band's lights show how far it's pulled down.
- **Tape**: **In** and **Drive**, a soft tape saturation with a little low bump and a softer top.
- **Limiter**: **Gain** into a true-peak limiter that never lets the sound over its **Ceiling** (-1 dB at first); **Release**. Its lights show how far it's pulling down. Turn up its Gain to make the song louder.

### Master a File

**File > Master a File...** opens an audio file (WAV, MP3, FLAC and the rest) as a new song of its own, named after it with " Master", the file on one audio track, the Mastering section in and its Mode **Master only**, with its window open.

### Exporting

While the Mastering section is in, **Song > Export Mixdown** has a **Mastering** choice: **With the Mastering section**, **Without it**, or **Both (two files)**, the one without it saved beside the other as "<name> (unmastered)". The Mastering Assistant and stems always leave it out.

## AI Mix and AI Master

Fanan AI can mix the song on the board and master it, or master it alone, as a **suggestion** you compare with what you had and keep or not. Open its window with **AI Mix...** at the top of the board, **AI Master...** in the Mastering window, or **Song > AI Mix & Master...**. It uses Fanan AI credits.

1. Choose **AI Mix** (Mix + Master: the board mixes the song, every strip and the Mastering section can change) or **AI Master** (Master only: just the Mastering section; the mix stays as it is). Sonify says which fits the song.
2. Pick a **Target**: **None** (clean and balanced, -14 LUFS), a **genre** (each with its own loudness, tonal balance, width and style), or **Match a reference track...** (choose a finished song; Sonify listens to it first). Add a note if you like ("keep the vocal upfront", "warmer").
3. Click **Make AI Mix Suggestion** (or **AI Master Suggestion**). Sonify renders the song to listen to it (the song is quiet for a few seconds now and then), Fanan AI moves the knobs, listens again and adjusts, then Sonify measures the result. **Stop** puts everything back as it was.
4. Read what it did and why, and the loudness and true peak **Before** and with the **Suggestion**.
5. Compare with **Suggestion** / **Before**: Before is heard at the suggestion's loudness, so louder doesn't fool your ear.
6. **Apply** keeps it (every move is on the board's and the Mastering section's own knobs, so you can change any of it); **Discard** puts everything back as it was.

> Tip: AI suggestions are a starting point, never the one right answer. Trust your ears.

- For AI Mix, the board starts mixing the song (at the Console's levels the first time); Discard switches back to the Console if that's where you were.
- Each strip's dynamic EQ is one of Fanan AI's main tools: it keys one track to another so they make room for each other only when needed.
- While the window works you can close it: the work goes on, and its result waits there.

## Controls

- Drag a knob or fader up or down; hold **Shift** for fine moves.
- Or use the mouse wheel, or click and use the arrow keys (**Home** and **End** take a knob to its ends).
- Double-click, or press **Delete**, to put a knob back where it starts and a fader back to 0 dB.
- Buttons light when they're on.

## Tips

- Moves on the board aren't Undo steps.
- **MIDI Link** stays at the top of the board: move a control on your MIDI device, then drag its chip onto any knob, fader or button here.
- Switching mixers unfreezes frozen tracks, since their sound was rendered for the other mixer.
- The Plate and Echo returns start at 0 dB: set each track's amount with its sends.
- To have Fanan AI set up the board, choose **Analog Board** in the flag on its Send line.

# AI Tools

## Overview

AI Tools holds Sonify's AI musical tools, each a small studio in a tab of its own: **Stem Splitter**, **Audio to MIDI**, **MIDI Generators**, **Tempo & Pitch** and **Mastering Assistant**. Open it with the **AI Tools** tab in the top bar, **Ctrl+3** or **View > AI Tools**; it opens on the tab you used last.

The tools work on this computer, on your song's clips; each change is one **Edit > Undo** step. Every tab picks its clip from a list of the song's clips, track by track, with the clip selected in the Studio chosen first; under it, where the clip is and what was done to it. An audio tab's **Import Audio...** puts a file from your computer into the song and chooses it.

While a tool works, a bar shows how far it is, with **Stop**. Then it says what came out, with **Show in the Studio**, which opens the Studio on the new tracks or the clip. Work started elsewhere (Fanan AI splitting stems, a mastering from the Song menu) shows in its tab too.

> Tip: To have Fanan AI do the work in your song, open the AI Assistant (the button at the lower right).

## Stem Splitter

A recording (a song, a mix) separated into its vocals, drums, bass, guitar, piano and the rest, each on a new audio track under it, in time with it. In the same go, Sonify can turn each stem into MIDI and find the song's chords, key and tempo.

1. Choose **The recording**.
2. Tick the stems you want (**Vocals**, **Drums**, **Bass**, **Guitar**, **Piano**, **The rest**), and whether to **Mute the original track**. **The rest** is everything the other stems ticked don't take.
3. Under **And then, in the same go**, tick **Turn each stem into MIDI** and **Find the chords, key and tempo** if you want them.
4. Click **Split into Stems**. **Stop** stops it, keeping what was made.

The bar shows each step: the split, then each stem into MIDI, one after another (the drums as kick, snare and hi-hat, the others as notes and chords), each on a new track under its stem. The chords, key and tempo are heard in the recording while it splits. At the end it says what it made, with **Show in the Studio**.

It's worked out on this computer and takes a few minutes for a whole song. The first split downloads what Sonify splits with (98 MB), once; **Guitar** and **Piano** come from a second model, downloaded the first time you split with them (110 MB). You need the internet for those downloads. It's the same split as **Event > Split into Stems...**.

### Chords, Key and Tempo

**Just the Chords, Key and Tempo** listens to the recording without splitting it (a few seconds). Either way, what was heard shows under the tab:

- **Key** (A minor, say), **Tempo** and how many **Bars**.
- A chord for each bar, four bars a line, each line starting with its bar number; two chords in a box share the bar, **%** repeats the bar before, and **N.C.** is no chord. When the recording plays to the song's beat, the bars are the song's; otherwise they're counted from its first downbeat (**Tempo & Pitch**'s **Sync Tempo** puts it on the song's beat).
- **Copy Chords** copies the key, the tempo and the chords as text, to paste anywhere.

## Audio to MIDI

Sonify listens to a recording and writes what it plays as notes, in a MIDI clip on a new instrument track under it.

1. Choose **The recording**.
2. Choose what it listens for: **Notes** (any instrument or voice, chords too) or **Drums** (kick, snare and hi-hat, on General MIDI notes).
3. Click **Turn into MIDI**.

When it's done, **Show in the Studio** opens the new clip in the Editor, and **Open in MIDI Generators** takes it to the next tab.

## MIDI Generators

The MIDI tools on a clip: the built-in ones (Arpeggiator, Chord, Strum, Echo and the rest) and Magenta's models (Continue Melody, Continue Drums, Improvise, Melody Maker, Drum Maker, Groove, Drumify and Harmonize).

1. Click a tool in the **Tools** list. A Magenta tool not on your computer yet says **Click to download**: clicking it downloads it (once, no credits), and it shows **Installed** when it's ready. **Download All**, by Magenta's name, downloads every one still missing, one after another. **Remove** takes a tool off this computer, and the round arrow looks for Fanan's newest.
2. Set it: its settings are the same as in **Event > MIDI Tools...**. For a Magenta tool, **Do** picks **New**, **Vary** or **Morph** where it offers them.
3. Choose **The clip** (for Morph, also the clip to morph to).
4. Click **Apply**. The first time, a Magenta model takes a moment to load.

A tool that adds a part (Improvise, Harmonize, Morph) puts it on a new track under the clip's; the others change the clip. **Edit > Undo** takes it back.

## Tempo and Pitch

A recording put on the song's beat, moved up or down, turned around or levelled. The original is always kept.

- **Its tempo**: the recording's own tempo. **Detect** listens for it.
- **Sync Tempo**: a stretched copy whose beats land on the song's. **Follow Song Tempo** stretches it again whenever the song's tempo changes (**Stop Following** ends that). With its tempo left empty, Sonify listens for it first.
- **Pitch**: type the semitones from the original (or use **-1** and **+1**), then **Set Pitch**. Its length stays as it is.
- **More**: **Reverse** (or **Forwards Again**), **Normalize**, and **Back to the Original**.

These are the Audio menu's changes, the same as in the Audio Editor.

## Mastering Assistant

Sonify listens to the whole song through Main, then masters it with **Sonify Mastering** as Main's last insert: EQ, glue compression and a true-peak limiter that keeps peaks under -1 dBTP.

1. Pick the **Loudness**: **Dynamic** (-16 LUFS), **Streaming** (-14) or **Loud** (-10).
2. Pick the **Character**: **Clean**, **Warm** or **Bright**.
3. Click **Master the Song**. The song plays silently, faster than real time; **Stop** cancels it.

You then see the loudness and true peak before and after, and what it changed. Play the song and switch **Master** and **Original** to compare; **Same loudness** matches their levels. **Controls...** opens Sonify Mastering in the Studio, **Remove** takes it out (**Edit > Undo** brings it back) and **Master Again...** starts over. Leaving the tab plays the master again. It's the same as **Song > Mastering Assistant...**.

# AI Assistant

## What It Does

Fanan AI is Sonify's assistant. Ask in words and it does the work in your song while you listen: it builds tracks, loads instruments and effects, writes parts and mixes. Open it with the **AI Assistant** button at the lower right of the transport, or **View > AI Assistant**.

Everything one request does is a single undo step: **Edit > Undo** (**Ctrl+Z**) takes it all back. The menu shows it as "Undo AI:" and the start of your request.

### What It Can Do

- Add instrument and audio tracks, FX channels and sends; remove a track only after asking you.
- Load your installed instruments and effects, and Sonify's own, and change their settings.
- Set volume, pan, mute, solo, monitoring, record arm and Main's volume, and set up sidechains (to duck the bass under the kick, say).
- Write MIDI parts, add Session Players to play a chord progression in a style, put MIDI FX on a track, or apply a MIDI tool to a clip.
- Change the tempo, time signature and key, anywhere along the song.
- Split a recording into stems (this takes minutes; the stems arrive after its reply).
- Set up the Analog Board, and switch which mixer mixes the song when you ask.
- Play, stop, rewind, set the loop, open a plugin's window and save the song.

### What It Can't Do

- Hear your audio: its mixing moves are moderate, a starting point for your ears.
- Freeze tracks, or add, move or export the song's video.
- Master by hand: it points you to the Mastering Assistant.
- Make blocks, make or change plugins and MIDI devices, browse the web, or work the Live Show Studio or the Sampling Studio. It tells you what to do there instead.

## The Window

The AI Assistant is a window over the studio, so the song stays in sight while you talk to it.

- Drag its bar to move it, and its lower right corner to size it. Double-click the bar to put it back at the lower right. It opens again where you left it.
- **x**, **Esc** or the **AI Assistant** button closes it; the conversation stays.
- It works over every studio but the Plugins Creator Studio (which has its own **Build with AI**) and the Web Browser, whose pages are windows of their own: there the button rests, and the window comes back with the next studio.

## Asking Well

Type in the **Ask Fanan AI...** box and press **Enter** or click **Send** (**Shift+Enter** starts a new line). A new conversation shows example requests; click one to put it in the box:

- "Add a synth on a new track and write an 8-bar chord progression in A minor"
- "Put a compressor on the bass and sidechain it to the kick"
- "Make a reverb FX channel and send the keys to it"

Say what you want in musical words, in any language. Fanan AI acts first, picking sensible choices itself, and asks only when it must. "This track" means the track selected in the Studio. Ask a question and it just answers, changing nothing.

Each step shows in the conversation with a tick, or a cross if it failed. Anything it wants you to answer or do is shown in yellow.

### The Studio Flag

The flag on the Send line marks a request with the studio it's about, so Fanan AI takes it in that studio's terms: choose **Analog Board**, for example, and it works on the board rather than the Console. The flag stays until you change it, and flagged requests show it in the conversation. **No flag** leaves it to Fanan AI.

### Deep

**Deep**, next to the flag on the Send line, sends every request to Fanan AI's strongest model. It costs twice the credits, so leave it off for the everyday things (mute a track, change the tempo, add a reverb, load an instrument): the quick model does those just as well and faster. Turn it on for the big jobs — writing and arranging parts, building a whole section, many tracks at once.

You don't have to watch it: when a request turns out to be a long one, Fanan AI goes deep by itself to finish it, and the button shows it did. Deep stays as you left it, next time too.

### Stopping and Starting Over

- While it works, **Send** becomes **Stop**. It stops after the current step; what it did so far stays, as one undo step.
- If it pauses after many steps, or its answer was cut off, say "continue", or ask for less at once.
- **New chat** clears the conversation and starts afresh.

## Credits and Sign-in

Fanan AI works through Fanan's server, so it needs the internet and your Fanan account.

1. Click **Sign in** at the top left.
2. Type your email and click **Sign in**.
3. Finish in the browser that opens (or make your account there). Sonify signs in by itself and stays signed in until you sign out.

Requests use Fanan AI credits. Your credits show on the Send line with **Buy credits** beside them, and in the account window (the account button, top left). **Buy credits** opens Fanan's site in your browser; new credits appear within seconds of paying. Credits belong to your account, so they work in every Fanan app, on every computer signed in to it.

If Fanan AI can't work right now, a note above the box says why, often with a button: **Sign in**, **Buy credits** or **Try again**. If the internet switch at the top left is blocking the internet, click it to allow it.

## Voice and Phone

When your version of Sonify offers them, two rows appear above the box: **Voice control** (talk to Sonify from the recording room) and **Phone remote** (talk to Fanan AI from your phone). The Settings and More part of this guide covers them.

## Elsewhere in Sonify

Fanan AI uses the same account and credits everywhere:

- Plugins Creator Studio (**Ctrl+5**): describe a new instrument, effect or MIDI device in **Build with AI** and Fanan AI makes it.
- Plugins Creator Studio, **Protection** tab: **Write it with Fanan AI** writes a store listing's tagline, story and features from the plugin and its picture.
- Sampling Studio, Loop Editor: **Ask Fanan AI** picks the smoothest sustain loops, with their crossfades.

# Song Builder Blocks

## Overview

Song Builder Blocks builds a song from blocks of clips. A block is a copy of the clips of one section, such as a verse or a chorus, across the tracks you chose. Each block is a pad on the page: click pads to line their blocks up in the song's order, then **Build Song** lays them out on the timeline, one after another.

Open it with the **Song Builder Blocks** tab in the top bar, **Ctrl+4**, or **Song > Song Builder Blocks** (it's in the View menu too).

- A block is a snapshot: a copy of the clips as they were, not linked to them. Editing the original clips later doesn't change the block.
- Blocks don't play by themselves. You hear them once they're built into the song.
- The blocks and their order are saved with the song, and every change to them is a step in **Edit > Undo**.

## Making Blocks

First, in the Studio (**Ctrl+1**), select the clips of a whole section, for example every track's clips of the first verse. MIDI and audio clips both work. Then:

- In Song Builder Blocks, click **Make from Selected Clips**. The button counts the clips selected in the Studio. The new pad's name is ready for typing: type a name and press **Enter** (**Esc** keeps the one it has).
- Or, in the Studio, choose **Song > Make Block from Selected Clips**, or right-click one of the selected clips and choose **Make Block from Selected Clips**. The Studio stays in front and says where the block went. **Go to Song Builder Blocks** on the same right-click menu takes you there.

A block covers whole bars: from the start of the bar its first clip starts in, to the bar line after its last clip ends. Its pad shows its name, its length in bars, how many tracks it covers, and a small picture of its clips and notes in the tracks' colours. Unnamed blocks are called Block 1, Block 2 and so on.

### Managing Pads

Right-click a pad for:

- **Add to the Song**
- **Rename**, or press **F2** on the pad.
- **Colour...**: green, blue, amber, pink, red, violet, teal or olive.
- **Duplicate**: a copy right after it, numbered (Chorus 2).
- **Delete**, or press **Delete** on the pad. A deleted block also leaves the song's order.

## Building the Song

The **Song** strip under the pads holds the song's order, each block as wide as its bars.

1. Click a pad (or press **Enter** or **Space** on it) to add its block to the end of the song. Click it again to repeat it: a block can be used as often as you like.
2. Drag a block along the strip to move it; a marker shows where it will land.
3. To take one out, click its small **x**, or click it and press **Delete**. The **Left** and **Right** arrow keys move between blocks.
4. Check the total beside the Song title: how many blocks and bars.
5. Check **Build at bar**: the bar the song starts on. It's the bar after the song's last clip, unless you type another.
6. Click **Build Song**, or press **Enter** in Build at bar.

Each block's clips go onto the tracks they came from, one block after another. A message says which bars the song now fills, and the new clips are selected in the Studio. **Clear** takes every block out of the song; the blocks themselves stay.

## Playing and Editing

The pads don't play: to hear what you built, play the song as usual. The transport stays at the bottom of every studio, so **Space** starts and stops it here too (except while a pad has focus, when Space adds that pad).

- The built clips are ordinary clips: edit, move or delete them in the Studio.
- To change a block itself, edit the clips in the song, select them again and make a new block. Delete the old pad if you no longer need it.
- **Edit > Undo** (**Ctrl+Z**) takes back a whole build in one step, as well as any change to the pads or the song's order.
- If a track a block used has been removed, its clips are left out of the build, and a message says how many.

## Live

**Live** (at the top of the page) turns the blocks into a clip launcher: launch them on their tracks as the song plays, and what you launch is written on the timeline after the end of the song. The song goes to the bar after its last clip, and the page shows a grid: a row for each block, a column for each track the blocks use.

- Click a cell to launch that block on its track, a block's name (▶) to launch all of it, ■ under a column to stop that track, **Stop All** to stop them all.
- **Launch on**: **the next beat**, **the next bar** or **the next 2 bars**. A launched block loops until another is launched on its track or it's stopped. With the song stopped, a launch starts it.
- A playing cell is filled in its block's colour; one waiting to start, or a ■ waiting to stop, blinks.
- The whole jam is one Undo step. Stopping the song takes away what was laid past the bar it's in; switching Live off stops at the next launch point and keeps what was laid.
- MIDI Link links pads to the cells, the block names, the ■ buttons and **Stop All**, by their place in the grid (so a link means the same place in every song). A Launchpad or APC mini added as a control surface is the grid itself (see Control Surfaces in the Settings and More tab). Pads work while Live is on.

## Tips

- Make a block for each section (intro, verse, chorus, bridge) across all its tracks, then try song orders in seconds.
- Build Song adds clips and never removes any, so build where the tracks are empty. The suggested bar, after the last clip, always is.
- Try an arrangement, listen, then **Undo** and try another.
- Give each kind of section its own colour, so the song strip reads at a glance.

# Plugins Creator Studio

## Overview

The Plugins Creator Studio is where you make your own instruments, effects and MIDI devices by asking Fanan AI. Open it from the studio selector, **View > Plugins Creator Studio** or **Ctrl+5**.

### The five tabs
- **Build with AI**: describe what you want; Fanan AI builds it, playing live as it works.
- **Edit UI**: reshape the plugin's panel and play it.
- **Knob Studio**: design how its knobs, sliders and switches look.
- **Protection**: protect it with its own serial formula, so it plays as a demo until a buyer registers it.
- **Store**: Sonify's notice board: ads for what users made, with links to their sites; post your own.

### Plugin types
- **Instrument**: plays notes, like a synth.
- **Effect**: changes sound, like a delay.
- **MIDI FX**: a MIDI device on an instrument track that changes, makes or filters notes.
- **Widget**: anything else, like a tuner or a spectrum display.

The type is chosen when you start and never changes; for another type, start a new plugin.

### Projects
The **Options** menu at the top left has **New plugin**, **Open**, **Open recent**, **Save** (**Ctrl+S**), **Save as** (**Ctrl+Shift+S**), **Show project folder**, **Close project**, **Sample library...** (see Sample Library) and **Sound when Fanan AI finishes**.
- **New plugin** asks for a **Name**, a **Type** and your **Company** (shown as the developer).
- Work is saved as it changes. **Save as** makes a copy under a new name and carries on in the copy.
- **Open** opens a plugin project or a MIDI FX file.
- **Ctrl+Z** undoes the last change, Fanan AI's work included; **Ctrl+Y** redoes it.

## Build with AI

Everything starts here. Fanan AI needs the internet and a Fanan account, and building uses Fanan AI credits (the same credits all of Sonify uses).

### Sign in and credits
Type your email in the **Account** box, click **Sign in** and finish in your browser. Your credits show at the top right. **Buy credits** gets more on the Fanan site; **Sign out** signs this computer out (your credits stay with your account).

### Describing a plugin
1. Choose a **Type** beside Send: **Instrument**, **Effect**, **MIDI FX** or **Widget**. Nothing is sent without one.
2. Optionally pick a look beside it: **Look: Auto** lets Fanan AI choose from its inspiration pool.
3. Describe the plugin and press **Enter**, for example "a warm tape delay with wow and flutter".
4. To add pictures, PDFs, text or your own code, click the paperclip (**Files** or **A folder of code**) or drop files on the panel. Pictures can be used on the panel, like a background or a logo.

### The plan and its price
Nothing changes without your approval. Fanan AI first proposes a plan with its steps and an estimated price in credits. As a rough idea of the scale: a typical subtractive synth — a few oscillators, a filter with its envelope, an amp envelope, an LFO and a dozen or so controls — is about 300 credits to build; a small effect is far less, a big plugin with a lot of fixing more. The bar above the request box offers **Start**, **Change** (type what should change for a new plan) and **Cancel**. The plan then ticks along with the credits each step used; a step that runs well over its estimate asks: **Go on** or **Pause**. While Fanan AI works, the send button becomes a stop button (it stops after the current step). A request that fails costs nothing.

The plugin plays as it's built: play it from your MIDI keyboard while the studio is in front. **MIDI In**, at the top right (beside the meter), picks what it hears: tick **All inputs** or just some; its light blinks as MIDI comes through. The plugin's own keyboards always play it. While a MIDI FX is open, your keyboard plays its track instead. To change anything, just ask in the same conversation.

## Edit UI

Edit UI shows the plugin's panel, playing live, on a grid. Its bar holds **Panic** (stops every sounding note), the build status, the **Edit** and **Play** tabs, **Export Plugin**, **Add to Sonify** and **Log** (the compiler's messages from the last build).

### Moving around
**Ctrl** + mouse wheel zooms, the wheel scrolls (**Shift** + wheel sideways), and dragging empty space moves the view. **Fit** (**Ctrl+0**) shows the whole window.

### Edit mode
In **Edit**, a searchable toolbox of parts runs down the left: Controls (knobs, sliders, switches, keyboard, XY pad, preset manager, page tabs...), Displays (meters, scopes, curves, lamps, 3D displays) and Pictures & text.
- Drag a part onto the window, or double-click it to add it in the middle. Pictures can be dragged in from the desktop.
- Click to select, drag to move, drag the handles to resize (**Shift** on a corner keeps its shape); the round handle above turns it.
- **Shift**+click or **Shift**+drag on empty space selects several.
- **Delete** removes, the arrow keys nudge, **Ctrl+C**, **Ctrl+V** and **Ctrl+D** copy, paste and duplicate.
- **Ctrl+]** and **Ctrl+[** move a part forward or backward (with **Shift**: to the front or back).
- Drag the window's corner to resize it, or press **C** to crop it, then **Enter**.

### The right-click menu
Right-click a part (or double-click a knob, slider or switch) for its menu: Copy, Paste, Duplicate and its layer, its settings in tabs (Object, Control, Text, Look, Shape and so on) and **Delete**. Right-click empty space for the window's menu: size, colours, **Add a background picture**, **Paint a lit background**, **Global light** and pages.

At the foot of every menu, "Tell Fanan AI what to do with it": type something like "make it bigger and gold" and press **Enter**. A banner shows its answer; **Open chat** shows the conversation. This uses credits.

### Pages
Click **+ Pages** above the window: the window becomes the "Main" page, a new page is added, and a page tabs part appears to switch them. **+** adds more. Right-click a page tab to **Rename**, **Move left**, **Move right** or delete it. Each part's **Page** setting puts it on one page or **Every page**.

### Auto layout and Play
**Auto layout**, in Edit's row, replaces the panel with a plain automatic one (it asks first). **Gallery…** beside it opens the Gallery's knobs, sliders and switches in a window of their own: drag one onto the panel to add it, or onto a knob, slider or switch to give it that look (it keeps its size, place, parameter and settings); double-click one to add it in the middle. A control's menu has **Replace this item with gallery item** under Copy, Paste and Duplicate, for the same thing from a list. Only the same kind replaces (a knob for a knob). An item marked **NC** has a licence that doesn't allow it in a plugin that's sold. The Gallery opens on **Sonify's gallery** (AppData\Fanan\Sonify\Knob Gallery on Windows, Library/Fanan/Sonify/Knob Gallery on a Mac): the knobs installed with Sonify, and any you add. Drop .knob or .fklayers files from your computer onto the Gallery to copy them into the folder it shows; **Choose Folder…** shows another folder of yours, and **Sonify Gallery** brings you back. In **Play** the panel is locked and works like the finished plugin: drag knobs (**Shift** for fine moves), double-click for the default, play its keyboard or your MIDI keyboard. Play's row is the preset manager.

## Knob Studio

The Knob Studio designs the looks of knobs, sliders and switches in layers, the KnobMan way: each layer a shape with its own light, bevel, texture and movement.

1. In Edit UI, open a control's menu and go to its **Look** tab (its **Look** list also has ready-made looks).
2. Click **Edit in Knob Studio**: its look opens there, and changes change only this control.

Under **Edit in Knob Studio**, **Apply this knob on similars** (slider, switch) puts this control's look on every other control of its kind:
- Tick it and they wear it at once; untick it and each wears its own look again.
- **Apply** beside it does it for good. It asks first, since there's no way back: unticking won't give them their own looks again.

- Every change is saved into the plugin at once and shows on the panel.
- The header has **Use on** this control, **Give this knob its own look** and **Back to the plugin**.
- The left panel starts with KnobMaker's buttons, working on the look being designed:
  - **Save as Sonify knob…** saves it as a .fklayers file (into Sonify's gallery at first, so it's there to pick).
  - **Export to PNG…** writes its frames, top to bottom, as a PNG filmstrip.
  - **Load Sonify knob…** and **Load KnobMan knob…** replace it with a .fklayers or .knob file (with no look open, the file becomes a new look).
  - **Select from Gallery…** opens Sonify's gallery beside the studio, and it stays open. Double-click a knob there (or click **Use**, or drag it onto the studio) and it replaces the look being designed; drag a look from the list below into the gallery to keep it there. Only the same kind replaces (a knob for a knob).
- Under them, the plugin's looks, with **+ New look** (built-in, empty, one from the Gallery, a KnobMan file or a whole folder of them), **Duplicate**, **Save to my looks** (for every project) and **Delete**.
- A look from a KnobMan file is drawn exactly as KnobMan draws it, pictures and textures inside the file included. Edit it here like any look (colours, sizes, turns, masks, layers added, removed or moved) and it stays drawn KnobMan's way; a setting KnobMan has no equivalent for (Bevel & Emboss, say) is left out, and the status line says so. Its **Sonify lighting** switch (above the canvas) lights its spheres and metal circles the way Sonify does, everything else staying as KnobMan draws it.
- The control's **Look** tab can undo the Knob Studio's changes to a built-in look.

## Presets

Presets live in Play's row and in any preset manager part on the panel. Build the plugin first.
- Choose one in the **Preset** list or step with the arrows. A * means its values changed.
- **Save** writes the values over the chosen preset (one of yours); **Save as** makes a new one under My presets, kept on this computer and shared with this plugin wherever it plays.
- **Load** brings in a preset file.
- **More** has **Save as a factory preset** (these come with the plugin when it's added to Sonify or exported), **Save bank** (every preset in one file), **Load bank**, **Delete** and **Show the presets folder**.

The chosen preset comes back when the project opens.

## Sample Library

A sampler plays its sample library: blobs, instruments made by the Sampling Studio's **SFZ to Blob**. Sonify reads their samples from the disk as they play, so a library of many gigabytes needs little memory, like the big commercial samplers.

1. Make the blobs in the Sampling Studio's **SFZ to Blob** tab.
2. Here, open **Options > Sample library...**.
3. Click **Add...** and pick the blobs. **Rename...** sets the name the plugin shows; **Up** and **Down** set their order (the plugin numbers them from 0); **Remove** takes one out (its file stays).
4. Ask Fanan AI to build a sampler that plays them, with an instrument choice and whatever else you want.
5. For a list of the instruments on the panel, drop an **Instrument list** from the Edit UI toolbox (Controls) and bind it to the sampler's instrument parameter. It shows the library's names by itself.

Each row says whether the plugin is playing that instrument, loading it, or can't find its blob. The blobs aren't copied anywhere: the plugin knows them by file name. On another computer it finds them in Documents\<company>\<plugin name>; if they're elsewhere, a made plugin's window shows which are missing, and **Locate...** points it to their folder.

## Add to Sonify

**Add to Sonify**, in the Edit UI bar, puts your plugin into the Browser under **Made in Sonify**, ready to drag onto a track, where it opens with its own panel and presets. Wait until Fanan AI has finished and it's playing, then click it: it's built from its code as it is now, then added.
- Click it again after changes to update it. Tracks already playing it get the new version when their song opens again.
- A **Save as** copy is added as a plugin of its own.
- A MIDI FX doesn't need this: it's on your tracks as soon as it's built.

### Sonify plugins (.sip)
Every Sonify plugin is one file ending in **.sip**: instruments, effects (widgets too) and MIDI FX alike. They're all kept together in Documents\Sonify\Plugins (in your user data folder), and the file itself says what kind it is, so Sonify puts each in its place: instruments and effects in the Browser under **Made in Sonify**, MIDI FX under **My Devices**.
- To share one, send its .sip file. To keep one you got, put it in Documents\Sonify\Plugins (a MIDI FX: **Open** in My Devices does it for you); dropped on a Rack, it plays from where it is.
- A .sip holds no native code: its plugin plays in Sonify's sandbox, so it can't touch your files or the internet.
- Plugins and devices from an older Sonify are turned into .sip files the first time this one starts; the old copies are kept in Plugins\Old format.

## Export Plugin

**Export Plugin**, in the Edit UI bar, makes plugin files any DAW loads, built on this computer. They play wherever you copy them, without Sonify. When the plugin is protected (the Protection tab), each one plays as a demo until it's registered.
- Windows: VST3 and CLAP; the compiler comes with Sonify.
- macOS: VST3, Audio Unit and CLAP. This needs Apple's free Command Line Tools: if they're missing, macOS offers to install them; then export again.
- Linux: VST3 and CLAP, with your system's C++ compiler.

1. Make sure Fanan AI has finished and the plugin is playing.
2. Click **Export Plugin**.
3. Untick any **Formats** you don't want.
4. **Into** is the folder, normally one named after the plugin in Documents\Sonify\Exports; **Choose** picks another.
5. **Copy them into my plugin folders too, where DAWs look** also copies them into your own plugin folders: on Windows AppData\Local\Programs\Common\VST3 and ...\CLAP (hover over it to see them). These need no admin rights; if your DAW doesn't list the plugin after a rescan, add these folders to its plugin folders once.
6. Click **Export**.

When it's done, your plugin folders open with the new plugin picked out in each, and **Show export folder** shows the copy in Into. Each format has a folder of its own named after the plugin (VST3\<name>, CLAP\<name>); a protected plugin's folder also holds its **license.soni**, which must always stay next to the plugin. Without the copy box ticked, copy each plugin's whole folder into your DAW's plugin folders yourself; then rescan plugins in your DAW. The panel and factory presets go with them. If it fails, the Export log says why; if a DAW has the plugin loaded, close it and export again.

A sampler's blobs aren't copied with it. On this computer the exported plugin finds them where they are. To give it to someone, send the blobs too: they go in the Documents folder under <company>\<plugin name> (the message after exporting names the folder), or anywhere listed in the plugin's library.txt (in your app data folder under <company>\<plugin name>, one folder per line).

## Protection and Store

### Protecting what you sell
In **Protection**, turn on **Protect this plugin** (or MIDI FX). Sonify makes it its own serial formula: three numbers, **A**, **B** and **M**. Keep them private and never change them once you sell.
- **Demo times**: how long it plays before each silence, and how long the silence lasts (a MIDI FX: how long notes pass, and how long they're held back).
- **From your buyer's ID to their serial**: the steps, worked out with your own numbers. A buyer sends you their computer ID (9 digits, like 482-039-175); the serial is (ID × A + B), divided by M, keeping the remainder, written with 10 digits.
- **JavaScript**, **PHP** and **Excel** do it for you: **Copy** or **Save** them for your site.
- **Check a serial** tests an ID and a serial against your formula.
Each serial unlocks the plugin only on the one computer whose ID it was made from.

What you sell (exported plugins, Sonify plugins, MIDI FX, protected blobs) carries a file called **license.soni** next to it. Exports put it there for you: always ship the plugin's whole folder.

### Registering
A protected plugin plays as a demo until it's registered. Click its registration badge (or **Register…**): copy **Your computer ID**, send it to the seller, then type or paste the serial they send back and click **Register**. It's kept on this computer, so it stays registered.

### The Store
The **Store** is Sonify's notice board: ads for plugins and MIDI FX users made, each with a link to its maker's site, where you buy it. Nothing is sold through Fanan.
- Search, pick a kind and a section, and click an ad to read it, watch its video, or **Visit website**.
- When the maker offers it, **Add to Sonify** downloads the Sonify version straight away (a protected one plays as a demo until you register it).
- **Post an ad** (signed in): title, section, tagline, story (or **Write it with Fanan AI**, which uses credits), features, a YouTube link, a picture and logo, **your website**, the formats you sell and any price text. Tick **Offer the Sonify version for instant download** to let people add it at once. **My ads** edits, hides or deletes yours.
- The first time you post, Sonify shows the **Store's rules** (no hate, insults, sexual content, violence, scams, impersonation, stolen work, spam or anyone's private details): click **I accept** once.
- Fanan AI checks every ad against the rules before it's posted, its words first and its pictures before it goes on the board. One that breaks a rule isn't posted, and Sonify says which rule and why. After three such posts, posting in the Store closes for that account for good.
- Links go only in the website field. Only Fanan posts under the names Fanan or Sonify, and a maker name belongs to the account that used it first.
- To report someone's ad, click **⋯** on it, then **Report this ad…**. When several people report the same ad, Fanan AI reads it again, and one that breaks the rules comes off the board.

## MIDI Devices

A MIDI FX is a Sonify device in an instrument track's MIDI FX, in front of the instrument. It hears the notes, plays its own, follows the song's key and grid, and can move Sonify's controls (play, faders, pan, mute, solo, a plugin's knob). It doesn't make sound itself. Arpeggiators, chord makers, step sequencers and humanizers are typical.

### Making one
1. In Build with AI, choose **MIDI FX** in **Type**.
2. Describe it, for example "an arpeggiator that follows the chord", and send. You can attach your own code for Fanan AI to port.
3. Fanan AI builds and tests it (using credits), and every build plays at once on an instrument track: the selected one, or the first. Add an instrument track if there's none.
4. Ask for changes in the same conversation; each build replaces it everywhere it plays, keeping its settings.

It has no panel to design: its settings and outputs show in its window on the track. Click an output's **Map**, then the control it should move.

### My Devices
Every MIDI FX you make is kept in My Devices (a .sip file in Documents\Sonify\Plugins), conversation included, so Fanan AI carries on next time.
- To add one, click **+ MIDI FX** on a track's Console strip and pick it under **My Devices**.
- In its window, **Open in Plugins Creator Studio** opens it here to change it; **Update to version** takes a newer build.
- **Save as** saves a copy of its .sip file to share; **Open** opens one from anywhere.

# Live Show Studio

## Overview

The Live Show Studio is for playing to people: music from a playlist, singers and players on stage, podcasts and live streams. Open it with its tab or **Ctrl+6**.

While its tab is in front, or while anything in it plays (the player, the recording or the stream), the show plays on Main in place of the song, and the song stops. Switch to another studio while it plays and the Studio is heard together with it.

The tabs at the top choose how the show is used:
- **Live & podcasts**: live channels for singers, players and voices, a playlist with one player, the show's screen for videos, and **Podcast...**.
- **Live streaming**: the same, sent out live with up to four cameras and the screen.

In the middle of the tab line are the show's switches. **Mute mics** and **Mute playback** (Live & podcasts and Live streaming) silence the live channels, or the player. Both are on every time Sonify starts, so turn them off before you begin. Live streaming adds **Enable streaming**, **Stream settings**, **Controller** and **Monitor**.

The show is saved with the song; stream settings and the Controller's setup stay on this computer.

> Tip: Knobs and faders: drag (Shift for fine moves), use the wheel or the arrow keys; double-click puts one back where it starts. Hover a knob to read its value.

## Library

The Library panel lists the audio files in a folder. Click **Browse...** to choose your tracks' folder; it opens there from now on. Click a folder to open it, **Up** to go back.
- Each track shows its tempo (BPM) and key once Sonify has listened to it: the first time it's loaded, or click **Analyze** to listen to every new track in the folder.
- Type in **Search** to find a track by name, key (Am) or tempo (124).

## Playlist and Player

Music plays from a playlist through one player, with the mixer on its left. Tracks never go straight into the player.

### Building the playlist
- Double-click a track or video in the library, click its **+**, or drag it onto the playlist.
- From a folder on your computer, drag files over the playlist and hold a moment before letting go.
- Drag a track by its grip to move it; its **x** takes it off.
- **Save...** keeps the playlist in a file with every track's options, hot cues and loops. **Open...** opens one. **Clear...** empties the playlist and the player.

### Playing
1. Press a track's round play button: it goes into the player, ready (anything playing stops, back at its start).
2. Press the player's Play button.
3. With **Auto** on, the playlist then plays itself, each track after the one before. The one coming up is marked **Next**.

A track that ends or is stopped goes back to its start.

### The player
The player shows its track's name, key, tempo and times, over a scrolling waveform with the beats marked and the whole track under it (click there to jump).
- The Play button starts and pauses it; the back-to-start button left of it takes the track back to its start.
- The **Tempo** slider is the player's BPM; move it and the track follows. Double-click it for the track's own tempo.
- **SYNC** makes the tempo follow the song's (in the transport under the page).
- **Jump to cue** 1 to 4 jump to the hot cues. They start a little apart at the track's beginning: drag each marker to where it belongs.
- **L1** to **L4** each put a four-beat loop where the player is (click again to remove it); the round-arrow button beside each plays round that loop until clicked again. Drag a loop's edges to resize it, or its middle on the waveform to move it.
- The lock button keeps every marker where it is.
- **BPM**: the track's own tempo; type it if Sonify got it wrong (40 to 300).
- **SLIDER SYNC**: on, the track plays at the player's Tempo; off, as recorded.
- Key **-** and **+** move the key by semitones, to suit a singer; a track keeps its key at any tempo. Double-click the key to reset it.
- **Vol**: the track's own volume, as recorded in the middle, up to +25 dB.

Hot cues, loops and typed tempos are remembered for each track.

### Track options
Click a banner to select it; double-click it (or click its arrow) to open its options:
- **BPM**: its own tempo, heard or typed.
- **Sync to player**: it plays at the song's tempo (the player's **SYNC** lights with it); off, as recorded.
- **Key**: semitones up or down.
- **Next**: when the next track comes, in seconds: 0 at this one's end, 3 a 3-second pause, -5 a crossfade over its last 5 seconds.
- **Vol**: its volume, as recorded in the middle.

## Singers and Players

Live channels put singers, players and voices on the show (Live & podcasts and Live streaming). Each channel is a Studio track heard through its own plugins: a singer's EQ or reverb goes on their track in the Studio.
1. In the Studio, make an audio track for each mic and choose its input with the input selector on its track header, or an instrument track for a keyboard.
2. In the Live Show, click **+ Add channel** and pick the track.
3. Turn off **Mute mics** when you're ready to be heard.

Each channel strip has a headphones button, **Pan**, a fader with its meter, and **M** to mute it. **No input** means the track has no input chosen. Its **x** takes the channel off the show; the track stays in the Studio.

While **Mute mics** is on, the channels are silent in the room, the recording and the stream, but their meters still move. Each channel can also duck the music under it (see Dynamic EQ).

## Mixer and Effects

### The mixer
The player has one strip: a headphones button, **Player gain**, and a fader with its meter.

The **Master** strip has the master fader and meter, and a light lit while the limiter works. Meters turn yellow near clipping and red at the top.

### Headphones
Pick where the headphones play in the list by the headphones icon: a Sub output set up in **Song > Song Setup** (**Audio I/O Setup**), such as an audio interface's outputs 3-4. The player's or a channel's headphones button sends it there before its fader. **Mix** sets the cue against the show, **Level** the volume, and **SPLIT** puts the cue left and the show right.

### The effects channel
Click **FX** under the master to open the Effects window; drag it by its top bar. Its tabs follow the sound's order: **Dynamic EQ**, **Compressor**, **Drive**, **Stereo Width** and **Limiter**. The light on each tab (or the **On** switch inside) switches that effect; **Reset...** puts the shown effect's controls back where they start. Nothing delays the sound. The Dynamic EQ and Limiter start on.
- **Dynamic EQ** works on the music only, making room for the voices. **Listens to** shows the live channels it hears; click one to turn it on or off. On its curve the dashed line is the music as it came and the green line the music as heard now. Each of four points has a ring (its Gain, up to +25 dB) and a triangle (its Duck: as low as the voices may take it): drag across for frequency, up or down for level, wheel for width; double-click the ring for 0 dB, the triangle for no ducking. Each point also has **Q**, **Thresh**, **Ratio**, **Attack** and **Release**. The **Level** point turns all the music down by up to its **Depth**, for talking over a bed.
- **Compressor**: Threshold, Ratio, Attack, Release, Knee and Makeup, with its curve and Reduction meter.
- **Drive**: soft saturation, with Drive, Tone (darker or brighter), Mix and Output.
- **Stereo Width**: **Width** (0% mono, 100% as it is, up to 200%), **Bass Mono** (keeps the bass below it in the middle, firm on any PA; off at its lowest) and **Output**. Its goniometer and **Correlation** meter show the stereo: +1 is mono, 0 wide, below 0 (red) out of phase, which thins out in mono.
- **Limiter**: Gain, Ceiling and Release. While it's on, nothing leaves above the Ceiling. Its display shows the last ten seconds.

## Screens and Video

The show's screen shows what the player plays: a video's picture, or a sound-only track's name on black. The next track's picture dissolves in as it crossfades.
- **Screen** on the rail opens or closes it as a small window over every other window, even with Sonify minimised. Drag its bar to move it, its corner to size it. It opens by itself when a video comes.
- **HDMI** puts it full screen on another screen, such as a projector or a TV: tick each one in its menu (**Look for screens again** finds a new one).

Videos show in the library too; their sound plays through the player like any track's.

## Cameras and Streaming

### Stream settings
In Live streaming, click **Stream settings** to open its panel, which previews the picture as it goes out.
- **Size**: 480p, 720p or 1080p. Cameras and the screen are set in the Controller.
- **Title** (top left), **Now playing** (bottom left) and **Logo** (top right; a PNG with a clear background).
- **Destinations**: click **+ Add destination**, pick YouTube, Facebook, Twitch, TikTok or **Other (RTMP)**, and paste the stream key from the service's live dashboard. For Other, type the whole RTMP or RTMPS address, key at the end. Tick each one to use; several go at once.
- TikTok also needs its **Server** URL; its LIVE Center gives both, and the key is new for each live. TikTok, or an Other server marked **9:16**, gets a vertical picture.
- **Keep a copy (MP4)** saves the stream in your recordings folder.
- **Sync**: if lips lag behind the sound, move it later (+) until they match, before going live.

### Going live
1. Click **Go live** (this needs an internet connection); each destination shows its state.
2. Until streaming is enabled, viewers see the show's name and "Back in a moment", in silence; you still hear the show.
3. Press **Enable streaming** to send the picture and sound. A red light shows on the studio's tab, and **LIVE** with the time appears at the top.
4. Click **End stream** when you're done.

You can't change the show's tab while live, or stream or record during an export.

### Recording the night
**REC** on the rail records the show as it's heard. With **Stems** on (choose before recording), the player and each channel also get a file of their own, to mix again in the Studio. Each night goes into a new Live Show folder among your recordings.

**Podcast...** (Live & podcasts) makes the last recording, or one you choose, ready to publish: -16 LUFS for a podcast or -14 LUFS for streams and video, saved as an MP3 beside it.

## Controller and MIDI

### The Controller
Click **Controller** (Live streaming) to open its own window; it keeps working when minimised. Its top bar has **Enable streaming**, the mutes, **Monitor** and the stream's status.
- Each row (**Camera 1** to **Camera 4**, then **Screen**) picks its camera or screen and is **Main** (the whole picture, one at a time), **Custom** (a box over it) or **Hidden**. Every change is an instant cut.
- Drag a Custom box on the preview to move it, its corner to size it (Shift keeps its shape).
- The preview's tabs switch between **Landscape (YouTube, Facebook, Twitch)** and **Vertical (TikTok)**, each with its own boxes; in Vertical, drag across to choose the part of Main that shows.
- A camera's **Sync** delays its picture so a fast camera waits for a slower one. **Look for cameras again** finds a new one.
- **PRESETS**: click one to put it on at once. **Save** updates the preset picked, **Save as...** makes a new one.

**Monitor** shows the picture exactly as viewers see it, in a small window over everything; it's never in the stream.

### MIDI pads
1. Choose your device in the list beside **MIDI**.
2. Click **Learn**, click a button (a row's Main, Custom or Hidden, a preset, Enable streaming, a mute or Monitor), then press a pad.
3. Click **Learn** again when done. While learning, right-click a button to forget its pad.

With **Pad lights** on, pads for what's live light up (on controllers with lights).

### MIDI Link
The player, mixer, channels, effects, Enable streaming, the mutes and REC can be played from a MIDI controller with MIDI Link, the strip at the right of the toolbar: move a control on your device, then drag its chip onto the Live Show control.

## Tips for a Show
- Turn off **Mute mics** and **Mute playback** before you start: both are on whenever Sonify starts.
- Click **Analyze** on your music folder ahead of time, so tempos and keys are ready.
- Keep the **Limiter** on and watch the light by **Master**.
- For streams, click **Go live** early: viewers see your show's name until you press **Enable streaming**.
- Press **REC** with **Stems** on to keep the night, then use **Podcast...** to publish it.

# Sampling Studio

## Overview

The Sampling Studio turns a sound into a sampled instrument. It plays plugins (VSTis, through their effects), a synth or you note by note, records each note until it dies away, and writes WAV files plus an SFZ instrument for the Sonify Sampler.

Open it from its tab in the top bar, with **View > Sampling Studio** or with **Ctrl+7**.

It has three tabs: **Sampling** (what to sample and how), **Loop Editor** and **SFZ to Blob**. The bar above them holds the **Family Name** box (every sample's name starts with it), the green play button (**Start**), the status line, level meters, **F** (opens the folder the samples go to) and a progress line.

The **Sampling** tab has three modes:
- **VSTi Sampling**: the keys you pick, each at its velocity layers, played one at a time.
- **Chord Sampling**: the chord types you pick, played on every root in a key range.
- **Sample Builder**: no recording; an instrument made from audio files and clips put on keys.

## Setting Up

### Source
In the **Source** row, choose what's sampled:
- **Nodes**: plugins you load and wire in the node area under **Clear All Keys** (see Nodes, below).
- **Hardware Synth**: notes go out of **MIDI Out** on channel **Ch**; the synth's sound comes in on the audio track you pick. Monitor the track to hear it.
- **Voice / Played**: you play each note (voice, guitar, anything) into the audio track you pick.

For the last two, set the audio track's input first.

### Nodes
With **Nodes** as the source, the area under **Clear All Keys** is a small node system, as in Sampler Arena, kept inside its borders. A keyboard stands on its left; the **Sampled Keys** and **MIDI Monitor** tabs sit on its right.

- **Sampling**: the studio's notes. Its **MIDI out** (the red connector) wires to the VSTis that play them. **E** opens the Sampler Settings (speed and the rest), **F** the folder the samples go to.
- **MIDI In**: your MIDI keyboards, to hear the VSTis before you sample them. Its list (the button on it) ticks **All inputs** or just some; the keyboard on the left always plays through it. Its **MIDI out** wires to VSTis too, and its light blinks as MIDI comes through. It's heard only while the Sampling Studio is in front, never while sampling, and it's never sampled.
- **Output** (stereo): what's wired into its **L** and **R** is what's sampled, and what you hear.
- A **VSTi** node: an instrument plugin, MIDI in (red) and its sound out as a pair (blue, **L** and **R**). **E** opens its window, **M** mutes it, **X** removes it.
- An **effect** node: sound in as a pair, sound out as a pair. **E**, **M**, **P** (pass through: the sound goes on untouched) and **X**.

The connectors sit on the nodes' sides, inputs on the right and outputs on the left, so the sound runs from right to left. Drag from an output to an input to wire them: drop on a connector to wire that side (**L** to **L**, or crossed), or on the node to wire both sides at once. Two wires into one side add up. Drag a wire's end off an input to move it or let it go; click a wire and press **Delete** to remove it. Drag a node by its body. Double-click a plugin node for its window; right-click for the rest.

The keyboard on the left plays the VSTis wired from the MIDI In node: the further right on a key, the louder; drag along it for a glissando; the wheel scrolls it. Notes coming through the MIDI In light up on it. The computer keyboard plays through it too.

**Sampled Keys** lists the keys picked, with their zones. **MIDI Monitor** shows the last 20 MIDI messages that came in, oldest first: each one's number, channel, what it is (note, controller, pitch bend...) and where it came from. A message the MIDI In node doesn't hear (its input isn't ticked) is greyed. **Clear** empties the list.

A VSTi gets the sampled flag (a white box with a red dot in its title) once the Sampling node's MIDI reaches it **and** its chain is connected to the Output, straight or through effects. Every flagged VSTi is sampled, all together as one sound. **Start** stays off until at least one is flagged. Effects can be chained and shared; a wire that would make a loop isn't made.

Load plugins straight from their folders with the buttons on the area's bar: **VST2**, **VST3** and **CLAP** (and **AU** on a Mac, **LV2** on Linux). Each opens a browse window at that format's usual folder (on Windows, C:\Program Files\Common Files\VST3, C:\Program Files\Common Files\CLAP and C:\Program Files\vstplugins). Double-click folders to go in, **Up** to go back, or type a folder; double-click a plugin (or pick it and click **Load**) to load it. A file holding several plugins asks which one. Once you load from another folder, the window opens there next time. VST2 needs a Sonify built with Steinberg's VST2 SDK; otherwise its button stays off.

The studio starts with no plugins: nothing loads until you load it. The nodes, their places and their plugins' settings are saved in presets.

### Keys and Zones
1. Click keys on the keyboard to pick them (they turn green); drag to pick several. Click a picked key again to drop it.
2. Right-click a key to hear it (through the nodes, or the synth); the lower you click on the key, the louder.
3. The graph above shows each key's zone: the keys its sample will play. Drag the yellow lines to move zone edges.
4. **Clear All Keys** drops them all; the **Sampled Keys** tab beside the nodes lists each zone.

A zone runs from 3 keys below its key up to the next zone. **Don't Use Key Ranges** makes each sample play only its own key.

A picked key has a small **E** button. It opens **Edit Key**, where that key gets its own **Velocity Layers**, **Duration**, **Key Shift** or **Custom Name**; **Reset to Global** undoes each.

### Settings
**Settings** opens the **Sampler Settings** window:
- **Bit Depth**, **Sample Rate** (**Interface** is your audio device's) and **Channels**.
- **Velocity Layers**: low to high (**+** adds, **X** removes). Each is recorded at its top minus **Vel Offset**; a single full layer at **Velocity**.
- **Key Shift**: moves the samples in the instrument, in semitones.
- **Mode: TIME** or **Mode: BARS** with **Duration**: how long each note is held, in ms or in bars (**Time Sig**).
- **BPM Layers**: tempos such as 80, 120, 160; every note is recorded at each, one SFZ per tempo. Blank: once, at the song's tempo.
- **Silence Thresh** and **Silence Dur**: a note's recording ends once it stays this quiet this long.
- **Fade Out on End** and its **Duration** fade each tail.
- **Round Robins**: 2 to 4 takes of each note, played in turn by the instrument.
- **Trim the silence before each attack** and **Normalize together (the loudest at -1 dB)**.
- **Note starts above** (Voice / Played): how loud a note must be to start recording.
- **Create SFZ**: writes the instrument; without it you get only WAVs.
- **Create Legato Loop**: gives every sample a sustain loop.

### Chords
In **Chord Sampling**, click **Chord Types...** and tick any of 18 types. Set **Min Key** and **Max Key** under **Chords Mode Key Range** in Settings. Each type is played on every root whose notes all fit the range. Chords make WAVs only, no SFZ.

### Presets and Folders
In the middle of the line with **VSTi Sampling**, **Save Preset** and **Load Preset** keep and restore the whole setup, and **Set Folder** chooses where samples go (Sonify's Samples folder at first; its tip shows where they go now). Sonify starts the Sampling Studio with fresh settings every time: a setup comes back only when you load its preset. **Ad Hoc Folder** on the Sampling tab sends them elsewhere for this run only, without changing Set Folder. With **Create SFZ**, a folder named after the family holds the SFZ, with the WAVs in a samples folder inside.

## Sampling

1. Set the **Family Name**, source, keys and settings.
2. Click the green play button (**Start**). If files of those names exist, choose **Write over them** or **Cancel**.
3. Follow the status line (the note, and how many of how many). The key playing glows orange; a bar on each key turns yellow, then green when its samples are in.
4. At the end the status shows **Done** and the sample count; notes that made no sound are left out.

While it runs, the play button is **Stop**: the note playing is thrown away, the rest kept. Click it again to **Resume**, or **Discard** the session. **Make Instrument** stops and makes the samples from what's recorded so far.

With **Voice / Played**, the bar says "Play C4 now" with the velocity to aim for. Play and let it ring; Sonify records from where the note starts until it dies away. **Skip** leaves a note out, **Redo** records the last again.

### Speed
**Sampling Speed** in Settings (**i** explains):
- **Real-Time**: recorded through your audio device. Works with everything; use it if unsure.
- **Turbo (~5x)**: about 5 times as fast, kind to most streaming libraries; the nodes are silent meanwhile.
- **Unleashed**: as fast as it goes, for synths and one-shots; sustained notes from disk-streaming libraries may glitch.

A hardware synth and you playing are always sampled in real time. Sampling needs a running audio device, and can't start while the Live Show plays or a mixdown is being made.

When an SFZ was made, **Try It on a New Track** and **Loop Editor** appear.

## Loop Editor

A loop lets a sample sustain while the key is held. The Loop Editor sets loops in any SFZ.

1. Click **Load SFZ Patch** (or **Loop Editor** after sampling).
2. Pick a sample in the list (Up and Down arrows move); a green dot means it loops.
3. Click **Auto Find** for up to 5 candidates, best first. Step with **<** and **>**; **Score** shows how clean each is.
4. **Play** hears the sample looping.
5. Drag **S** and **E** on the waveform to fine-tune.
6. Set the crossfade with **XFade** (20 to 500 ms), or untick **On**.
7. **Remove Loop** takes a loop away.
8. **Save** writes the loops into the SFZ and its WAVs; **Save As** makes a new file. Nothing else in the SFZ changes.

**Batch Find All** loops every sample that has a clean loop and leaves the rest as they were; then **Save**.

### Ask Fanan AI
**Ask Fanan AI** has Fanan AI listen to the finder's candidates (and the current loop) and pick the smoothest, with its crossfade, or decide a sound is better unlooped. Choose **This sample** or **All samples** beside it; its reasons show in the status line, and **Save** writes the result. You must be signed in to your Fanan account, and it uses credits: up to 24 samples per request, each request charged.

> Tip: With **Create Legato Loop** on, new samples arrive looped; check them here.

## Sample Builder

The **Sample Builder** makes an instrument from audio you already have.

1. Click **Sample Builder** and pick keys on the keyboard.
2. Click a key's **E**, then **Sample Slots**.
3. Fill a slot with **Load Sample**, by dragging an audio file from your computer and holding it over the slot a moment, or with **Clip...** (one of the song's audio clips, as trimmed, with its gain and fades).
4. Set each slot's **Velocity** range; an empty slot always waits below for another. **X** removes one.
5. Turn on **Create SFZ** in Settings and click the play button to build.

Each sample plays only its own key and keeps its file's name. Slots on a key with the same velocity range take turns as round robins. **Key Shift**, **Normalize together**, **Create Legato Loop**, **Bit Depth** and **Sample Rate** apply too.

## SFZ to Blob

A blob is a sampled instrument in one file: an SFZ and all its samples, packed together. Samplers made in the Plugins Creator Studio play blobs straight from the disk, so even instruments of many gigabytes need little memory (see **Sample Library** in the Plugins Creator Studio).

1. Open the **SFZ to Blob** tab.
2. Add instruments with **Add SFZ...** (one or more SFZ files), **Add Folder...** (every SFZ in a folder and its folders) or **Add Last Sampled** (the instrument the Sampling Studio made last).
3. Under **Blobs go**, choose **Beside each SFZ** (named after it) or **To one folder** (**Set Folder...** picks it).
4. Under **Samples**, choose **As they are** (16-bit, 24-bit or float, as each file is; Ogg and MP3 become 24-bit), **16-bit**, **24-bit** or **32-bit float**.
5. Click **Make Blobs**. They're made one after another; **Stop** stops the one being made (nothing of it is kept) and the rest wait.

Each row shows its progress, then what it made (regions, samples, size) or why it couldn't. A note in amber says what was left out, such as missing samples. **F** shows the blob in its folder; **x** takes a row off the list, and **Clear List** takes them all.

Any SFZ works: its groups, #include and #define files, default_path, note names and every opcode go in. A WAV's own loop is used when the SFZ gives none. Sounds an SFZ player makes up itself (sample=*sine) can't go in a blob.

## Using the Result

### Try It
**Try It on a New Track** adds an instrument track, named after the family, with a Sonify Sampler playing the instrument, and takes you to the Studio. **Edit > Undo** takes it back.

### Sonify Sampler
Sonify's built-in SFZ player plays any SFZ. To add one yourself, drag **Sonify Sampler** (a System Tool) from the Browser onto an instrument track.

Open its window with the keyboard icon on the track header. At the top:
- **Load SFZ...** opens an instrument, **Latest** the one sampled last, **Clear** empties it.
- **Save SFZ** writes the SFZ again with the sampler's settings in it; **Save As...** saves a copy.
- **Load Preset...** and **Save Preset...** keep settings with the instrument (Sampler Arena's presets open too).

Below are slider groups: **Amp Envelope**, **Filter** (and its type), **Filter Envelope**, **Play** (**Poly** or **Mono**: **Legato**, **Retrigger New**, **Retrigger Stolen**; octave, glide), **Volume**, **Pitch Envelope**, **Pitch**, **Tremolo LFO**, **Filter LFO** and **Vibrato LFO**. Double-click a slider to reset it. Loading an SFZ takes its own envelope, filter and LFO settings.

Harder notes play louder, round robins take turns and the sustain pedal holds notes (Poly). Settings are saved with the song and can be automated.

# Web Browser

## Tabs

The **Web Browser** studio (**Ctrl+8**) puts the internet in tabs inside Sonify while your song plays on: the transport stays at the bottom, so you can play, stop and record as you browse.

- **Open new tab** (or **Ctrl+T** from any studio, or **View > New Web Tab**) opens a new tab page with quick links to YouTube, KVR Audio, Bandcamp and Wikipedia. In the Studio, **Ctrl+T** opens a tab only if your keyboard mapping doesn't use it.
- Click a tab to show it. Its x, a middle-click or **Ctrl+W** closes it; closing the last tab opens a fresh one.
- **Ctrl+PageUp** and **Ctrl+PageDown** go to the previous and next tab.
- A page that opens a new window opens it as a new tab here.

When you go to another studio, your tabs stay as they were. They come back the next time you start Sonify, and sign-ins and cookies are kept between sessions.

## Address and Buttons

Type a web address in the address bar (**Ctrl+L** jumps there), or words to search for with Google, and press **Enter**. **Esc** puts the current address back.

- **Back** (**Alt+Left**) and **Forward** (**Alt+Right**).
- **Reload** (**F5** or **Ctrl+R**); while a page loads, it becomes **Stop**.
- **Home** puts a new tab page in place of the page.

When a page fails, the Web Browser says "This page couldn't load" with the reason.

## Internet Switch

Web pages need the internet. With the top bar's switch on **Internet off**, open pages go blank and new ones can't load. Click the switch back to **Internet on**, and each tab reloads where it was.

# Settings and More

## Settings

**File > Settings...** opens one window with six tabs. Changes apply at once and are kept; click **Close** or press **Esc** when done.

- **Appearance**: **Luminance** sets how dark or bright the arrangement and the Editor's piano roll are, **Contrast** how bright text and symbols are. **Back to the Defaults** resets both.
- **Keyboard Shortcuts**: which DAW's keys Sonify uses.
- **Network**: **Allow remote control apps to discover Sonify** makes Sonify announce itself on your local network, so remote control apps there can find it.
- **Locations > User Data**: where new songs, recordings of unsaved songs, mixdowns and autosaves go (**Browse...**, or **Default** for Documents\Sonify), and **Autosave every** so many **minutes** (1 to 120). An autosave is a copy: your song is saved only when you save it.
- **Locations > Plug-ins**: the Plugin Folders the Browser reads.
- **Audio Setup**: the audio driver and device, sample rate, block size, input channels, **Release audio device in background**, **Audio I/O Setup...** and **Test**.
- **MIDI Setup**: external MIDI devices, control surfaces, MIDI inputs and sync.

The transport's audio device button opens **Audio Setup**; **File > Plugin Folders...** opens **Locations > Plug-ins**.

## Keyboard Shortcuts

Sonify can use the keys of the DAW you know. In **File > Settings... > Keyboard Shortcuts**, choose the **Keyboard mapping**: Studio One (the default), Cubase, Logic Pro, Pro Tools, Ableton Live, Cakewalk or FL Studio. The list shows every command with its keys; where that DAW has no key for a command, there's none here either. You pick a whole mapping: single keys can't be changed. Menus and button tips show the keys in use.

### Studio One's Main Keys

- **Space** start or stop, **Home** return to zero, the number pad's * record, **Num /** loop, **C** metronome.
- **Ctrl+N** new song, **Ctrl+O** open, **Ctrl+S** save, **Ctrl+Alt+S** save as, **Ctrl+E** export mixdown.
- **Ctrl+Z** undo, **Ctrl+Y** redo, **Ctrl+X**, **Ctrl+C**, **Ctrl+V** cut, copy, paste at the playhead, **D** duplicate, **Ctrl+A** select all, **Ctrl+D** deselect all, **Delete**, **Alt+X** split at the playhead, **Q** quantize.
- **F2** Editor, **F3** Console, **F5** Browser, **A** automation.
- **E** zoom in, **W** zoom out, **Alt+Z** the whole song, **Ctrl+0** default zoom; **Shift+E** and **Shift+W** zoom the tracks' height.
- **1** Arrow, **2** Range, **3** Split, **4** Eraser, **5** Paint, **6** Mute, **8** Listen (in the arrangement and the Editor alike), **T** Add Tracks.

### Keys in Every Mapping

- **Ctrl+1** to **Ctrl+9** pick a studio; **Ctrl+Tab** and **Ctrl+Shift+Tab** the next and previous one.
- **Ctrl+T** opens a new web tab (in the Studio, only when the mapping doesn't use it).
- In the Editor, Up and Down transpose the selected notes (with Shift, an octave); Left and Right move them along the grid.

The transport keys, Undo and Redo, and New, Open, Save and Save As work in every studio. No key works while a dialog is open; while you type in a field, only New, Open, Save and Save As do.

## MIDI Devices

**File > Settings... > MIDI Setup** has three parts.

### External Devices

Add your keyboards, controllers, drum kits and instruments to give them names, channels and filters; until then, tracks hear the MIDI ports by their own names.

1. Click **Add...**.
2. Pick a model (type in **Search** to find it), or **New Keyboard**, **New Controller**, **New Drum Kit** or **New Instrument**. Sonify fills in the name and maker, and picks ports named after the model.
3. Check **Device Name**, **Receive From** (the port it sends on) and **Send To** (the port it listens to; optional for a keyboard), set the options and click **OK**.

- **MIDI Channels**: **All**, or the ones it uses.
- **Filter**: Aftertouch, Program Change, Pitch Bend or Controllers ticked here never reach the tracks.
- **Split Channels**: one track input per channel.
- **Default Instrument Input**: new instrument tracks hear this device.
- **Enable MPE** with its **Pitch Range**, for MPE controllers.
- **Send MIDI Clock**, **Use MIDI Clock Start**, **Send MIDI Timecode**: the device follows Sonify's tempo and position while it plays (they need a **Send To** port).

Tracks pick the device by name in their MIDI in list. **Edit...** (or a double-click) changes a device; **Remove** deletes it.

### MIDI Inputs and Sync

**MIDI Inputs** lists every MIDI input, hardware and virtual (loopMIDI cables too), with its kind of device. Sonify listens to the ticked ones; a new one is ticked as it appears. "Can't open it: another app is using it" means another program holds the port. Under **Sync**, Sonify is the master; **MIDI timecode frames** sets the timecode rate (24, 25, 29.97 drop frame or 30 fps).

## Control Surfaces

A control surface plays Sonify itself: Mackie Control, its Extender, HUI, and surfaces that speak them (Behringer X-Touch, iCON, PreSonus FaderPort and more).

1. In **File > Settings... > MIDI Setup**, click **Add...**.
2. Pick your surface's model, or **New Control Surface** and its **Protocol**: **Mackie Control**, **Mackie Control Extender** or **HUI**.
3. Choose both ports, **Receive From** and **Send To**, and click **OK**.
4. Set the surface to its Mackie Control (MCU) or HUI mode.

Its strips are the Console's channels, tracks then FX channels, eight to a surface. Faders set volume, knobs pan (or a send's level, with Send), and strip buttons record-arm, solo, mute and select; the master fader is Main's, and the transport, jog wheel, Undo and Save work too. Motor faders, lights, names and meters follow Sonify. Surfaces take channels in the device list's order, so add the left one first; the bank buttons move them all. A surface's port never plays tracks. Problems, such as a missing port, show in red under the list.

### Pad Grids (Clip Launcher)

A Novation Launchpad (X, Mini MK3, Pro MK3), an AKAI APC mini or an APC mini MK2 can be Song Builder Blocks' Live grid. Add it as above and pick its **(Clip Launcher)** model, or **New Control Surface** with the Protocol **Launchpad (Clip Launcher)**, **APC mini (Clip Launcher)** or **APC mini MK2 (Clip Launcher)**. Choose both ports; for a Launchpad its MIDI ports, not its DAW ports (on Windows, the second ones: MIDIIN2 / MIDIOUT2). Sonify puts it in Programmer mode.
- The 8 x 8 pads are the grid: a row for each of the first eight blocks, a column for each of the first eight tracks. A pad launches that block on that track.
- The side buttons launch whole blocks. A Launchpad's top row, or the buttons under an APC's pads, stop tracks; **Stop All** is the Launchpad's lowest side button, or **Shift** on an APC.
- The lights follow: a cell with a clip lit in its block's colour (yellow on the first APC mini), playing pulsing (steady green on the first APC mini), waiting to start flashing; a track waiting to stop flashes.
- It works while Live is on, and stays dark while it's off.

## MIDI Link

MIDI Link lets a knob, slider, wheel, pad or key on any MIDI device move a control in Sonify. Its strip is at the right of the toolbar, in the Studio and the Analog Board.

1. Move a knob, slider or wheel on your device, or hit a pad: it shows as a chip in the strip.
2. Drag the chip onto a control, or tap the chip, then click the control.
3. In the small window by the control, choose its type: **Knob** (faders and knobs), **Toggle** (each press switches it), **Momentary** (on while held) or **Trigger** (each press does it once, as the launcher's pads). **Clear** unlinks the control, **Cancel** puts the links back as they were, x closes the window.

### MIDI Learn

Click **Learn** in the strip: every control that can be linked gets a dashed outline. Click a control (it blinks), then move what should move it on your device; the link is made and its small window opens. Click the next control, as many as you like. **Learn** again or **Esc** stops.

### The MIDI Link window

Click **MIDI Link** in the strip (or right-click the strip > **MIDI Link Window**). It holds:
- **Plugin knob**: touch a knob in a plugin's window, then link to this slot.
- **Focus**: **Volume**, **Pan**, **Mute**, **Solo**, **Record** and eight knobs that always move the selected track's controls, whichever track is selected. The knobs are the first eight knobs of its instrument (or of its first effect that has some); on a Node Rack, its eight Macros.
- Every link: what moves it, what it's linked to (click to see the control) and its type; the bin removes one. **Save Map...**, **Load Map...** and **Clear All** at the bottom.

You can link track and channel faders, pans, sends, Mute, Solo, Record and Monitor, Main's fader and Mute, the transport buttons, Song Builder Blocks' Live grid, and the Analog Board's and Live Show Studio's controls. A key or pad can't move a fader or knob.

Links belong to your setup, not the song: they work in every song, by track number, and a linked control's MIDI no longer reaches the tracks. Right-click the strip to **Save MIDI Link Map...**, **Load MIDI Link Map...** or **Clear All Links**.

## Phone and Voice

**Phone remote** (in the AI Assistant) lets you text Fanan AI from your phone. Fanan AI can do everything there that it does when you type in Sonify. Turn it on, then:
- **In the studio:** scan the QR code it shows with the phone's camera. The phone must be on the same Wi-Fi as the computer. No sign-in needed. The phone stays paired until Sonify closes: each time Sonify starts it makes a new code, so scan again then (a visitor's phone, or a QR code seen in a screenshot, stops working too). **New pairing code** unpairs every phone at once; scan again after it. The first time, Windows may ask whether Sonify can use the network: allow private networks.
- **Away:** open the address it shows on the phone and sign in with the same Fanan account.

On the phone, **CHAT** shows the conversation, and a line under your last message says where it is: Message sent, Message arrived, Sonify is generating an answer, Message received. A chime plays when the answer arrives (the bell at the top turns it off). **Stop** stops Fanan AI. Save a text you send often as a shortcut: hold your message down and pick **Add to shortcuts** (or use the ☆, or **+ New shortcut**). It becomes a button on **SHORTCUTS**: tap it, say **Yes**, and it's sent. Hold a shortcut down to **Send**, **Edit** or **Delete** it (✕ deletes it too). Shortcuts are kept in Sonify for next time, so every phone has the same ones.

**Voice control** (from the recording room, say "Sonify", what you want, then "over") is switched off in this version of Sonify, so its row doesn't show.

## Troubleshooting

### No Sound

- In **File > Settings... > Audio Setup** a device must be chosen; problems show in red there. Click **Test** for a test tone.
- In **Song > Song Setup... > Audio I/O Setup > Outputs**, check where Main plays.
- Check the tracks' Mute and Solo and the transport's **Main** meter.
- While the Live Show Studio shows or plays, it has the outputs and the song stays stopped; its **Mute mics** and **Mute playback** are on when Sonify starts.
- With **Mix on the board** on, the Analog Board mixes the song instead of the Console.
- With **Release audio device in background** on, Sonify lets the device go while another app is in front.

### No MIDI

- The transport's **MIDI** light shows whether notes arrive.
- In **MIDI Setup > MIDI Inputs**, the port must be ticked and free.
- The track must hear the device (its MIDI in button) and have **Monitor** or **Record arm** on.

### A Plugin Is Missing

- If you chose No at startup, restart Sonify and choose Yes.
- Check **File > Plugin Folders...**, then **File > Rescan Plugins**.
- Plugins under **Couldn't be read** failed to load; a plugin whose file is gone shows greyed out until it's back.
- A song missing plugins says "Some of the song couldn't be loaded" as it opens.
- WCLAP plugin windows stay blank while the internet is off.

### Fanan AI Doesn't Answer

The AI Assistant says why, above its box, with a button: the internet is blocked (switch it to **Internet on**), there's no internet or Fanan's server isn't responding (**Try again**), you're not signed in (**Sign in**), or you're out of credits (**Buy credits**).

### Pages Don't Load

With **Internet off**, web pages, Fanan AI and signing in can't reach the network. Click the switch to turn it back on; open web tabs reload where they were.
