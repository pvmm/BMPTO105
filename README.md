```
               =====================================
               105 color image viewer for MSX   v1.0
               =====================================
               
                       (c) 2006 Daniel Vik
                       
                       
Introduction
------------

This package contains an encoder to convert 24bit windows bitmaps (.bmp)
to 105 color interlaced screen2 images. The package also contains an
assembly listing to view the images on the MSX. 

The encoder is pretty straight forward. Just invoke:

bmpto105.exe image.bmp

and a file called image.si2 is created. The si2 file is then used when
compiling the viewer application for the MSX.

The source bitmap file is recommended to be 128x192 pixels. The encoder
is however able to encode imgages of other sizes as well as long as
the width and height is a multiple of 8 bytes.
```
