/*****************************************************************************
**
** Copyright (C) 2006 Daniel Vik
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
******************************************************************************
*/
#include <fstream>
#include <string.h>
#include <stdio.h>

typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned long  UInt32;

#define MIN( x, y ) ( (x) < (y) ? (x) : (y) )
#define MAX( x, y ) ( (x) > (y) ? (x) : (y) )

union RgbColor
{
    struct 
    {
        UInt8 unused;
        UInt8 r;
        UInt8 g;
        UInt8 b;
    } rgb;
    UInt32 raw;
};

struct RgbBitmap
{
    UInt32 width;
    UInt32 height;
    RgbColor bitmap[1];
};

struct Msx105Bitmap
{
    UInt32 width;
    UInt32 height;
    struct
    {
        UInt8 c0;
        UInt8 p0;
        UInt8 c1;
        UInt8 p1;
    } bitmap[1];
};


struct BmpHeader {
	UInt32 size;
    UInt32 reserved1;
    UInt32 offBits;
    UInt32 bmHeaderSize;
    UInt32 width;
    UInt32 height;
	UInt16 planes;
    UInt16 bitCount;
	UInt32 compression;
    UInt32 sizeImage;
    UInt32 xPelsPerMeter;
    UInt32 yPelsPerMeter;
    UInt32 clrUsed;
    UInt32 clrImportant;
};

RgbBitmap* loadBitmap( const char* fileName )
{
    FILE* f;

    f = fopen( fileName, "rb" );

    if ( f == NULL ) 
    {
        printf( "   ERROR: Faliled opening file \"%s\"\n" ,fileName );
        return NULL;
    }

    char fileType[2];
    if ( fread( fileType, 1, 2, f ) != 2 ) {
        printf( "   ERROR: Failed reading bitmap header.\n" );
        fclose( f );
        return NULL;
    }

    if ( memcmp( fileType, "BM", 2 ) != 0 ) {
        printf( "   ERROR: Not a valid bitmap file.\n" );
        fclose( f );
        return NULL;
    }

    BmpHeader bmpHeader;
    if ( fread( &bmpHeader, 1, sizeof(bmpHeader), f ) != sizeof(bmpHeader) ) {
        printf( "   ERROR: Failed reading bitmap header.\n" );
        fclose( f );
        return NULL;
    }

    if ( bmpHeader.planes != 1 || 
         bmpHeader.compression != 0 ||
         bmpHeader.bitCount != 24 ) 
    {
        printf( "   ERROR: Unsuported bitmap format.\n" );
        fclose( f );
        return NULL;
    }

    RgbBitmap* bm = (RgbBitmap*) calloc( 1, sizeof(RgbBitmap) + bmpHeader.width * bmpHeader.height * sizeof(RgbColor) );
    if ( bm == NULL ) 
    {
        printf( "   ERROR: Failed allocating memory for bitmap.\n" );
        fclose( f );
        return NULL;
    }

    bm->width = bmpHeader.width;
    bm->height = bmpHeader.height;

    for ( UInt32 y = 0; y < bm->height; y++ ) 
    {
        for (UInt32 x = 0; x < bm->width; x++ )
        {
            UInt8 color[3];
            if ( fread( color, 1, 3, f ) != 3 ) 
            {
                free( bm );
                printf( "   ERROR: Failed reading bitmap data.\n" );
                fclose( f );
                return NULL;
            }
            UInt32 i = (bm->height - y - 1) * bm->width + x;
            bm->bitmap[i].rgb.b = color[0];
            bm->bitmap[i].rgb.g = color[1];
            bm->bitmap[i].rgb.r = color[2];
        }
    }

    fclose( f );

    return bm;
}


const UInt32 defaultPalette[16] =
{
    0x000000, 0x000000, 0x24da24, 0x68ff68, 0x2424ff, 0x4868ff, 0xb62424, 0x48daff,
    0xff2424, 0xff6868, 0xdada24, 0xdada91, 0x249124, 0xda48b6, 0xb6b6b6, 0xffffff
};

