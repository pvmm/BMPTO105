```
               ======================================
               105 color image converter for MSX v1.0
               ======================================

                     (c) 2026 Pedro de Medeiros


Introduction
------------

Based on the previous work of Daniel Vik on the 105 color image
converter, this package contains an encoder to convert several bitmaps
formats to 105 color interlaced screen2 images. The package also contains
an assembly listing to view the images on the MSX (WIP).

The encoder is pretty straight forward. Just invoke:

tests/bmpto105_test.exe image.png

and two new files are created. The first is the "image.si2" that is used
when compiling the viewer application for the MSX. The second is a
"image.105.png" for reference.

The source bitmap file is recommended to be 256x192 pixels. The encoder
is however able to encode images of other sizes as well as long as the
width and height is a multiple of 8 pixels.
```

⚠️ **IMPORTANT** ⚠️

* **i105view.asm** requires [tniASM 0.45](http://www.tni.nl/products/tniasm.html) to compile.
