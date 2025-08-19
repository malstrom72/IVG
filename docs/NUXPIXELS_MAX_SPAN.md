# NUXPIXELS_MAX_SPAN Regression Results

- Built IVG2PNG with `NUXPIXELS_MAX_SPAN` values 7, 8, 9, and 10.
- Converted 60 regression `.ivg` files to PNG for each build and compared outputs.
- 24 PNGs differed; the remaining 36 were identical across all spans.

Files with differing outputs and spans producing identical PNGs:

- `StarTest.png` – span 7 | spans 8,9,10
- `beatrick.png` – span 7 | span 8 | span 9 | span 10
- `beatrick2.png` – span 7 | span 8 | span 9 | span 10
- `bender_logo.png` – span 7 | span 8 | spans 9,10
- `bitbox_logo.png` – span 7 | spans 8,9,10
- `flakes_logo.png` – span 7 | spans 8,9,10
- `fooBar_logo.png` – span 7 | span 8 | spans 9,10
- `gradientXFormTest2.png` – span 7 | span 8 | span 9 | span 10
- `gradientXFormTests.png` – span 7 | span 8 | span 9 | span 10
- `huge.png` – span 7 | span 8 | span 9 | span 10
- `imageTest1.png` – span 7 | span 8 | span 9 | span 10
- `maskedPatternFillTest.png` – spans 7,8 | spans 9,10
- `masktest.png` – span 7 | span 8 | span 9 | span 10
- `masktest2.png` – span 7 | spans 8,9 | span 10
- `patterntest.png` – span 7 | span 8 | span 9 | span 10
- `pong_logo.png` – span 7 | spans 8,9,10
- `reciter_logo.png` – span 7 | span 8 | spans 9,10
- `ringmod_logo.png` – span 7 | spans 8,9,10
- `test.png` – span 7 | span 8 | spans 9,10
- `test2.png` – span 7 | span 8 | spans 9,10
- `textTest1.png` – span 7 | span 8 | spans 9,10
- `trancelvania_logo.png` – span 7 | spans 8,9,10
- `transformTests1.png` – span 7 | spans 8,9,10
- `unicode.png` – span 7 | span 8 | spans 9,10

These results indicate that adjusting `NUXPIXELS_MAX_SPAN` can alter rendering for certain inputs while leaving others unaffected.