RgbColor msxPalette[16];

void initPalette()
{
    for ( int i = 0; i < 16; i++ ) 
    {
        msxPalette[i].rgb.unused = 0;
        msxPalette[i].rgb.r = (UInt8) ( (defaultPalette[i] >> 16) & 0xff );
        msxPalette[i].rgb.g = (UInt8) ( (defaultPalette[i] >>  8) & 0xff );
        msxPalette[i].rgb.b = (UInt8) ( (defaultPalette[i] >>  0) & 0xff );
    }
}

struct Color105
{
    struct {
        UInt8 c0;
        UInt8 c1;
    } msx;
    RgbColor rgb;
};

UInt32   msxColorTableSize = 0;
Color105 msxColorTable[8192][4];

void createColorTable()
{
    initPalette();

    for ( int bg0 = 1; bg0 < 16; bg0++ ) 
    {
        for ( int fg0 = bg0 + 1; fg0 < 16; fg0++ )
        {
            for ( int bg1 = bg0; bg1 < 16; bg1++ )
            {
                for ( int fg1 = bg1 + 1; fg1 < 16; fg1++ )
                {
                    UInt32 i;
                    for ( i = 0; i < msxColorTableSize; i++ ) 
                    {
                        if ( msxColorTable[i][0].msx.c0 == bg0 && msxColorTable[i][0].msx.c1 == bg1 &&
                             msxColorTable[i][1].msx.c0 == fg0 && msxColorTable[i][1].msx.c1 == bg1 &&
                             msxColorTable[i][2].msx.c0 == bg0 && msxColorTable[i][2].msx.c1 == fg1 &&
                             msxColorTable[i][3].msx.c0 == fg0 && msxColorTable[i][3].msx.c1 == fg1 )
                        {
                            break;
                        }
                    }
                    if ( i == msxColorTableSize )
                    {
                        msxColorTable[i][0].msx.c0 = bg0;
                        msxColorTable[i][0].msx.c1 = bg1;
                        msxColorTable[i][0].rgb.rgb.r  = ( (int)msxPalette[bg0].rgb.r + msxPalette[bg1].rgb.r ) / 2;
                        msxColorTable[i][0].rgb.rgb.g  = ( (int)msxPalette[bg0].rgb.g + msxPalette[bg1].rgb.g ) / 2;
                        msxColorTable[i][0].rgb.rgb.b  = ( (int)msxPalette[bg0].rgb.b + msxPalette[bg1].rgb.b ) / 2;
                        
                        msxColorTable[i][1].msx.c0 = fg0;
                        msxColorTable[i][1].msx.c1 = bg1;
                        msxColorTable[i][1].rgb.rgb.r  = ( (int)msxPalette[fg0].rgb.r + msxPalette[bg1].rgb.r ) / 2;
                        msxColorTable[i][1].rgb.rgb.g  = ( (int)msxPalette[fg0].rgb.g + msxPalette[bg1].rgb.g ) / 2;
                        msxColorTable[i][1].rgb.rgb.b  = ( (int)msxPalette[fg0].rgb.b + msxPalette[bg1].rgb.b ) / 2;
                        
                        msxColorTable[i][2].msx.c0 = bg0;
                        msxColorTable[i][2].msx.c1 = fg1;
                        msxColorTable[i][2].rgb.rgb.r  = ( (int)msxPalette[bg0].rgb.r + msxPalette[fg1].rgb.r ) / 2;
                        msxColorTable[i][2].rgb.rgb.g  = ( (int)msxPalette[bg0].rgb.g + msxPalette[fg1].rgb.g ) / 2;
                        msxColorTable[i][2].rgb.rgb.b  = ( (int)msxPalette[bg0].rgb.b + msxPalette[fg1].rgb.b ) / 2;
                       
                        msxColorTable[i][3].msx.c0 = fg0;
                        msxColorTable[i][3].msx.c1 = fg1;
                        msxColorTable[i][3].rgb.rgb.r  = ( (int)msxPalette[fg0].rgb.r + msxPalette[fg1].rgb.r ) / 2;
                        msxColorTable[i][3].rgb.rgb.g  = ( (int)msxPalette[fg0].rgb.g + msxPalette[fg1].rgb.g ) / 2;
                        msxColorTable[i][3].rgb.rgb.b  = ( (int)msxPalette[fg0].rgb.b + msxPalette[fg1].rgb.b ) / 2;

                        msxColorTableSize++;
                    }
                }
            }
        }
    }
}

