# Helvetia — Master Build Tracker (Updated)

**Legend:** `[ ]` not started · `[🚧]` in progress · `[✅]` shipped · `[⛔]` blocked
**Type codes:** FP = FileProcessor · GN = Generator · CV = Converter · LK = Lookup · VE = Viewer/Editor · CA = Calculator

---

## 📊 Overall Progress

| Module | Tools | Done | Progress |
|---|---|---|---|
| Security & Crypto | 43 | 0 | ░░░░░░░░░░ 0% |
| PDF | 61 | 3 | █░░░░░░░░░ 5% |
| Image | 97 | 0 | ░░░░░░░░░░ 0% |
| Video | 34 | 0 | ░░░░░░░░░░ 0% |
| Audio | 19 | 0 | ░░░░░░░░░░ 0% |
| Document | 29 | 0 | ░░░░░░░░░░ 0% |
| DevTools / Text / Encoding | 96 | 0 | ░░░░░░░░░░ 0% |
| Calculators & Unit Converters | 65 | 0 | ░░░░░░░░░░ 0% |
| Archive | 10 | 0 | ░░░░░░░░░░ 0% |
| Subtitle | 3 | 0 | ░░░░░░░░░░ 0% |
| Utility | 6 | 0 | ░░░░░░░░░░ 0% |
| Misc | 28 | 0 | ░░░░░░░░░░ 0% |
| **TOTAL** | **491** | **3** | **█░░░░░░░░░ 0.6%** |

## 🏗️ Infrastructure Progress

| Component | Status | Notes |
|---|---|---|
| QPDF C++ wrapper | ✅ Shipped | `src/modules/pdf/backend/qpdf_wrapper.{h,cpp}` — merge, split, rotate, compress, encrypt, decrypt, page count, metadata |
| QPDF test harness | ✅ Shipped | `tests/qpdf_wrapper_test.c` — CLI verification |
| SQLite DB layer | ✅ Shipped | `src/core/db/` — schema, migrations, vault meta, notes, favorites |
| Vault crypto (libsodium) | 🚧 In progress | Argon2id + XChaCha20-Poly1305 envelope encryption |
| AdwApplicationWindow migration | ✅ Shipped | `AdwToolbarView` + `AdwHeaderBar` + `AdwToastOverlay` + `AdwNavigationSplitView` |
| Action system | ✅ Shipped | `win.open_tool`, `win.close_tool`, `win.tool_action` dispatcher |
| Accelerators | ✅ Shipped | `Ctrl+O`, `Ctrl+S`, `Ctrl+Q`, `Ctrl+W`, `Ctrl+E`, `Escape` |
| Tool registry | ✅ Shipped | `HelvetiaTool` + `HelvetiaToolCommand[]` per tool |
| Module registry | ✅ Shipped | 8 modules registered with sidebar icons |
| Six UI templates | [ ] Not started | FileProcessor, Generator, Converter, Lookup, Viewer/Editor, Calculator |
| CSS design tokens | 🚧 In progress | `theme.css` with `.tooly-tool-card`, `.heading`, `.navigation-sidebar` |
| Flathub manifest | [ ] Not started | Needed before v1.0 |
| AppStream metadata | [ ] Not started | `org.helvetia.Helvetia.metainfo.xml` |
| User documentation | [ ] Not started | Per-tool docs, getting-started guide |

---

## 🔐 Module 1 — Security & Crypto

### [ ] Module complete (43 tools)

**Encryption**
- [ ] File Encrypt — FP — Drop file, password + confirm, algorithm, key file → `.helv`
- [ ] File Decrypt — FP — Drop `.helv`, password → original file
- [ ] Folder Encrypt — FP — Drop folder, options, password → `.helv` folder archive
- [ ] Text Encrypt — FP — Text area + password → base64 ciphertext
- [ ] PGP Encrypt / Decrypt — FP — Mode toggle, recipient key or private key
- [ ] Encrypted Archive — FP — Files/folder, password → 7z/ZIP AES-256
- [ ] Archive Password — FP — Add/remove/change password on archives

