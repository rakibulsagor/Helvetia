import os
import re

# We will skip Module 1 since we already have a custom utility_module.c
RAW_TEXT = """
## 📄 Module 2 — PDF
*Full PDF editing, all offline.*

### Viewing & Navigation
63. **PDF Viewer**
64. **Page Thumbnails**
65. **PDF Search**
66. **PDF Outline / Bookmarks**
67. **PDF Metadata Viewer**

### Organizing
68. **Merge PDFs**
69. **Split PDF**
70. **Extract Pages**
71. **Delete Pages**
72. **Reorder Pages**
73. **Rotate Pages**
74. **Insert Pages**
75. **Duplicate Pages**
76. **Reverse Page Order**

### Editing
77. **PDF Text Editor** (inline editing)
78. **PDF Annotation** (highlight, underline, strike, notes)
79. **PDF Drawing** (freehand, shapes, arrows)
80. **PDF Form Filler**
81. **PDF Form Creator**
82. **PDF Redaction**
83. **PDF Watermark** (text and image)
84. **PDF Page Numbers**
85. **PDF Headers & Footers**
86. **PDF Stamp Tool**
87. **PDF Signature** (draw, type, image)
88. **PDF Bookmark Editor**
89. **PDF Link Editor**
90. **PDF Attachment Manager**

### Security
91. **PDF Encrypt** (password)
92. **PDF Decrypt**
93. **PDF Permissions** (print, copy, edit)
94. **PDF Certificate Sign**
95. **PDF Timestamp**

### Optimization
96. **PDF Compress**
97. **PDF Optimize for Web**
98. **PDF Optimize for Print**
99. **PDF Repair**
100. **PDF Linearize** (fast web view)

### Conversion
101. **PDF to Image** (PNG, JPG, TIFF, WEBP)
102. **Image to PDF**
103. **PDF to Text**
104. **PDF to HTML**
105. **PDF to DOCX**
106. **PDF to XLSX**
107. **PDF to PPTX**
108. **PDF to Markdown**
109. **PDF to EPUB**
110. **Office to PDF** (DOCX, XLSX, PPTX, ODT, ODS)
111. **HTML to PDF**
112. **Markdown to PDF**
113. **EPUB to PDF**
114. **PDF/A Converter** (archival)

### OCR & Scan
115. **OCR PDF** (Tesseract)
116. **OCR Image to Text**
117. **Scan to PDF** (SANE scanner)
118. **Multi-page Scan**
119. **Deskew Scanned Pages**
120. **Auto-crop Scan**

### Comparison
121. **PDF Compare** (side-by-side diff)
122. **PDF Overlay Compare**

---

## 🖼️ Module 3 — Images
*From quick crops to full editing.*

### Viewing
123. **Image Viewer**
124. **Image Slideshow**
125. **Image Thumbnail Grid**
126. **EXIF Viewer**
127. **Metadata Inspector**

### Basic Editing
128. **Crop**
129. **Resize**
130. **Rotate**
131. **Flip**
132. **Straighten**
133. **Perspective Correct**
134. **Canvas Resize**
135. **Auto-crop Borders**

### Adjustments
136. **Brightness / Contrast**
137. **Levels**
138. **Curves**
139. **Exposure**
140. **Saturation / Vibrance**
141. **Hue Shift**
142. **White Balance**
143. **Color Balance**
144. **Shadows / Highlights**
145. **Gamma Correction**
146. **Auto Enhance**
147. **Histogram**

### Filters
148. **Blur** (Gaussian, motion, radial)
149. **Sharpen**
150. **Unsharp Mask**
151. **Noise Reduction**
152. **Denoise**
153. **Sepia**
154. **Grayscale**
155. **Invert**
156. **Posterize**
157. **Threshold**
158. **Vignette**
159. **Film Grain**
160. **Glow**
161. **Emboss**
162. **Edge Detect**
163. **Pixelate**
164. **Mosaic**

### Drawing & Compositing
165. **Brush Tool**
166. **Eraser**
167. **Fill / Bucket**
168. **Gradient Tool**
169. **Text Tool**
170. **Shape Tool** (rectangle, ellipse, polygon, line)
171. **Arrow Tool**
172. **Eyedropper**
173. **Clone Stamp**
174. **Healing Brush**
175. **Smudge Tool**
176. **Dodge / Burn**
177. **Selection Tools** (rect, ellipse, lasso, magic wand)
178. **Layer Manager**
179. **Layer Masks**
180. **Blend Modes**
181. **Opacity Control**

### Transform
182. **Scale**
183. **Skew**
184. **Distort**
185. **Warp**
186. **Free Transform**

### Retouching
187. **Background Removal**
188. **Background Replace**
189. **Portrait Retouch**
190. **Blemish Removal**
191. **Red-eye Removal**
192. **Teeth Whitening**
193. **Skin Smoothing**

### Composition
194. **Collage Maker**
195. **Grid Layout**
196. **Panorama Stitch**
197. **HDR Merge**
198. **Focus Stack**
199. **Image Stack**
200. **Watermark** (text/image)
201. **Border / Frame**
202. **Drop Shadow**
203. **Reflection**

### Format & Conversion
204. **Format Converter** (PNG, JPG, WEBP, AVIF, HEIC, TIFF, BMP, GIF, SVG, ICO)
205. **Batch Convert**
206. **HEIC to JPG**
207. **RAW to JPG** (LibRaw)
208. **SVG to PNG**
209. **PNG to SVG** (trace)
210. **Image to Base64**
211. **Image Compression**
212. **Lossless Optimizer**

### Special
213. **QR Code Reader**
214. **Barcode Reader**
215. **Steganography** (hide/extract text)
216. **Image to ASCII Art**
217. **Pixel Art Scaler**
218. **Sprite Sheet Slicer**
219. **Animated GIF Maker**
220. **Animated GIF Splitter**
221. **Screenshot Tool**
222. **Screen Recorder** (GIF/video)

---

## 🎵 Module 4 — Media
*Audio and video, offline via FFmpeg.*

### Audio
223. **Audio Player**
224. **Audio Converter** (MP3, FLAC, WAV, OGG, AAC, OPUS, M4A)
225. **Audio Trimmer**
226. **Audio Merger**
227. **Audio Splitter**
228. **Audio Normalizer**
229. **Audio Compressor**
230. **Audio Amplifier**
231. **Audio Fade In/Out**
232. **Audio Pitch Shift**
233. **Audio Speed Change**
234. **Audio Reverb**
235. **Audio Equalizer**
236. **Audio Noise Removal**
237. **Audio Metadata Editor**
238. **Audio Waveform Viewer**
239. **Audio Spectrogram**
240. **Text to Speech** (eSpeak NG / Piper)
241. **Speech to Text** (Whisper.cpp)
242. **Audio Transcription**
243. **Audio Chapter Splitter**

### Video
244. **Video Player**
245. **Video Converter** (MP4, MKV, WEBM, AVI, MOV, HEVC, AV1)
246. **Video Trimmer**
247. **Video Cutter**
248. **Video Merger**
249. **Video Splitter**
250. **Video Compressor**
251. **Video Resizer**
252. **Video Cropper**
253. **Video Rotator**
254. **Video Flipper**
255. **Video Speed Change**
256. **Video Reverse**
257. **Video Stabilizer**
258. **Video Frame Extractor**
259. **Video to GIF**
260. **GIF to Video**
261. **Video to Audio**
262. **Video to Images**
263. **Images to Video**
264. **Video Thumbnail Generator**
265. **Video Metadata Editor**
266. **Video Chapter Editor**
267. **Video Subtitle Muxer**
268. **Video Subtitle Extractor**
269. **Video Watermark**
270. **Video Overlay**
271. **Video Color Correction**
272. **Video Bitrate Calculator**
273. **Video Codec Info**
274. **Video Screen Recorder**
275. **Webcam Recorder**
276. **DVD / Blu-ray Ripper** (offline, decryption-free sources only)

### Subtitles
277. **Subtitle Editor**
278. **Subtitle Converter** (SRT, VTT, ASS, SSA)
279. **Subtitle Sync**
280. **Subtitle Translator** (offline, Argos)

---

## 📝 Module 5 — Documents
*Office and text document tools.*

### Word Processing
281. **DOCX Viewer**
282. **DOCX Editor** (basic)
283. **DOCX to PDF**
284. **DOCX to Markdown**
285. **DOCX to HTML**
286. **DOCX to TXT**
287. **TXT to DOCX**
288. **Markdown to DOCX**
289. **ODT Converter**

### Spreadsheets
290. **XLSX Viewer**
291. **XLSX to PDF**
292. **XLSX to CSV**
293. **CSV to XLSX**
294. **CSV Viewer / Editor**
295. **CSV to JSON**
296. **JSON to CSV**
297. **ODS Converter**
298. **Spreadsheet Merger**

### Presentations
299. **PPTX Viewer**
300. **PPTX to PDF**
301. **PPTX to Images**
302. **ODP Converter**

### Ebooks
303. **EPUB Reader**
304. **EPUB Editor**
305. **EPUB to PDF**
306. **EPUB to MOBI**
307. **MOBI to EPUB**
308. **EPUB Metadata Editor**
309. **Comic Reader** (CBZ, CBR)
310. **Comic Converter**

### Markup
311. **Markdown Editor**
312. **Markdown Previewer**
313. **HTML Viewer**
314. **HTML Editor**
315. **HTML to PDF**
316. **HTML to Markdown**
317. **RST Converter**
318. **LaTeX to PDF**
319. **PDF to LaTeX**

### Office Utilities
320. **Document Metadata Editor**
321. **Document Compare**
322. **Document Merger**
323. **Document Splitter**
324. **Document Watermark**
325. **Document Redaction**
326. **Document OCR**

---

## 🔄 Module 6 — Convert
*Universal file format conversion.*

### Archives
327. **ZIP Create**
328. **ZIP Extract**
329. **TAR Create / Extract**
330. **GZ / BZ2 / XZ Compress**
331. **7Z Create / Extract**
332. **RAR Extract**
333. **Archive Converter**
334. **Archive Splitter**
335. **Archive Password**

### Data Formats
336. **JSON Formatter**
337. **JSON Validator**
338. **JSON to YAML**
339. **YAML to JSON**
340. **JSON to XML**
341. **XML to JSON**
342. **JSON to TOML**
343. **TOML to JSON**
344. **CSV to JSON**
345. **JSON to CSV**
346. **JSON to SQL**
347. **SQL to JSON**
348. **JSONPath Tester**
349. **XML Formatter**
350. **XML to YAML**

### Encoding
351. **Base64 Encode**
352. **Base64 Decode**
353. **URL Encode / Decode**
354. **HTML Entity Encode / Decode**
355. **Hex Encode / Decode**
356. **Binary Encode / Decode**
357. **Unicode Escape / Unescape**
358. **ROT13**
359. **Morse Code**
360. **NATO Alphabet**

### Charset
361. **Character Encoding Converter** (UTF-8, UTF-16, ISO-8859, Shift-JIS, GBK, KOI8)
362. **Line Ending Converter** (LF, CRLF, CR)
363. **BOM Add / Remove**

### Universal
364. **Universal File Converter** (any → any supported)
365. **Batch Converter**
366. **File Format Inspector**
367. **File Type Detector** (libmagic)
368. **File Splitter / Joiner**
369. **File Renamer** (batch, regex)
370. **File Timestamp Editor**
371. **File Permission Editor**

---

## 🔐 Module 7 — Security
*Encryption, hashing, privacy — all offline.*

### Encryption
372. **File Encrypt** (AES-256, ChaCha20)
373. **File Decrypt**
374. **Folder Encrypt**
375. **Text Encrypt**
376. **PGP Encrypt / Decrypt** (GPG)
377. **Key Generator**
378. **Key Manager**
379. **Encrypted Archive**

### Hashing
380. **File Hash Calculator**
381. **Text Hash Calculator**
382. **Hash Verifier**
383. **Checksum File Creator**
384. **Checksum File Verifier**
385. **HMAC Generator**

### Password
386. **Password Vault** (SQLite, master password)
387. **Password Strength Checker**
388. **Password Breach Check** (offline wordlist)
389. **Password Generator**
390. **Passphrase Generator**
391. **PIN Generator**

### Privacy
392. **Secure Delete** (multi-pass shred)
393. **Metadata Stripper** (images, PDFs, docs)
394. **EXIF Remover**
395. **Steganography Encode**
396. **Steganography Decode**
397. **Anonymous File Renamer**

### Certificates
398. **X.509 Certificate Viewer**
399. **Certificate Chain Verifier**
400. **CSR Generator**
401. **Self-signed Certificate Generator**
402. **Key Pair Generator** (RSA, ECDSA, Ed25519)
403. **SSH Key Generator**
404. **SSH Key Inspector**

### Network (local only)
405. **Local Port Scanner**
406. **DNS Lookup**
407. **WHOIS Lookup** (offline DB)
408. **IP / Subnet Calculator**
409. **MAC Address Lookup**
410. **SSL/TLS Certificate Checker** (offline, against local cert store)

---

## 💻 Module 8 — Dev Tools
*Everyday developer utilities.*

### Encoding & Formats
411. **Base64 Tool**
412. **URL Encoder**
413. **JWT Decoder**
414. **JWT Encoder**
415. **UUID / ULID / NanoID Generator**
416. **Hash Tool**
417. **HMAC Tool**
418. **Bcrypt / Argon2 Tool**

### Data
419. **JSON Formatter**
420. **JSON Validator**
421. **JSONPath Tester**
422. **JSON Diff**
423. **YAML Formatter**
424. **XML Formatter**
425. **TOML Formatter**
426. **SQL Formatter**
427. **CSV Formatter**

### Text & Regex
428. **Regex Tester**
429. **Regex Cheatsheet**
430. **Text Diff**
431. **Line Sorter / Deduplicator**
432. **Case Converter**
433. **Slug Generator**
434. **String Escape / Unescape**
435. **Lorem Ipsum**

### Time & Numbers
436. **Unix Timestamp Converter**
437. **ISO 8601 Converter**
438. **Cron Parser**
439. **Number Base Converter**
440. **Bitwise Calculator**
441. **Byte Size Converter**
442. **Random Data Generator**

### Network & Web
443. **HTTP Header Parser**
444. **URL Parser**
445. **User-Agent Parser**
446. **MIME Type Lookup**
447. **HTTP Status Code Lookup**
448. **CIDR Calculator**
449. **IPv4 / IPv6 Converter**

### Code & Files
450. **Code Formatter** (C, Python, JS, JSON, XML, SQL, etc.)
451. **Code Minifier**
452. **Diff Viewer**
453. **File Type Detector**
454. **Hex Viewer**
455. **Binary Viewer**
456. **File Checksum**
457. **Line Ending Converter**

### Generators
458. **UUID Generator**
459. **Password Generator**
460. **Lorem Ipsum Generator**
461. **Dummy JSON Generator**
462. **Fake Data Generator**
463. **QR Code Generator**
464. **Barcode Generator**
465. **Color Palette Generator**

### API & Testing
466. **REST Client** (offline, local only)
467. **GraphQL Query Tester** (offline schema)
468. **WebSocket Tester** (local)
469. **JSON Schema Validator**
470. **OpenAPI Viewer**

### System Info
471. **System Info Viewer**
472. **Disk Usage Viewer**
473. **Process Viewer**
474. **Environment Variable Viewer**
475. **Font Viewer**
476. **Installed Package Viewer**
477. **Network Interface Viewer**
478. **Battery Info**
479. **CPU / Memory Monitor**
480. **Sensors Viewer**
"""