#define ColorMse(c1, c2) (                                                  \
    ( (int)c1.rgb.r - (int)c2.rgb.r ) * ( (int)c1.rgb.r - (int)c2.rgb.r ) + \
    ( (int)c1.rgb.g - (int)c2.rgb.g ) * ( (int)c1.rgb.g - (int)c2.rgb.g ) + \
    ( (int)c1.rgb.b - (int)c2.rgb.b ) * ( (int)c1.rgb.b - (int)c2.rgb.b )   \
)

UInt32 findBestMatch( RgbColor* source )
{
    UInt32 bestIndex = 0;
    UInt32 bestMse   = 0xffffffff;

    for ( UInt32 i = 0; i < msxColorTableSize; i++ ) 
    {
        UInt32 mse = 0;

        RgbColor c0 = msxColorTable[i][0].rgb;
        RgbColor c1 = msxColorTable[i][1].rgb;
        RgbColor c2 = msxColorTable[i][2].rgb;
        RgbColor c3 = msxColorTable[i][3].rgb;

        for ( UInt32 j = 0; j < 8; j++ )
        {
            UInt32 t0 = ColorMse( c0, source[j] );
            UInt32 t1 = ColorMse( c1, source[j] );
            UInt32 t2 = ColorMse( c2, source[j] );
            UInt32 t3 = ColorMse( c3, source[j] );

            UInt32 m0 = MIN( t0, t1 );
            UInt32 m1 = MIN( t2, t3 );
            mse += MIN( m0, m1 );
        }

        if (mse < bestMse ) 
        {
            bestMse = mse;
            bestIndex = i;
        }
    }

    return bestIndex;
}

Msx105Bitmap* convertBitmap( RgbBitmap* rgbBm )
{
    Msx105Bitmap* msxBm = (Msx105Bitmap*) calloc( 1, sizeof(Msx105Bitmap) + 4 * (rgbBm->width / 8) * rgbBm->height );
    msxBm->width  = rgbBm->width / 8;
    msxBm->height = rgbBm->height / 8;

    for ( UInt32 h = 0; h < 8 * msxBm->height; h++ )
    {
        printf( "\rConverting line %d/%d", h + 1, 8 * msxBm->height );

        for ( UInt32 w = 0; w < msxBm->width; w++ )
        {
            RgbColor* rgbBitmap = rgbBm->bitmap + h * rgbBm->width + w * 8;

            UInt32 idx = findBestMatch( rgbBitmap );

            RgbColor c0 = msxColorTable[idx][0].rgb;
            RgbColor c1 = msxColorTable[idx][1].rgb;
            RgbColor c2 = msxColorTable[idx][2].rgb;
            RgbColor c3 = msxColorTable[idx][3].rgb;

            UInt8 pattern0 = 0;
            UInt8 pattern1 = 0;

            for ( UInt32 j = 0; j < 8; j++ )
            {
                UInt32 t[4] = {
                    ColorMse( c0, rgbBitmap[j] ),
                    ColorMse( c1, rgbBitmap[j] ),
                    ColorMse( c2, rgbBitmap[j] ),
                    ColorMse( c3, rgbBitmap[j] )
                };

                UInt32 i = 0;
                i = t[i] < t[1] ? i : 1;
                i = t[i] < t[2] ? i : 2;
                i = t[i] < t[3] ? i : 3;

                pattern0 <<= 1;
                pattern1 <<= 1;

                switch (i) 
                {
                case 1:
                    pattern0 |= 1;
                    break;
                case 2:
                    pattern1 |= 1;
                    break;
                case 3:
                    pattern0 |= 1;
                    pattern1 |= 1;
                    break;
                }
            }

            msxBm->bitmap[h * msxBm->width + w].c0 = msxColorTable[idx][0].msx.c0 + 16 * msxColorTable[idx][1].msx.c0;
            msxBm->bitmap[h * msxBm->width + w].p0 = pattern0;
            msxBm->bitmap[h * msxBm->width + w].c1 = msxColorTable[idx][0].msx.c1 + 16 * msxColorTable[idx][2].msx.c1;
            msxBm->bitmap[h * msxBm->width + w].p1 = pattern1;
        }
    }

    printf( "\n" );

    return msxBm;
}