**Hashing**
- [ ] Hash Generator — GN — Text + algorithm → live hash, copy
- [ ] Hash Tool — FP — File + algorithm → hash
- [ ] Hash Verifier — FP — File + expected hash → pass/fail
- [ ] File Hash Calculator — FP — Files + algorithm → hash table
- [ ] Text Hash Calculator — CV — Text + algorithm → live hash
- [ ] File Checksum — FP — File + algorithm → checksum
- [ ] Checksum File Creator — FP — Files/folder → SHA256SUMS file
- [ ] Checksum File Verifier — FP — Checksum file + files → pass/fail table
- [ ] HMAC Generator — GN — Message + key + algorithm → HMAC
- [ ] HMAC Tool — FP — File + key + algorithm → HMAC

**Passwords**
- [ ] Password Generator — GN — Length, charset, count → live passwords
- [ ] Passphrase Generator — GN — Word count, separator → live passphrase
- [ ] PIN Generator — GN — Length → live PIN
- [ ] Password Strength Checker — CV — Password → live strength meter
- [ ] Password Breach Check — LK — Password → breach status
- [ ] Password Vault — VE — Master unlock → entry list + editor

**Keys & Certificates**
- [ ] Key Generator — GN — Type, size, format → key pair
- [ ] Key Pair Generator — GN — RSA/ECDSA/Ed25519 → both keys
- [ ] Key Manager — VE — Key ring, import/export/delete
- [ ] SSH Key Generator — GN — Type, bits, comment → SSH key pair
- [ ] SSH Key Inspector — VE — Drop SSH key → details panel
- [ ] CSR Generator — GN — Subject fields → PEM CSR + key
- [ ] Self-signed Certificate Generator — GN — Subject, validity → cert + key
- [ ] Certificate Chain Verifier — FP — Leaf + intermediates + root → chain status
- [ ] X.509 Certificate Viewer — VE — Drop cert → read-only tree
- [ ] SSL/TLS Certificate Checker — FP — Cert or hostname → chain report

**PDF Signing**
- [ ] PDF Encrypt — FP — PDF + passwords + permissions
- [ ] PDF Decrypt — FP — PDF + password
- [ ] PDF Certificate Sign — FP — PDF + cert + key

**Privacy**
- [ ] Secure Delete — FP — File/folder + passes → shred
- [ ] Steganography — FP — Cover image + secret → embedded image
- [ ] Steganography Encode — CV — Image + text → modified image
- [ ] Steganography Decode — CV — Image → extracted text

**Network**
- [ ] Local Port Scanner — LK — Host + range → open ports table

---

## 📄 Module 2 — PDF

### [🚧] Module in progress (3 of 61 tools shipped)

**Viewing & Navigation**
- [ ] PDF Viewer — VE — Full viewer with zoom, thumbnails, search
- [ ] Page Thumbnails — FP — Thumbnail grid → ZIP
- [ ] PDF Search — FP — Query → highlighted results
- [ ] PDF Outline / Bookmarks — FP — Extract outline
- [ ] PDF Metadata Viewer — VE — Read-only metadata table

**Organizing**
- [✅] Merge PDFs — FP — Multi-file + reorder → merged PDF — *Shipped: `pdf_merge.c`*
- [✅] Split PDF — FP — Mode (page/range/count) → multiple PDFs — *Shipped: `pdf_split.c`*
- [✅] PDF Compress — FP — Quality preset → compressed PDF — *Shipped: `pdf_compress.c`*
- [ ] Extract Pages — FP — Ranges → extracted PDF(s)
- [ ] Delete Pages — FP — Thumbnail grid → modified PDF
- [ ] Reorder Pages — FP — Drag grid → reordered PDF
- [ ] Rotate Pages — FP — Angle + range → rotated PDF — *Wrapper ready: `qpdf_rotate_pages()`*
- [ ] Insert Pages — FP — Two PDFs + position → combined
- [ ] Duplicate Pages — FP — Pages + count → duplicated PDF
- [ ] Reverse Page Order — FP — Reverse → PDF

**Editing**
- [ ] PDF Text Editor — VE — Inline text editing
- [ ] PDF Annotation — FP — Highlight/underline/note → annotated PDF
- [ ] PDF Drawing — FP — Canvas with pen/shape → annotated PDF
- [ ] PDF Form Filler — FP — Editable form fields → filled PDF
- [ ] PDF Form Creator — FP — Add form fields → fillable PDF
- [ ] PDF Redaction — FP — Redaction editor → redacted PDF
- [ ] PDF Watermark — FP — Text/image + position → watermarked PDF
- [ ] PDF Page Numbers — FP — Format + position → numbered PDF
- [ ] PDF Headers & Footers — FP — Text + position → modified PDF
- [ ] PDF Stamp Tool — FP — Stamp + position → stamped PDF
- [ ] PDF Signature — FP — Draw/type/upload → signed PDF
- [ ] PDF Bookmark Editor — VE — Tree editor → saved PDF
- [ ] PDF Link Editor — VE — Link list editor
- [ ] PDF Attachment Manager — VE — Attachment panel