def generate_id(name):
    s = name.lower()
    s = re.sub(r'[^a-z0-9]', '_', s)
    s = re.sub(r'_+', '_', s)
    return s.strip('_')

def derive_icon(t_name, mod_id):
    s = t_name.lower()
    # Provide highly specific GNOME symbolic icons
    if 'pdf' in mod_id:
        if 'search' in s: return 'system-search'
        if 'merge' in s: return 'list-add'
        if 'split' in s: return 'list-remove'
        if 'edit' in s or 'annot' in s: return 'document-edit'
        if 'secure' in s or 'encrypt' in s or 'pass' in s: return 'dialog-password'
        if 'print' in s: return 'printer'
        return 'application-pdf'
    if 'image' in mod_id:
        if 'crop' in s: return 'crop'
        if 'color' in s or 'hue' in s or 'bright' in s: return 'color-select'
        if 'camera' in s or 'photo' in s: return 'camera-photo'
        if 'view' in s: return 'eog'
        return 'image-x-generic'
    if 'media' in mod_id:
        if 'audio' in s or 'sound' in s: return 'audio-x-generic'
        if 'video' in s or 'movie' in s: return 'video-x-generic'
        return 'multimedia-video-player'
    if 'document' in mod_id:
        if 'docx' in s or 'word' in s: return 'x-office-document'
        if 'xlsx' in s or 'csv' in s or 'spread' in s: return 'x-office-spreadsheet'
        if 'pptx' in s or 'present' in s: return 'x-office-presentation'
        return 'text-x-generic'
    if 'convert' in mod_id:
        if 'zip' in s or 'tar' in s or 'archive' in s: return 'package-x-generic'
        if 'code' in s or 'base64' in s: return 'accessories-character-map'
        return 'view-refresh'
    if 'security' in mod_id:
        if 'key' in s or 'cert' in s: return 'network-server'
        if 'hash' in s: return 'document-properties'
        if 'delete' in s: return 'edit-delete'
        return 'dialog-password'
    if 'dev' in mod_id:
        if 'json' in s or 'xml' in s: return 'text-x-script'
        if 'regex' in s: return 'system-search'
        if 'network' in s or 'ip' in s: return 'network-wired'
        return 'applications-development'
        
    return 'applications-utilities'