void saveBitmap( const char* fileName, Msx105Bitmap* msxBm )
{
    FILE* f = fopen( fileName, "wb" );
    if ( f == NULL )
    {
        printf( "   ERROR: Faliled opening file \"%s\"\n" ,fileName );
        return;
    }

    printf( "Saving image: \"%s\" \n", fileName );

    UInt8 header[2] = { (UInt8) msxBm->width, (UInt8) msxBm->height } ;

    // Save header
    fwrite( header, 1, 2, f );

    // Save pattern for even image
    for ( UInt32 h = 0; h < msxBm->height; h++ ) 
    {
        for ( UInt32 w = 0; w < msxBm->width; w++ )
        {
            for ( UInt32 t = 0; t < 8; t++ )
            {
                fwrite( &msxBm->bitmap[(h * 8 + t) * msxBm->width + w].p0, 1, 1, f );
            }
        }
    }

    // Save colors for even image
    for ( UInt32 h = 0; h < msxBm->height; h++ ) 
    {
        for ( UInt32 w = 0; w < msxBm->width; w++ )
        {
            for ( UInt32 t = 0; t < 8; t++ )
            {
                fwrite( &msxBm->bitmap[(h * 8 + t) * msxBm->width + w].c0, 1, 1, f );
            }
        }
    }

    // Save pattern for odd image
    for ( UInt32 h = 0; h < msxBm->height; h++ ) 
    {
        for ( UInt32 w = 0; w < msxBm->width; w++ )
        {
            for ( UInt32 t = 0; t < 8; t++ )
            {
                fwrite( &msxBm->bitmap[(h * 8 + t) * msxBm->width + w].p1, 1, 1, f );
            }
        }
    }

    // Save colors for odd image
    for ( UInt32 h = 0; h < msxBm->height; h++ ) 
    {
        for ( UInt32 w = 0; w < msxBm->width; w++ )
        {
            for ( UInt32 t = 0; t < 8; t++ )
            {
                fwrite( &msxBm->bitmap[(h * 8 + t) * msxBm->width + w].c1, 1, 1, f );
            }
        }
    }

    fclose( f );
}

void main( int argc, char* argv[] ) 
{
    if ( argc != 2 )
    {
        printf( "Usage:    bmpto105 <filename.bmp>\n\n" );
        return;
    }

    char* bmpFilename = argv[1];

    createColorTable();

    RgbBitmap* rgbBm = loadBitmap( bmpFilename );
    if ( rgbBm == NULL )
    {
        return;
    }

    Msx105Bitmap* msxBm = convertBitmap( rgbBm );

    // Create filename for destination file (change extension to .si2)
    char msxFilename[512];
    strcpy( msxFilename, bmpFilename );

    char* ptr = msxFilename + strlen( msxFilename ) - 1;

    while ( *ptr != '.' )
    {
        ptr--;
    }
    *ptr = 0;

    strcat( msxFilename, ".si2" );

    saveBitmap( msxFilename, msxBm );
}