**Security**
- [ ] PDF Encrypt — FP — See Security module — *Wrapper ready: `qpdf_encrypt_file()`*
- [ ] PDF Decrypt — FP — See Security module — *Wrapper ready: `qpdf_decrypt_file()`*
- [ ] PDF Permissions — FP — Restrict print/copy/edit
- [ ] PDF Certificate Sign — FP — See Security module
- [ ] PDF Timestamp — FP — TSA → timestamped PDF

**Optimization**
- [✅] PDF Compress — FP — See Organizing
- [ ] PDF Optimize for Web — FP — DPI + linearize
- [ ] PDF Optimize for Print — FP — DPI + embed fonts
- [ ] PDF Repair — FP — Damaged PDF → recovered
- [ ] PDF Linearize — FP — Fast web view

**Conversion**
- [ ] PDF to Image — CV — Format + DPI + range
- [ ] Image to PDF — CV — Multi-image → PDF
- [ ] PDF to Text — CV — Plain text extraction
- [ ] PDF to HTML — CV — HTML with images
- [ ] PDF to DOCX — CV — Word document
- [ ] PDF to XLSX — CV — Table extraction
- [ ] PDF to PPTX — CV — Slide conversion
- [ ] PDF to Markdown — CV — Markdown
- [ ] PDF to EPUB — CV — Reflowable ebook
- [ ] PDF to LaTeX — CV — LaTeX source
- [ ] Office to PDF — CV — DOCX/XLSX/PPTX → PDF
- [ ] HTML to PDF — CV — HTML → PDF
- [ ] Markdown to PDF — CV — MD → PDF
- [ ] EPUB to PDF — CV — EPUB → PDF
- [ ] DOCX to PDF — CV — DOCX → PDF
- [ ] PPTX to PDF — CV — PPTX → PDF
- [ ] XLSX to PDF — CV — XLSX → PDF
- [ ] LaTeX to PDF — CV — .tex → PDF
- [ ] PDF/A Converter — CV — Archival standard

**OCR & Scan**
- [ ] OCR PDF — FP — Searchable text layer
- [ ] OCR Image to Text — CV — Image → text
- [ ] Scan to PDF — CV — Scanner → PDF
- [ ] Multi-page Scan — FP — ADF scan → PDF
- [ ] Deskew Scanned Pages — FP — Straighten
- [ ] Auto-crop Scan — FP — Trim borders

**Comparison**
- [ ] PDF Compare — FP — Side-by-side diff
- [ ] PDF Overlay Compare — FP — Overlay diff

---

## 🖼️ Module 3 — Image

### [ ] Module complete (97 tools)

**Viewing**
- [ ] Image Viewer — VE — Full-screen viewer
- [ ] Image Slideshow — FP — Slideshow
- [ ] Image Thumbnail Grid — FP — Contact sheet
- [ ] EXIF Viewer — VE — EXIF table
- [ ] Metadata Inspector — VE — All metadata

**Basic Editing**
- [ ] Crop — FP — Crop overlay → cropped image
- [ ] Resize — FP — Dimensions + algorithm
- [ ] Rotate — FP — Angle → rotated
- [ ] Flip — FP — H/V mirror
- [ ] Straighten — FP — Auto or manual
- [ ] Perspective Correct — FP — Corner drag
- [ ] Canvas Resize — FP — New canvas size
- [ ] Auto-crop Borders — FP — Trim borders

**Adjustments**
- [ ] Brightness / Contrast — FP — Sliders
- [ ] Levels — FP — Black/white/gamma
- [ ] Curves — FP — Curve editor
- [ ] Exposure — FP — EV slider
- [ ] Saturation / Vibrance — FP — Two sliders
- [ ] Hue Shift — FP — Hue rotation
- [ ] White Balance — FP — Temperature + tint
- [ ] Color Balance — FP — Shadows/mids/highlights
- [ ] Shadows / Highlights — FP — Recovery
- [ ] Gamma Correction — FP — Gamma slider
- [ ] Auto Enhance — FP — One-click
- [ ] Histogram — FP — RGB + luma chart