def derive_cli(t_name):
    # e.g., "Merge PDFs" -> "merge-pdfs", "PDF to DOCX" -> "pdf2docx"
    s = t_name.lower()
    s = s.replace(' to ', '2')
    s = re.sub(r'[^a-z0-9]', '-', s)
    s = re.sub(r'-+', '-', s)
    return s.strip('-')

def derive_keywords(t_name, mod_id):
    words = t_name.lower().split()
    words.append(mod_id.lower())
    # unique words
    words = list(set([re.sub(r'[^a-z0-9]', '', w) for w in words if len(w) > 2]))
    if not words: return "NULL"
    
    kw_str = ", ".join([f'"{w}"' for w in words])
    return f"(const char*[]){{ {kw_str}, NULL }}"

modules = []
current_mod = None
current_sub = None

for line in RAW_TEXT.split("\n"):
    line = line.strip()
    if not line: continue
    
    # New module
    m = re.match(r'^## [^\w]*Module \d+ — (.*)$', line)
    if m:
        mod_name = m.group(1).strip()
        mod_id = generate_id(mod_name).replace('_tools', 'tools')
        current_mod = {
            "name": mod_name,
            "id": mod_id,
            "desc": "",
            "subs": []
        }
        modules.append(current_mod)
        continue
    
    if line.startswith('*') and line.endswith('*') and current_mod and not current_mod["subs"]:
        current_mod["desc"] = line.strip('*')
        continue
        
    # New subcategory
    m = re.match(r'^### (.*)$', line)
    if m:
        sub_name = m.group(1).strip()
        current_sub = {
            "name": sub_name,
            "tools": []
        }
        if current_mod:
            current_mod["subs"].append(current_sub)
        continue
        
    # New tool
    m = re.match(r'^\d+\.\s*\*\*(.*?)\*\*(?:\s*\((.*?)\))?$', line)
    if m:
        t_name = m.group(1).strip()
        t_desc = m.group(2).strip() if m.group(2) else ""
        t_id = generate_id(t_name)
        if current_sub:
            current_sub["tools"].append({
                "id": t_id,
                "name": t_name,
                "desc": t_desc
            })

