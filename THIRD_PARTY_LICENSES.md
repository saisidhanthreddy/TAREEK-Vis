# Third-Party Licenses

TAREEK-Vis uses the following third-party libraries and tools:

---

## Qt6

- **License**: LGPL v3.0
- **Website**: https://www.qt.io/
- **Components used**: Widgets, OpenGL, OpenGLWidgets, Concurrent, Network
- **Usage**: Dynamically linked

The Qt Toolkit is Copyright (C) The Qt Company Ltd. and other contributors.
Qt is available under the LGPL v3.0 license. See https://www.gnu.org/licenses/lgpl-3.0.html

---

## PugiXML

- **License**: MIT
- **Version**: 1.15
- **Website**: https://pugixml.org/
- **Copyright**: Copyright (C) 2006-2026 Arseny Kapoulkine

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files, to deal in the Software
without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the above copyright notice and this permission notice.

---

## ZLIB

- **License**: Zlib License
- **Website**: https://www.zlib.net/
- **Copyright**: Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software. Permission is granted to anyone to use this software
for any purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented.
2. Altered source versions must be plainly marked as such.
3. This notice may not be removed or altered from any source distribution.

---

## FFmpeg (Optional)

- **License**: LGPL 2.1+ or GPL (depending on build configuration)
- **Website**: https://ffmpeg.org/
- **Usage**: Invoked as external subprocess for video recording (not linked)

FFmpeg is a trademark of Fabrice Bellard. If FFmpeg is bundled with this
application, its license terms apply to the FFmpeg binary only, not to TAREEK-Vis.
Prebuilt releases bundle the gyan.dev "essentials" build, which includes
GPL-licensed components (e.g. libx264); this is compatible with TAREEK-Vis's
own GPLv3 license. An LGPL-only FFmpeg build may be substituted if a less
restrictive redistribution is needed.

---

## OpenGL

- **Usage**: Graphics rendering via system-provided drivers
- **No licensing restrictions** apply to the use of the OpenGL API.