**Filters**
- [ ] Blur — FP — Gaussian/motion/radial
- [ ] Sharpen — FP — Strength + radius
- [ ] Unsharp Mask — FP — Amount/radius/threshold
- [ ] Noise Reduction — FP — Strength
- [ ] Denoise — FP — Wavelet
- [ ] Sepia — FP — Intensity
- [ ] Grayscale — FP — Channel mixer
- [ ] Invert — FP — Negative
- [ ] Posterize — FP — Levels
- [ ] Threshold — FP — Level
- [ ] Vignette — FP — Amount + radius
- [ ] Film Grain — FP — Intensity
- [ ] Glow — FP — Radius + intensity
- [ ] Emboss — FP — Strength
- [ ] Edge Detect — FP — Algorithm
- [ ] Pixelate — FP — Block size
- [ ] Mosaic — FP — Tile size

**Drawing & Compositing**
- [ ] Brush Tool — FP — Canvas painting
- [ ] Eraser — FP — Canvas erasing
- [ ] Fill / Bucket — FP — Flood fill
- [ ] Gradient Tool — FP — Linear/radial
- [ ] Text Tool — FP — Text placement
- [ ] Shape Tool — FP — Rect/ellipse/polygon
- [ ] Arrow Tool — FP — Arrow drawing
- [ ] Eyedropper — FP — Color picker
- [ ] Clone Stamp — FP — Clone tool
- [ ] Healing Brush — FP — Healing
- [ ] Smudge Tool — FP — Smudge
- [ ] Dodge / Burn — FP — Lighten/darken
- [ ] Selection Tools — FP — Rect/lasso/wand
- [ ] Layer Manager — VE — Layer panel
- [ ] Layer Masks — FP — Mask editing
- [ ] Blend Modes — FP — Mode + opacity
- [ ] Opacity Control — FP — Opacity slider

**Transform**
- [ ] Scale — FP — Scale + anchor
- [ ] Skew — FP — Skew angle
- [ ] Distort — FP — Pinch/bulge/twirl
- [ ] Warp — FP — Mesh warp
- [ ] Free Transform — FP — Move/rotate/scale

**Retouching**
- [ ] Background Removal — FP — Auto/manual cutout
- [ ] Background Replace — FP — New background
- [ ] Portrait Retouch — FP — Skin/eye/teeth sliders
- [ ] Blemish Removal — FP — Brush tool
- [ ] Red-eye Removal — FP — Auto/click
- [ ] Teeth Whitening — FP — Click/auto
- [ ] Skin Smoothing — FP — Strength

**Composition**
- [ ] Collage Maker — FP — Multi-image collage
- [ ] Grid Layout — FP — Grid layout
- [ ] Panorama Stitch — FP — Overlap stitch
- [ ] HDR Merge — FP — Multi-exposure
- [ ] Focus Stack — FP — Focus stacking
- [ ] Image Stack — FP — Blend modes
- [ ] Watermark — FP — Text/image watermark
- [ ] Border / Frame — FP — Border styles
- [ ] Drop Shadow — FP — Shadow
- [ ] Reflection — FP — Reflection effect

**Format & Conversion**
- [ ] Format Converter — CV — Any → any format
- [ ] Batch Convert — CV — Multi-file batch
- [ ] HEIC to JPG — CV — HEIC conversion
- [ ] RAW to JPG — CV — RAW development
- [ ] SVG to PNG — CV — Rasterize
- [ ] PNG to SVG — CV — Trace
- [ ] Image to Base64 — CV — Base64 encoding
- [ ] Image Compression — FP — Quality slider
- [ ] Lossless Optimizer — FP — Lossless optimize

**Special**
- [ ] QR Code Reader — CV — Image → decoded text
- [ ] Barcode Reader — CV — Image → decoded
- [ ] Steganography — FP — Embed text
- [ ] Steganography Encode — CV — See Security
- [ ] Steganography Decode — CV — See Security
- [ ] Image to ASCII Art — CV — ASCII conversion
- [ ] Pixel Art Scaler — FP — Upscale
- [ ] Sprite Sheet Slicer — FP — Slice sheet
- [ ] Animated GIF Maker — FP — Images → GIF
- [ ] Animated GIF Splitter — FP — GIF → frames
- [ ] Screenshot Tool — FP — Capture region
- [ ] Screen Recorder — FP — Record screen
- [ ] Color Picker — FP — Eyedropper
- [ ] Color Converter — CV — Color format conversion
- [ ] Color Palette Generator — GN — Palette
- [ ] Color Blindness — FP — CVD simulation
- [ ] Grid Layout — FP — See Composition
- [ ] OCR Image to Text — CV — OCR
- [ ] PPTX to Images — CV — Slides → images
- [ ] ULID Generator — GN — Misassigned; belongs in DevTools
- [ ] Unicode Escape / Unescape — CV — Misassigned; belongs in DevTools

