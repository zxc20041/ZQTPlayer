# Third-Party Notices

ZQTPlayer uses the following third-party open source software.

---

## FFmpeg

- **Website:** <https://ffmpeg.org/>
- **Version:** master-latest (BtbN LGPL-shared pre-built)
- **License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1-or-later)
- **Used libraries:** libavformat, libavcodec, libavutil, libswscale, libswresample
- **Usage:** Dynamic linking (shared libraries)

FFmpeg is a collection of libraries and tools to process multimedia content
such as audio, video, subtitles, and related metadata.

> This software uses libraries from the FFmpeg project under the LGPL v2.1.
> See [LICENSES/LGPL-2.1.txt](LICENSES/LGPL-2.1.txt) for the full license text.

FFmpeg source code is available at: <https://git.ffmpeg.org/ffmpeg.git>

---

## Qt 6

- **Website:** <https://www.qt.io/>
- **Version:** 6.10.2
- **License:** GNU Lesser General Public License v3.0 (LGPL-3.0-only)
- **Used modules:** Qt Quick, Qt Quick Controls 2, Qt Multimedia, Qt Linguist Tools
- **Usage:** Dynamic linking (shared libraries)

Qt is a cross-platform application development framework for desktop, embedded,
and mobile platforms.

> This software uses the Qt framework under the LGPL v3.0.
> See [LICENSES/LGPL-3.0.txt](LICENSES/LGPL-3.0.txt) for the full license text.

Qt source code is available at: <https://code.qt.io/>

---

## Compliance Notes

- Both FFmpeg and Qt are used via **dynamic linking** (shared libraries), which
  is compliant with the LGPL requirements.
- Users may obtain the source code of these libraries from the URLs listed above,
  and may re-link the application against modified versions of these libraries.
- No modifications have been made to the FFmpeg or Qt source code.

### Additional Distribution Constraints

- If FFmpeg is rebuilt with **GPL-only** options/components (for example certain
  external codecs), the resulting distribution may be subject to GPL obligations.
- If switching from dynamic linking to **static linking**, additional LGPL
  obligations apply (for example, providing relinkable object files or another
  compliant replacement mechanism).
- If third-party source code is modified, keep change notices and provide
  corresponding source for modified portions as required by the applicable license.

### Release Checklist

Before external release, confirm:

- FFmpeg build flags do not unintentionally enable GPL-only paths.
- License texts are packaged: `LICENSES/LGPL-2.1.txt`, `LICENSES/LGPL-3.0.txt`.
- Third-party notice file is included in installer/package/about page.
- Upstream source locations remain valid and publicly accessible.
- If any local patch exists, patch summary and source access path are documented.

### Legal Notice

This file is an engineering-oriented compliance summary and does not constitute
legal advice. For commercial/public release, obtain legal review.
