# NPEG for VLC

### Bringing a forgotten Amiga video codec back to life

**NPEG** is a video format developed by **Noster** of the Amiga demoscene group **Venture** for the 1998 production *Video Meliora Proboque, Deteriora Sequor*. This project reconstructs the decoder from surviving Amiga binaries and original `.npeg` files, and makes the format playable in **VLC 3.x** on Linux.

This is a **native VLC demux plugin**, not a wrapper around an MP4 file. The frames are decoded from the original NPEG bitstream and handed to VLC as RGB24 video. A standalone `npeg-convert` utility can also export the video to MP4.

> **Project status:** The standalone decoder has been exercised against both preserved NPEG films, and the plugin logic has been tested using a mock VLC host. A successful build against the actual VLC headers is the next step; playback in a real VLC installation has not yet been independently verified for this release. This is a preservation and reverse-engineering project, not an official VideoLAN component.

## The story

In the late 1990s, the Amiga demoscene was still producing ambitious audiovisual work on machines whose memory bandwidth, CPU power, storage and graphics architecture imposed very real limits. Demoscene developers routinely wrote their own playback engines, compression schemes and timing code to make the hardware do something it was never quite supposed to do.

Venture's *Video Meliora Proboque, Deteriora Sequor* belongs to that world. It was released at **Satellite 1998** in November 1998, where it placed **first in the Amiga demo competition**, according to the [Pouët production entry](https://www.pouet.net/prod.php?which=78531). The production was presented as an Amiga AGA demo, although it is also classified as *wild* in the Pouët database. The preserved AmigaOS executable identifies itself with the version string:

```text
$VER: VIDEO_MELIORA by Venture (8.11.98).
```

The string establishes a date associated with the executable (**8 November 1998**), not necessarily its original public-release date. Its surviving code refers to `mpega.library`, `cybergraphics.library`, an NPEG replay process and related playback routines. In other words, this was more than an animation pasted into a standard picture viewer: it had a purpose-built multimedia playback path.

**Noster** wrote the NPEG codec and its Amiga playback engine. The name playfully combines **Noster** with **MPEG**, but NPEG is **not an MPEG-compliant file format**. The Venture production also involved **Korbatz** (creative direction and lyrics), **Noster** (lyrics, codec and player), **Wierza** (music), and **Smajk** (production contributor and uploader of the archived distribution). These credits draw on firsthand recollections and the surviving release notes; they are not intended as an exhaustive original credit roll.

The original release notes thanked **Bax** for allowing the band to record audio in his studio, **Steff** for lending a bass guitar, and **Skoczek** for lending a guitar. The surviving documentation conveys the spirit of a production that mixed custom software, live-action footage, recorded music and an unmistakably irreverent sense of humour.

The Latin title comes from Ovid's *Metamorphoses* (Book VII): *“I see the better course and approve it, but follow the worse.”* An appropriately mischievous title for a garage-born demoscene production.

There is a small preservation story here too: the original files survived for decades, but their dedicated Amiga player did not follow them to modern operating systems. This project aims to change that—without requiring an Amiga emulator or a previously converted reference recording.

## Original Amiga releases and hardware requirements