---

## 🎵 Module 4 — Video

### [ ] Module complete (34 tools)

**Playback & Info**
- [ ] Video Player — FP — Full player
- [ ] Video Codec Info — FP — Codec + stream info
- [ ] Video Bitrate Calculator — CA — Duration + size → bitrate
- [ ] Video Thumbnail Generator — GN — Timestamp + count

**Editing**
- [ ] Video Trimmer — FP — Start/end → clip
- [ ] Video Cutter — FP — Keyframe cut
- [ ] Video Merger — FP — Multi-clip merge
- [ ] Video Splitter — FP — Split by time/size
- [ ] Video Cropper — FP — Crop region
- [ ] Video Resizer — FP — Resolution change
- [ ] Video Rotator — FP — 90/180/270
- [ ] Video Flipper — FP — Mirror
- [ ] Video Speed Change — FP — 0.25x–4x
- [ ] Video Reverse — FP — Reverse playback
- [ ] Video Stabilizer — FP — Stabilization
- [ ] Video Color Correction — FP — Color panel
- [ ] Video Overlay — FP — Image/text overlay
- [ ] Video Watermark — FP — Watermark
- [ ] Video Chapter Editor — VE — Chapter timeline
- [ ] Video Metadata Editor — VE — Metadata form

**Conversion**
- [ ] Video Converter — CV — Format conversion
- [ ] Video Compressor — FP — Target size
- [ ] Video to Audio — CV — Extract audio
- [ ] Video to GIF — CV — Clip → GIF
- [ ] GIF to Video — CV — GIF → video
- [ ] Video to Images — CV — Frames → images
- [ ] Images to Video — CV — Images → video
- [ ] Video Frame Extractor — FP — Frames at intervals
- [ ] Video Screen Recorder — FP — Screen recording
- [ ] Webcam Recorder — FP — Webcam capture
- [ ] Screen Recorder — FP — Screen recording
- [ ] Screenshot Tool — FP — Screenshot
- [ ] Video Subtitle Muxer — FP — Embed subtitles
- [ ] Video Subtitle Extractor — FP — Extract subtitles

---

## 🎧 Module 5 — Audio

### [ ] Module complete (19 tools)

- [ ] Audio Player — FP — Player with waveform
- [ ] Audio Converter — CV — Format conversion
- [ ] Audio Trimmer — FP — Start/end trim
- [ ] Audio Merger — FP — Join audio
- [ ] Audio Splitter — FP — By time/silence
- [ ] Audio Normalizer — FP — Target LUFS
- [ ] Audio Compressor — FP — Threshold/ratio
- [ ] Audio Amplifier — FP — Gain
- [ ] Audio Fade In/Out — FP — Fade durations
- [ ] Audio Pitch Shift — FP — Semitones
- [ ] Audio Speed Change — FP — Speed
- [ ] Audio Reverb — FP — Room/damping
- [ ] Audio Equalizer — FP — 10-band EQ
- [ ] Audio Noise Removal — FP — Profile + strength
- [ ] Audio Metadata Editor — VE — ID3 tags
- [ ] Audio Waveform Viewer — VE — Waveform display
- [ ] Audio Spectrogram — FP — Spectrogram
- [ ] Audio Transcription — FP — Whisper.cpp
- [ ] Audio Chapter Splitter — FP — By chapters

---

## 📝 Module 6 — Document

### [ ] Module complete (29 tools)

**Word Processing**
- [ ] DOCX Viewer — VE — Read-only render
- [ ] DOCX Editor — VE — Rich text editor
- [ ] DOCX to HTML — CV
- [ ] DOCX to Markdown — CV
- [ ] DOCX to TXT — CV
- [ ] TXT to DOCX — CV
- [ ] Markdown to DOCX — CV
- [ ] ODT Converter — CV

