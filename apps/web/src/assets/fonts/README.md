# Noto Sans

Self-hosted Latin WOFF2 from the official Google Fonts Noto Sans v42 stylesheet.
Normal variable weight; the upstream stylesheet assigns this same file to 400
and 700. The UI declares the 400-700 interval, including 600 headings.
35,820 bytes; SHA-256 `51ca196f49a33e79e7870ff88ebd2829a3f627a51e7d690986618f0e7ad2b52d`.

Source: https://fonts.googleapis.com/css2?family=Noto+Sans:wght@400;700&display=swap
Asset: https://fonts.gstatic.com/s/notosans/v42/o-0bIpQlx3QUlC5A4PNB6Ryti20_6n1iPHjc5a7duw.woff2
License: OFL.txt, from https://github.com/google/fonts/blob/main/ofl/notosans/OFL.txt

No OS font install or live font-service requests. Vite owns the hashed asset URL
and base path. `font-display: swap` and Segoe/system fallbacks keep first paint,
offline views and characters outside the Latin subset readable. Italic uses the
browser's normal oblique synthesis; it is not another downloaded font face.
