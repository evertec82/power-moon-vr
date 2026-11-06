# Third-party components

The complete Windows package includes the base runtime dependencies and first-run tools. See the packaged licenses/ directory for the notices below. No game data or generated game library is included.

- SDL3: zlib license, SDL3-zlib.txt; https://github.com/libsdl-org/SDL
- FFmpeg 9.0.2: unmodified LGPL-only H.264/AAC/VP8/VP9/Vorbis/Opus build, FFmpeg-LGPL-2.1.txt. DLLs: avcodec-63, avformat-63, avutil-61, swscale-10. Build configuration: tools/build_deps_windows.sh.
- GNU libiconv 1.19: LGPL 2.1, libiconv-LGPL-2.1.txt.
- GNU compiler runtime: GCC-runtime-library-exception.txt and GPL-3.0.txt; winpthreads.txt.
- shaderc and OpenXR loader: Apache 2.0; Apache-2.0.txt and OpenXR.txt.
- Dear ImGui, Vulkan Memory Allocator, SMAA, AMD FSR1 and Ryujinx-derived code: retained MIT notices in licenses/ and third_party/.
- Roboto font: Apache 2.0, Roboto.txt. It is not a game font.
- Embedded Python 3.12.10: PSF license, Python-PSF.txt. Capstone 5.0.7: BSD, Capstone-BSD.txt. python-lz4 4.4.5: BSD, python-lz4-BSD.txt.
- Unicorn 2.1.4: GPL 2.0, GPL-2.0.txt; used by the first-run generator.
- Zig 0.17.0: MIT, Zig-MIT.txt, with LLVM exceptions and toolchain notices retained in zig/lib/.

FFmpeg, libiconv and Unicorn source archives are supplied in PowerMoonVR-v0.2.1-dependency-sources.zip on the same release page, with source hashes and FFmpeg build instructions. They are unmodified third-party sources, not game code. Upstream sources: https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz , https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.19.tar.gz , https://github.com/unicorn-engine/unicorn/tree/2.1.4 . You may replace the dynamically linked libraries with compatible builds.

Windows system components and the Vulkan graphics-driver loader are supplied by Windows and the graphics driver and are not packaged.