**Spreadsheets**
- [ ] XLSX Viewer — VE — Grid with sheets
- [ ] XLSX to CSV — CV
- [ ] CSV to XLSX — CV
- [ ] CSV Viewer / Editor — VE — Editable grid
- [ ] ODS Converter — CV
- [ ] Spreadsheet Merger — FP

**Presentations**
- [ ] PPTX Viewer — VE — Slide viewer
- [ ] ODP Converter — CV

**Ebooks**
- [ ] EPUB Reader — FP — Reader
- [ ] EPUB Editor — VE — Chapter editor
- [ ] EPUB to MOBI — CV
- [ ] MOBI to EPUB — CV
- [ ] EPUB Metadata Editor — VE
- [ ] Comic Reader — FP — CBZ/CBR reader
- [ ] Comic Converter — CV

**Markup**
- [ ] Markdown Editor — VE — Source + preview
- [ ] Markdown Previewer — VE — Rendered preview
- [ ] HTML Viewer — VE — WebKit render
- [ ] HTML Editor — VE — Source + preview
- [ ] HTML to Markdown — CV
- [ ] RST Converter — CV
- [ ] HTML Entity Encode / Decode — CV

**Office Utilities**
- [ ] Document Metadata Editor — VE
- [ ] Document Compare — FP
- [ ] Document Merger — FP
- [ ] Document Splitter — FP
- [ ] Document Watermark — FP
- [ ] Document Redaction — FP
- [ ] Document OCR — FP

---

## 💻 Module 7 — DevTools / Text / Encoding

### [ ] Module complete (96 tools)

**Encoding & Formats**
- [ ] Base64 Tool — FP
- [ ] Base64 Encode — CV
- [ ] Base64 Decode — CV
- [ ] URL Encoder — CV
- [ ] URL Encode / Decode — CV
- [ ] URL Parser — CV
- [ ] JWT Decoder — CV
- [ ] JWT Encoder — CV
- [ ] UUID Generator — GN
- [ ] UUID / ULID / NanoID Generator — GN
- [ ] NanoID Generator — GN
- [ ] Hash Generator — GN
- [ ] Hash Tool — FP
- [ ] HMAC Generator — GN
- [ ] HMAC Tool — FP
- [ ] Bcrypt / Argon2 Tool — FP
- [ ] Text Hash Calculator — CV
- [ ] Unicode Escape / Unescape — CV

**Data**
- [ ] JSON Formatter — CV
- [ ] JSON Validator — FP
- [ ] JSONPath Tester — FP
- [ ] JSON Diff — FP
- [ ] YAML Formatter — CV
- [ ] XML Formatter — CV
- [ ] TOML Formatter — CV
- [ ] SQL Formatter — CV
- [ ] CSV Formatter — CV
- [ ] JSON to YAML — CV
- [ ] YAML to JSON — CV
- [ ] JSON to XML — CV
- [ ] XML to JSON — CV
- [ ] JSON to TOML — CV
- [ ] TOML to JSON — CV
- [ ] CSV to JSON — CV
- [ ] JSON to CSV — CV
- [ ] JSON to SQL — CV
- [ ] SQL to JSON — CV
- [ ] XML to YAML — CV
- [ ] Dummy JSON Generator — GN
- [ ] Fake Data Generator — GN
- [ ] JSON Schema Validator — FP
- [ ] OpenAPI Viewer — VE

**Text & Regex**
- [ ] Regex Tester — FP
- [ ] Regex Cheatsheet — FP
- [ ] Text Diff — FP
- [ ] Diff Viewer — VE
- [ ] Line Sorter / Deduplicator — CV
- [ ] Line Deduplicator — CV
- [ ] Case Converter — CV
- [ ] Text Case Converter — CV
- [ ] Text Reverser — CV
- [ ] Text Sorter — CV
- [ ] Whitespace Trimmer — CV
- [ ] Word & Char Counter — CV
- [ ] Slug Generator — GN
- [ ] String Escape / Unescape — CV
- [ ] Lorem Ipsum — FP
- [ ] Lorem Ipsum Generator — GN
- [ ] Find & Replace — FP
- [ ] Morse Code — FP
- [ ] NATO Alphabet — FP
- [ ] ROT13 — FP