for mod in modules:
    mod_id = mod["id"]
    if mod_id == "utilities":
        continue
        
    c_code = f"""\
/* ================================================================
 * Helvetia — {mod['name']} Module
 * Auto-generated by gen_modules.py
 * ================================================================ */
#include "{mod_id}_module.h"
#include <stddef.h>

"""
    
    for sub in mod["subs"]:
        sub_id_clean = generate_id(sub["name"])
        c_code += f"static const HelvetiaTool tools_{sub_id_clean}[] = {{\n"
        for t in sub["tools"]:
            desc = t['desc'].replace('"', '\\"') if t['desc'] else t['name']
            icon = f'"{derive_icon(t["name"], mod_id)}-symbolic"'
            cli_cmd = derive_cli(t['name'])
            keywords = derive_keywords(t['name'], mod_id)
            c_code += f'    {{ "{t["id"]}", "{t["name"]}", "{desc}", {icon}, {keywords}, "{cli_cmd}", NULL }},\n'
        c_code += "    { NULL }\n};\n\n"
        
    c_code += f"static const HelvetiaSubcategory {mod_id}_subcategories[] = {{\n"
    for sub in mod["subs"]:
        sub_id_clean = generate_id(sub["name"])
        c_code += f'    {{ "{sub["name"]}", tools_{sub_id_clean} }},\n'
    c_code += "    { NULL, NULL }\n};\n\n"
    
    desc = mod['desc'].replace('"', '\\"')
    c_code += f"""\
static const HelvetiaModule {mod_id}_module = {{
    .id            = "{mod_id}",
    .name          = "{mod['name']}",
    .icon_name     = "{derive_icon(mod['name'], mod_id)}-symbolic",
    .description   = "{desc}",
    .subcategories = {mod_id}_subcategories,
    .create_view   = NULL,
    .on_activate   = NULL,
    .on_deactivate = NULL,
    .on_shutdown   = NULL,
}};

const HelvetiaModule *helvetia_{mod_id}_get_module(void) {{
    return &{mod_id}_module;
}}
"""
    
    dir_path = f"/home/d4rkman/Downloads/Files/1.Study/projects/Swiss/src/modules/{mod_id}"
    with open(f"{dir_path}/{mod_id}_module.c", "w") as f:
        f.write(c_code)

print("Modules completely regenerated with accurate keywords, cli mappings, and icons.")