The [surviving release notes](https://www.pouet.net/prod_nfo.php?font=4&which=78531), uploaded under **Smajk**'s address (`smajk@man.szczecin.pl`), describe a post-party revision that differed slightly from the version shown at Satellite 1998. A separate patch was distributed for owners of the party release: its installation instructions told users to copy the patch files into the original demo directory and run an `update` script. The revision also included a dedicated **low-end version**.

The original Amiga-side requirements, as documented by Venture, were:

- An **Amiga with AGA** graphics or a **CyberGraphX (CGX)**-compatible graphics environment.
- **XPK**, **asyncio** and **mpega** libraries, plus the **AHI** audio system.
- A powerful **Motorola 68k CPU**; the documentation explicitly favored the fastest 68k processor available.
- Some familiarity with the Amiga demoscene—listed, tongue-in-cheek, as a requirement in its own right.

### The 68060 and the low-end edition

The original release notes are unusually candid about performance: *Video Meliora* was computationally demanding, and Venture created a special reduced version for machines **without a 68060**. It used **lower film quality**, was **scaled down** and omitted some functionality, including **English subtitles**. This was not simply a case of choosing a slower FPS at playback: the distributed editions genuinely differed in features and visual quality.

The low-end player offered more display modes. Noster recommended **Super72 at 200 × 150**, with additional extra-low-resolution modes available at the expense of appearance. The player attempted to center its output at higher resolutions, but did not apply horizontal centering in CGX modes: Noster noted that he was unsure whether CyberGraphX supported the required screen movement.

These facts are important when discussing the codec's design. NPEG was resource-conscious, but the full production was **not optimized for an unexpanded A500 or a stock A1200**. The 68060 was the performance target of the richer edition; compatibility with weaker CPUs was addressed by releasing a separate, reduced version. A recollection from the production circle also confirms that Noster owned an **A1200 with a 68060 accelerator** toward the end of that period. The release notes, however, do not establish how many CPU-specific decoder implementations were compiled internally.

### PowerUP accelerator incompatibility

The original player was reported not to work with certain **PowerUP PPC accelerator boards**, even though its code was intended for **m68k** execution. Venture advised disabling the PowerUP board to watch the demo; according to the release notes, the party organizers managed this without physically removing the card. Noster did not own a PPC setup for debugging and suspected the problem could lie in AmigaDOS **process creation or message-port initialization**. That is his historical hypothesis, not a confirmed root cause.

This compatibility problem belongs to the **original 1998 Amiga player**, not to the modern Linux/VLC plugin described below.


## What survived

| Artifact | What it tells us |
| --- | --- |
| `video_meliora` | Original AmigaOS Hunk executable, with symbols and playback routines |
| `video.npeg` | Main NPEG video, **160 × 88**, **1,344 frames** |
| `prep2.npeg` | Additional NPEG animation, **160 × 88**, **144 frames** |
| `muza.mp2` (or `muza(1).mp2`) | Separate MPEG Layer II audio track |
| `tekst.xpk`, `doodie_friend.xpk` | Other original Venture assets compressed with XPK/HFMN |

Both preserved videos total **1,488 encoded frames**. The main NPEG stream does not contain its accompanying MP2 sound. The preserved reference recording suggests **7.5 frames per second**; this frame rate is a playback default, **not a confirmed property of the NPEG header**. The 160 × 88 sample dimensions should likewise **not** be treated as proof that every historically distributed, high-quality or low-end NPEG asset used the same size.

## How NPEG works

### Container layout

NPEG uses an **IFF-like** binary container: a `FORM` header with a form type of `NPEG`, followed by named chunks. Integers are stored in **big-endian** order, as one would expect from its Motorola 68k heritage.

```text
FORM  ....  NPEG
  NPHD  video metadata (14 bytes in the surviving files)
  NPAL  optional 256-colour RGB palette (768 bytes)
  NTRE  Huffman decoding tree
  NIFR  compressed frame 0
  NIFR  compressed frame 1
  ...
```

The reconstructed reader deliberately **does not apply ordinary IFF even-byte padding between chunks**: the preserved NPEG streams place subsequent chunks immediately after odd-length payloads. A generic IFF reader that always aligns to an even byte can therefore lose synchronization.

### Frame decoding pipeline

```text
NIFR payload
    |
    v
Run-length decoding (RLE)
    |
    v
Huffman decoding (NTRE tree)
    |
    v
Signed 8 x 8 DCT coefficients
    |
    v
Dequantization + inverse DCT
    |
    v
8-bit pixel values
    |
    +-- NPAL present --> palette RGB lookup
    |
    +-- no palette ----> grayscale RGB
    |
    v
RGB24 frame --> VLC
```

**1. RLE first.** Each `NIFR` starts with a big-endian 32-bit value giving the expected number of bytes after the RLE stage. The next bytes use a compact control scheme: a control byte with its high bit set repeats the following byte `(control & 0x7f) + 1` times; otherwise the same number of literal bytes are copied.

**2. Huffman next.** `NTRE` consists of six-byte entries: a 16-bit symbol and two 16-bit child indices. Internal nodes are marked `0xffff`; the last entry is the root. The bitstream is read most-significant-bit first from big-endian 16-bit words. The current implementation produces one signed coefficient byte per image sample location.

**3. Block-based image reconstruction.** The coefficients are arranged in **8 × 8 blocks**. At **160 × 88**, this means **20 × 11 = 220 blocks per frame**. The decoder dequantizes them and performs a two-dimensional inverse discrete cosine transform (IDCT), then clamps and rounds the reconstructed intensity values.

**4. Palette or grayscale.** `prep2.npeg` contains an `NPAL` chunk with 256 RGB triplets; `video.npeg` does not. The current reconstruction renders the former using its palette and the latter as grayscale.

### The quantization-table curiosity

One especially recognizable artifact in the original executable is a **64-element quantization table** at approximately offset `0x304c` in the first code section. Its values are the familiar **JPEG luminance quantization matrix multiplied by ten**. The decoded values used here are:

```text
16  11  10  16  24  40  51  61
12  12  14  19  26  58  60  55
14  13  16  24  40  57  69  56
14  17  22  29  51  87  80  62
18  22  37  56  68 109 103  77
24  35  55  64  81 104 113  92
49  64  78  87 103 121 120 101
72  92  95  98 112 100 103  99
```

That is a neat piece of engineering archaeology: NPEG's reconstructed video path is **DCT-based**, but it is **not JPEG or MPEG**, and ordinary JPEG/MPEG decoders cannot consume its bitstream. A recognizable mathematical building block does not imply compatibility with a standard container or codec.

### What is confirmed, and what remains reconstruction

The chunk identifiers, original data, tree representation and executable symbols are observable in the preserved material. The RLE/Huffman/IDCT pipeline is implemented and works on the two available samples. However, the complete original Noster source code is unavailable, and we cannot claim a byte-for-byte faithful reimplementation of every arithmetic or rounding detail. Other NPEG encoders or variants are not yet covered by a compatibility test suite.

The standalone decoder treats each available `NIFR` as independently decodable. That is what the preserved samples and current implementation support; it should not be generalized into a claim about every possible version of NPEG.

## Build on Fedora Linux

### Requirements

You need **native RPM packages of VLC 3.x** and matching development headers, a C compiler and Make. The optional MP4 exporter additionally uses `ffmpeg` and an available H.264 encoder.

```bash
sudo dnf install gcc make pkgconf-pkg-config vlc vlc-devel
```

The relevant Fedora development package is [`vlc-devel`](https://packages.fedoraproject.org/pkgs/vlc/vlc-devel/). Confirm that `pkg-config` can find the plugin SDK:

```bash
vlc --version | head -n 1
pkg-config --modversion vlc-plugin
```

For the following commands, run them in the directory containing this `README.md` and the `Makefile`.

### Compile

```bash
make clean
make
```

This builds:

- `libnpeg_plugin.so` — VLC 3.x demux module using the native decoder;
- `npeg-convert` — command-line frame/MP4 exporter.

The module is compiled against the VLC plugin headers on **your own system**. Do not copy the `.so` from a different VLC build or distribution.

### Run the tests

```bash
make test
```

The tests exercise the standalone decoder and a **mock VLC integration**, using `samples/video.npeg` and `samples/prep2.npeg`. Passing these tests **does not by itself prove the module loads in desktop VLC**.

### Install for your user

```bash
make install-user
```

This places the module at:

```text
~/.local/lib/vlc/plugins/demux/libnpeg_plugin.so
```

No files in the system VLC installation are overwritten. Start VLC with the additional plugin search path:

```bash
VLC_PLUGIN_PATH="$HOME/.local/lib/vlc/plugins" \
  vlc --demux=npeg samples/video.npeg
```

Or open the secondary animation:

```bash
VLC_PLUGIN_PATH="$HOME/.local/lib/vlc/plugins" \
  vlc --demux=npeg samples/prep2.npeg
```

The `--demux=npeg` switch explicitly selects the plugin. Once automatic discovery is known to work on your installation, you can experiment with omitting it.

### Frame rate and external audio

NPEG does not embed the separate MP2 soundtrack. To change playback speed:

```bash
VLC_PLUGIN_PATH="$HOME/.local/lib/vlc/plugins" \
  vlc --demux=npeg --npeg-fps=7.5 samples/video.npeg
```

To attach a separate audio source (synchronization is not yet guaranteed):

```bash
VLC_PLUGIN_PATH="$HOME/.local/lib/vlc/plugins" \
  vlc --demux=npeg samples/video.npeg \
  :input-slave=/absolute/path/to/muza.mp2
```

### Convert NPEG to MP4

The standalone converter actually decodes the **NPEG frames**. FFmpeg is used only to encode the output movie, not to decode the original NPEG.

```bash
sudo dnf install ffmpeg
./npeg-convert samples/video.npeg \
  -o video-meliora.mp4 --fps 7.5 --scale 4
```

To add the original audio:

```bash
./npeg-convert samples/video.npeg \
  -o video-meliora.mp4 --fps 7.5 --scale 4 \
  --audio /absolute/path/to/muza.mp2
```

To inspect a file or export its first frame without FFmpeg:

```bash
./npeg-convert samples/prep2.npeg --info
./npeg-convert samples/prep2.npeg --frame 0 --ppm first-frame.ppm
```

### Troubleshooting

If the library builds but the player cannot open the NPEG file, capture detailed VLC output:

```bash
VLC_PLUGIN_PATH="$HOME/.local/lib/vlc/plugins" \
  vlc -vvv --demux=npeg samples/video.npeg 2>&1 | tee vlc-npeg.log
```

Check that the running VLC binary is the native RPM build, that `vlc-devel` matches it, and that the module exists at the installed path. **VLC 4.x, Flatpak and Snap packages are not supported by this build procedure.**

To uninstall the user-local plugin:

```bash
rm -f "$HOME/.local/lib/vlc/plugins/demux/libnpeg_plugin.so"
```

## Project structure

```text
src/npeg.c                NPEG parsing, RLE, Huffman, IDCT, RGB conversion
src/npeg.h                Public decoder interface
src/vlc_npeg.c            VLC demux plugin, timing, seeking, RGB frame delivery
src/npeg-convert.c        Optional MP4 and PPM exporter
scripts/install-user.sh   User-local installation
samples/                  Preserved original NPEG test files
tests/                    Decoder smoke tests and a mock VLC host
Makefile                  Build and test targets
```

## Limitations and future work

- **VLC ABI:** This plugin targets VLC **3.x**, and must be compiled against the local VLC SDK; real-VLC loading still requires verification.
- **Timing:** 7.5 FPS comes from the preserved reference recording. Native historical synchronization is not fully reconstructed.
- **Audio:** MP2 is external; automatic soundtrack discovery and exact sync are not implemented.
- **Memory:** The plugin reads the entire file into memory for indexing and seeking (with a **256 MiB file-size limit**) and the decoder keeps an additional copy.
- **Compatibility:** Validation currently covers only the surviving `video.npeg` and `prep2.npeg` samples, not every possible NPEG variant.
- **Math:** The reconstructed inverse transform uses floating-point arithmetic; it is not guaranteed to reproduce the original m68k arithmetic or pixel rounding exactly.
- **Security:** This is a preservation prototype. Do not treat it as a security-audited parser for arbitrary untrusted media.

A useful next step is real-world VLC playback validation, followed by exact frame comparisons against the original Amiga output, improved soundtrack synchronization, fuzz testing, and eventually an FFmpeg decoder or a standalone format specification.

## Credits and provenance

- **Noster / Venture** — original NPEG codec and Amiga playback engine (1998); lyrics co-written with Korbatz.
- **Korbatz** — Venture creative contributor, art direction and lyrics.
- **Wierza** — music.
- **Smajk** — contributed to the Venture production and uploaded the preserved release/archive documentation.
- **Bax** — access to the studio used for recording.
- **Steff** — loan of a bass guitar.
- **Skoczek** — loan of a guitar.
- **This project** — modern reverse-engineered reconstruction of the format and a VLC playback module, based on preserved files and disassembly. This is an independent effort, not an official continuation of the original project.

Historical evidence includes the original executable's `$VER` string, its remaining playback symbols, the preserved sample files, Venture's archived NFO and firsthand recollections from a participant. The NFO describes a **patch for the party version**, a revised release and a separate **low-end build**; these historical versions should not be conflated with the modern VLC module. Additional references:

- [Pouët: *Video Meliora* by Venture — Satellite 1998, first place](https://www.pouet.net/prod.php?which=78531)
- [Archived Venture release notes (NFO)](https://www.pouet.net/prod_nfo.php?which=78531)
- [VLC development package for Fedora](https://packages.fedoraproject.org/pkgs/vlc/vlc-devel/)
- [Ovid's Latin line and translation](https://pl.wikisource.org/wiki/M._Arcta_S%C5%82owniczek_wyraz%C3%B3w_obcych/Video_meliora,_proboque,_deteriora_sequor)

**Rights and licensing:** Do not assume the original Venture work or included sample media are public domain. No license for the original samples is asserted here, and this README does not grant redistribution rights. Determine an appropriate license for the modern source separately with its maintainers.

---

*Some software survives because it was popular. Some survives because somebody kept the disks. NPEG belongs to the second category.*