**Time & Numbers**
- [ ] Unix Timestamp — FP
- [ ] Unix Timestamp Converter — CV
- [ ] ISO 8601 Converter — CV
- [ ] Cron Parser — CV
- [ ] Cron Expression — FP
- [ ] Number Base Converter — CV
- [ ] Binary Calculator — CA
- [ ] Hex Calculator — CA
- [ ] Bitwise Calculator — CA
- [ ] Byte Size Converter — CV
- [ ] Random Data Generator — GN
- [ ] Random Number Generator — GN

**Network & Web**
- [ ] HTTP Header Parser — CV
- [ ] HTTP Status Code Lookup — LK
- [ ] User-Agent Parser — LK
- [ ] MIME Type Lookup — LK
- [ ] CIDR Calculator — CA
- [ ] IP / Subnet Calculator — CA
- [ ] IPv4 / IPv6 Converter — CV
- [ ] DNS Lookup — LK
- [ ] WHOIS Lookup — LK
- [ ] MAC Address Lookup — LK
- [ ] Local Port Scanner — LK

**Code & Files**
- [ ] Code Formatter — CV
- [ ] Code Minifier — FP
- [ ] File Format Inspector — VE
- [ ] File Type Detector — FP
- [ ] Hex Viewer — VE
- [ ] Binary Viewer — VE
- [ ] File Checksum — FP
- [ ] Line Ending Converter — CV
- [ ] BOM Add / Remove — FP
- [ ] File Renamer — FP
- [ ] File Permission Editor — VE
- [ ] File Timestamp Editor — VE
- [ ] Anonymous File Renamer — FP
- [ ] File Splitter / Joiner — FP

**API & Testing**
- [ ] REST Client — FP
- [ ] GraphQL Query Tester — FP
- [ ] WebSocket Tester — FP
- [ ] JSON Schema Validator — FP
- [ ] OpenAPI Viewer — VE

**System Info**
- [ ] System Info Viewer — VE
- [ ] Disk Usage Viewer — VE
- [ ] Process Viewer — VE
- [ ] Environment Variable Viewer — VE
- [ ] Font Viewer — VE
- [ ] Installed Package Viewer — VE
- [ ] Network Interface Viewer — VE
- [ ] Battery Info — FP
- [ ] CPU / Memory Monitor — FP
- [ ] Sensors Viewer — VE

---

## 🧮 Module 8 — Calculators & Unit Converters

### [ ] Module complete (65 tools)

**Math Calculators** (all CA type)
- [ ] Scientific Calculator
- [ ] Percentage Calculator
- [ ] Big Number Calculator
- [ ] Exponent Calculator
- [ ] Factor Calculator
- [ ] Fraction Calculator
- [ ] Greatest Common Factor
- [ ] Least Common Multiple
- [ ] Log Calculator
- [ ] Matrix Calculator
- [ ] Number Sequence Calculator
- [ ] Percent Error Calculator
- [ ] Permutation and Combination
- [ ] Probability Calculator
- [ ] Pythagorean Theorem
- [ ] Quadratic Formula Calculator
- [ ] Ratio Calculator
- [ ] Right Triangle Calculator
- [ ] Roman Numeral
- [ ] Root Calculator
- [ ] Rounding Calculator
- [ ] Sample Size Calculator
- [ ] Scientific Notation Calculator
- [ ] Slope Calculator
- [ ] Standard Deviation Calculator
- [ ] Statistics Calculator
- [ ] Triangle Calculator
- [ ] Z-score Calculator
- [ ] Mean, Median, Mode, Range
- [ ] Confidence Interval Calculator
- [ ] Half-Life Calculator
- [ ] Distance Calculator
- [ ] Duration Calculator

**Geometry**
- [ ] Area Calculator
- [ ] Circle Calculator
- [ ] Surface Area Calculator
- [ ] Volume Calculator
- [ ] Pythagorean Theorem

**Health & Everyday**
- [ ] BMI Calculator
- [ ] Tip Calculator
- [ ] Age Calculator
- [ ] Week Number
- [ ] World Clock

**Unit Converters** (CV type)
- [ ] Unit Converter
- [ ] Temperature Converter
- [ ] Currency Converter
- [ ] Data Size Converter
- [ ] Energy Converter
- [ ] Pressure Converter
- [ ] Speed Converter
- [ ] Cooking Measure
- [ ] Character Encoding Converter
- [ ] Centimetre
- [ ] Fahrenheit
- [ ] Gram
- [ ] Octal (8)
- [ ] Weight

**File/Misc Converters** (CV type)
- [ ] Universal File Converter
- [ ] Batch Converter
- [ ] Archive Converter
- [ ] HTML to Markdown
- [ ] RST Converter
- [ ] Subtitle Converter
- [ ] Text to Speech
- [ ] Speech to Text

**Color**
- [ ] Contrast Checker

---

## 📦 Module 9 — Archive

### [ ] Module complete (10 tools)

- [ ] ZIP Create — FP
- [ ] ZIP Extract — FP
- [ ] TAR Create / Extract — FP
- [ ] 7Z Create / Extract — FP
- [ ] RAR Extract — FP
- [ ] GZ / BZ2 / XZ Compress — FP
- [ ] Archive Converter — CV
- [ ] Archive Splitter — FP
- [ ] File Splitter / Joiner — FP
- [ ] Notes / Scratchpad — VE
- [ ] Palette Generator — GN

---

## 💬 Module 10 — Subtitle

### [ ] Module complete (3 tools)

- [ ] Subtitle Editor — VE — Timeline editor
- [ ] Subtitle Sync — FP — Offset + framerate
- [ ] Subtitle Translator — FP — Offline translation

---

## 🔧 Module 11 — Utility

### [ ] Module complete (6 tools)

- [ ] Notes / Scratchpad — VE — Autosave notes
- [ ] To-Do List — VE — Task manager
- [ ] Clipboard Manager — VE — History
- [ ] Countdown Timer — FP — Countdown
- [ ] Pomodoro Timer — FP — Work/break cycles
- [ ] Stopwatch — FP — Elapsed + laps
- [ ] Random Picker — FP — Dice/coin/cards

---

## 🧩 Module 12 — Misc

### [ ] Module complete (28 tools)

- [ ] Barcode Generator — GN
- [ ] Barcode Reader — CV
- [ ] QR Code Generator — GN
- [ ] QR Code Reader — CV
- [ ] Gradient Generator — GN
- [ ] Palette Generator — GN
- [ ] Invert — FP — *BentoPDF reference*
- [ ] Border / Frame — FP
- [ ] Brightness / Contrast — FP
- [ ] Edge Detect — FP
- [ ] Film Grain — FP
- [ ] Lossless Optimizer — FP
- [ ] Metadata Inspector — VE
- [ ] CSV Viewer / Editor — VE
- [ ] HTML Editor — VE
- [ ] HTML Viewer — VE
- [ ] Markdown Editor — VE
- [ ] Markdown Previewer — VE
- [ ] Cooking Measure — FP
- [ ] DNS Lookup — LK
- [ ] MAC Address Lookup — LK
- [ ] WHOIS Lookup — LK
- [ ] Batch Convert — CV
- [ ] Lorem Ipsum — FP
- [ ] Lorem Ipsum Generator — GN
- [ ] ROT13 — FP
- [ ] Random Number Generator — GN
- [ ] Anonymous File Renamer — FP

---

## 🎯 Suggested Build Order

Tick modules off in this order for fastest progress:

1. [ ] **Utility** (6) — easiest, validates the 6 templates
2. [ ] **Misc** (28) — mostly simple conversions
3. [ ] **Archive** (10) — libarchive, no GUI complexity
4. [ ] **DevTools / Text** (96) — pure text, no external deps
5. [ ] **Calculators** (65) — no external deps
6. [ ] **Image** (97) — Cairo/GEGL, medium complexity
7. [🚧] **PDF** (61) — qpdf + Poppler, hardest — *3 shipped*
8. [ ] **Document** (29) — LibreOffice/Pandoc subprocess
9. [ ] **Audio** (19) — FFmpeg
10. [ ] **Video** (34) — FFmpeg
11. [ ] **Subtitle** (3) — libass
12. [ ] **Security** (43) — libsodium, highest stakes

---

## ✅ Milestones

- [ ] **v0.1** — Utilities module complete, all 6 templates built, app runs
- [🚧] **v0.2** — PDF module complete — *3 of 61 tools done*
- [ ] **v0.3** — Images module complete
- [ ] **v0.4** — DevTools + Calculators complete
- [ ] **v0.5** — Media (Audio + Video) complete
- [ ] **v0.6** — Security module complete
- [ ] **v0.7** — Documents + Archive + Subtitle complete
- [ ] **v0.8** — Misc complete
- [ ] **v0.9** — All 491 tools shipped, testing, accessibility pass
- [ ] **v1.0** — Flathub submission, AppStream metadata, docs, release
