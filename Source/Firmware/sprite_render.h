if ( updateMenu == 3 )
{
cartMenuCurrent = cartMenu;

// payload

static unsigned char bitmapPrev[ 64 * 64 ];
unsigned char bitmap[ 64 * 64 ];
memset( bitmap, 0, 64 * 64 );

#if 0

int image[ 192 * (168+2) ];
memset( image, 0, sizeof( int ) * 192 * 168 );

convertScreenToBitmap( framebuffer );

static int ctn = 0;
ctn ++;
int tick = c64CycleCount / 1000;
//#pragma omp parallel for schedule (static, 1)
for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192; i++ )
{
//int f = 127 - ( (int)( GetTickCount() * 0.02f + sinf( GetTickCount() * 0.001123f + sinf( i * 0.01332f ) ) * 20.0f  ) & 127 );
int f = 127-( (int)( tick * 0.02f + sinf( sinf( tick * 0.001123f ) + i * (sinf( tick * 0.001723f + ctn * 0.00622f ) + 1.0f ) * 0.01332f ) * 20.0f  ) & 127 );
//int f = ( (int)( 64+0*GetTickCount() * 0.02f + (sinf( ctn * 0.1322f ) + 1.0f ) * 20.0f  ) & 127 );
//int f = (int)( GetTickCount() * 0.02f ) & 127;
//pImage[ i + j * imgWidth ] = glm::vec4( i, j, i*j, 0.0f ) / 256.0f;
int x = i;
int y = j;
#if 1
f &= 7;
//int l = animation[ x + y * 192 + f * 192 * 168 ];
u64 o = (u64)&animation[ x + y * 192 + f * 192 * 168 ];
s32 l;
asm volatile( "LDNP X2, X1, [%0]" :: "r" (o) ); 
//asm volatile( "MOV %[l], X1" :  "=r" (l) ); 

asm("mov %[result], X1" : [result] "=r" (l));

//pImage[ i + j * imgWidth ] = glm::vec4( v );

if ( framebuffer[ x + 64 + 1 + ( y + 6*8 + 1 ) * 320 ] ) l = 0;
if ( framebuffer[ x + 64 + 1 + 1 + ( y + 6*8 + 1 ) * 320 ] ) l = 0;

//l = ( l * l ) / 256;
//if ( l > bluenoise[ (x&63) + (y&63) * 64 ] ) l = 255; else l = 0;
//image[ x + y * 192 ] = l>>7;
//if(0)
if ( !( i & 1 ) )
{
int v = ( l & 63 ) << 2;
l &= 192;
if ( v > bluenoise[ (x&63) + (y&63) * 64 ] ) l += 64;
if ( l > 192 ) l = 192;
//l = l & (255-1-2-4-8-16-32);
int xx = x, yy = y;
//if ( ctn & 1 ) xx += 1;
//if ( ctn & 1 ) yy += 1;
l >>= 6;
image[ xx + yy * 192 ] = l;
image[ xx+1 + yy * 192 ] = l;
}
#endif

#if 0
int l = rawData[ x * 2 + y * 192 + f * 192 * 168 ];
//l = l / 3 + 32;
int v = ( ( l << 3 ) + image[ x * 2 + y * 192 ] ) >> 3;

int r = v >> 6;
if ( r > 3 ) r = 3; if ( r < 0 ) r = 0;
int err = v - (r << 6);

image[ x * 2 + y * 192 ] = r * 64;
image[ x * 2 + 1 + y * 192 ] = r * 64;

image[ (x+1) * 2 + y * 192 ] += err;
image[ (x+2) * 2 + y * 192 ] += err;
image[ (x-1) * 2 + (y+1) * 192 ] += err;
image[ (x+0) * 2 + (y+1) * 192 ] += err;
image[ (x+1) * 2 + (y+1) * 192 ] += err;
image[ (x+0) * 2 + (y+2) * 192 ] += err;
#endif

}

if(0)
for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192; i+=8 )
{
int x = i;
int y = j;

int bx = x / 24, by = y / 21;
int byteX = ( x % 24 ) / 8;
int row = y % 21;
int ofs = by * 8 * 64 + bx * 64 + row * 3 + byteX;

int v = ( image[ x + 0 + y * 192 ] << 7 ) | 
( image[ x + 1 + y * 192 ] << 6 ) | 
( image[ x + 2 + y * 192 ] << 5 ) | 
( image[ x + 3 + y * 192 ] << 4 ) |
( image[ x + 4 + y * 192 ] << 3 ) | 
( image[ x + 5 + y * 192 ] << 2 ) | 
( image[ x + 6 + y * 192 ] << 1 ) | 
( image[ x + 7 + y * 192 ] );

bitmap[ ofs ] = v;
}

for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192/2; i+=4 )
{
int x = i;
int y = j;

int bx = x / 12, by = y / 21;
int byteX = ( x % 12 ) / 4;
int row = y % 21;
int ofs = by * 8 * 64 + bx * 64 + row * 3 + byteX;

int v = ( image[ (x + 0)*2 + y * 192 ] << 6 ) | 
( image[ (x + 1)*2 + y * 192 ] << 4 ) | 
( image[ (x + 2)*2 + y * 192 ] << 2 ) | 
( image[ (x + 3)*2 + y * 192 ] );

bitmap[ ofs ] = v;
}
#endif


#if 1
const int tm[4*4] = {
// original
0, 12, 3, 15,
8, 4, 11, 7,
2, 14, 1, 13,
10, 6, 9, 5 };
// line-like
/* 0,  4,  2,  6,
 8, 12, 10, 14,
 3,  7,  1,  5,
11, 15,  9, 13};*/


//for ( int i = 0; i < 100; i++ )
static int time = 0;
time += 1;

#ifdef MULTICOLOR
int image[ 96 * (168+2) ];
memset( image, 0, sizeof( int ) * 96 * 168 );

int lx = 50, ly = 50;
lx = cosf( time / 300.0f ) * 50.0f + 192/4;
ly = sinf( time / 300.0f ) * 50.0f + 168/2;

for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192/2; i++ )
{
int x = i;
int y = j;

int nx = rawSprite[ (x*2+1) + y * 192 ] - rawSprite[ x*2 + y * 192 ];
int ny = rawSprite[ x*2 + (y+1) * 192 ] - rawSprite[ x*2 + y * 192 ];

int dx = x - lx;
int dy = y - ly;

nx -= 2 * dx;
ny -= dy;
if ( nx < -127 ) nx = -127; if ( nx > 127 ) nx = 127;
if ( ny < -127 ) ny = -127; if ( ny > 127 ) ny = 127;

int l = ( 28768 - 3*( nx * nx + ny * ny) ) / 128;
//l += tm[ (x&3) + (y&3) * 4 ] * 4;
if ( l < 0 ) l = 0;


// Attkinson dithering
// v = pixel value + diffused error
// diffused error not divided upfront, could have accumulated before
int v = ( ( l << 3 ) + image[ x + y * 96 ] ) >> 3;
int r = v >> 6;
if ( r > 3 ) r = 3; if ( r < 0 ) r = 0;
int err = v - (r << 6);

image[ x + y * 96 ] = r;

/*image[ (x+1) + y * 96 ] += err;
image[ (x+2) + y * 96 ] += err;
image[ (x-1) + (y+1) * 96 ] += err;
image[ (x+0) + (y+1) * 96 ] += err;
image[ (x+1) + (y+1) * 96 ] += err;
image[ (x+0) + (y+2) * 96 ] += err;*/
}

for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192/2; i+=4 )
{
int x = i;
int y = j;

int bx = x / 12, by = y / 21;
int byteX = ( x % 12 ) / 4;
int row = y % 21;
int ofs = by * 8 * 64 + bx * 64 + row * 3 + byteX;

int v = ( image[ x + 0 + y * 96 ] << 6 ) | 
( image[ x + 1 + y * 96 ] << 4 ) | 
( image[ x + 2 + y * 96 ] << 2 ) | 
( image[ x + 3 + y * 96 ] );

bitmap[ ofs ] = v;
}

#endif


int image[ 192 * (168+2) ];
memset( image, 0, sizeof( int ) * 192 * 168 );

int lx = 50, ly = 50;
const int SHIFT = 256;
lx = ( cosf( time / 37.0f ) * 50.0f + sinf( time / 27.0f ) * 10.0f + 192.0f/2.0f ) * SHIFT;
ly = ( sinf( time / 30.0f ) * 50.0f + 168.0f/2.0f ) * SHIFT;

//lx = 192/2*SHIFT;
//ly = 168/2*SHIFT;

convertScreenToBitmap( framebuffer );

for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192; i++ )
{
int x = i;
int y = j;

int nx = rawSprite[ (x+1) + y * 192 +30*192] - rawSprite[ x + y * 192 +30*192 ];
int ny = rawSprite[ x + (y+1) * 192 +30*192 ] - rawSprite[ x + y * 192 +30*192 ];
//int nx = logoBump[ ((x+1) + y * 192 +30*192)*3+2] - logoBump[ (x + y * 192 +30*192)*3+2 ];
//int ny = logoBump[ (x + (y+1) * 192 +30*192)*3+2 ] - logoBump[ (x + y * 192 +30*192)*3+2 ];
//int nx = logoBump[ ( x + y * 192 ) * 3 + 0 ] - 128;
//int ny = logoBump[ ( x + y * 192 ) * 3 + 1 ] - 128;

//nx *= -64;
//ny *= -64;
nx *= SHIFT;
ny *= SHIFT;

int dx = x * SHIFT - lx;
int dy = y * SHIFT - ly;

nx -= dx;
ny -= dy;
//if ( nx < -127 ) nx = -127; if ( nx > 127 ) nx = 127;
//if ( ny < -127 ) ny = -127; if ( ny > 127 ) ny = 127;

//int l = ( 32768 - 3*( nx * nx + ny * ny) ) / 188;
int l = ( 182 * SHIFT - sqrtf( nx * nx + ny * ny) );

//l = (x * 255/192 + y * 255/168 )*SHIFT/2;

//l = i > 192/2 ? 255 : 0; //255 - i * 255 / 192;
/*l = 120 * SHIFT;
l -= ( tm[ (x&3) + (y&3) * 4 ] - 8 ) * 4 * SHIFT;
if ( l < 0 ) l = 0;
*/
l /= SHIFT;

//l = ( l * l ) >> 8;
//l = ( l * l ) >> 5;

l = ( l * l );

l *= rawSprite[ x + y * 192+30*192 ];
//l *= logoBump[ ( x + (y+30) * 192 ) * 3 + 2 ];
l >>= 15;


//if ( l > bluenoise[ (x&63) + (y&63) * 64 ] ) l = 255; else l = 0;
//l -= ( tm[ (x&3) + (y&3) * 4 ] - 8 ) * 4 * SHIFT;
//if ( l < 0 ) l = 0;


//l *= 255-framebuffer[ x + 64 + 1 + ( y + 6*8 + 1 ) * 320 ];
if ( framebuffer[ x + 64 + 1 + ( y + 6*8 + 1 ) * 320 ] ) l = 0;

//if ( ( y % 21 ) == 0 && ((((x & 16)>>4) ^ ( y & 1 ))&1) ) l = 255;

// Attkinson dithering
// v = pixel value + diffused error
// diffused error not divided upfront, could have accumulated before
int v = ( ( l << 3 ) + image[ x + y * 192 ] ) >> 3;

if ( framebuffer[ x + 64 + 1 + ( y + 6*8 + 1 ) * 320 ] ) v = 0;

//int r = v >> 7;
if ( v > 255 ) v = 255; 
//if ( r < 0 ) r = 0;
int err = v - (v & 128);

image[ x + y * 192 ] = v >> 7;

image[ (x+1) + y * 192 ] += err;
image[ (x+2) + y * 192 ] += err;
image[ (x-1) + (y+1) * 192 ] += err;
image[ (x+0) + (y+1) * 192 ] += err;
image[ (x+1) + (y+1) * 192 ] += err;
image[ (x+0) + (y+2) * 192 ] += err;

/*// Sierra dithering
// v = pixel value + diffused error
// diffused error not divided upfront, could have accumulated before
int v = ( ( l << 5 ) + image[ x + y * 192 ] ) >> 5;
int r = v >> 7;
if ( r > 1 ) r = 1; if ( r < 0 ) r = 0;
int err = v - (r << 7);

image[ x + y * 192 ] = r;

image[ (x+1) + y * 192 ] += err * 5;
image[ (x+2) + y * 192 ] += err * 3;
image[ (x-2) + (y+1) * 192 ] += err * 2;
image[ (x-1) + (y+1) * 192 ] += err * 4;
image[ (x+0) + (y+1) * 192 ] += err * 5;
image[ (x+1) + (y+1) * 192 ] += err * 4;
image[ (x+2) + (y+1) * 192 ] += err * 2;
image[ (x-1) + (y+2) * 192 ] += err * 2;
image[ (x+0) + (y+2) * 192 ] += err * 3;
image[ (x+1) + (y+2) * 192 ] += err * 2;
*/
}

for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192; i+=8 )
{
int x = i;
int y = j;

int bx = x / 24, by = y / 21;
int byteX = ( x % 24 ) / 8;
int row = y % 21;
int ofs = by * 8 * 64 + bx * 64 + row * 3 + byteX;

int v = ( image[ x + 0 + y * 192 ] << 7 ) | 
( image[ x + 1 + y * 192 ] << 6 ) | 
( image[ x + 2 + y * 192 ] << 5 ) | 
( image[ x + 3 + y * 192 ] << 4 ) |
( image[ x + 4 + y * 192 ] << 3 ) | 
( image[ x + 5 + y * 192 ] << 2 ) | 
( image[ x + 6 + y * 192 ] << 1 ) | 
( image[ x + 7 + y * 192 ] );

bitmap[ ofs ] = v;
}

/* multicolor conversion
for ( int j = 0; j < 168; j++ )
for ( int i = 0; i < 192/2; i+=4 )
{
int x = i;
int y = j;

int bx = x / 12, by = y / 21;
int byteX = ( x % 12 ) / 4;
int row = y % 21;
int ofs = by * 8 * 64 + bx * 64 + row * 3 + byteX;

int v = ( image[ (x + 0)*2 + y * 192 ] << 6 ) | 
( image[ (x + 1)*2 + y * 192 ] << 4 ) | 
( image[ (x + 2)*2 + y * 192 ] << 2 ) | 
( image[ (x + 3)*2 + y * 192 ] );

bitmap[ ofs ] = v;
}*/


#endif


#if 0
memcpy( &cartMenuCurrent[ 0x3000 ], bitmap, 4096 );

// copies 4k from $b000 to $2000
const unsigned char copyCodeSprite[] = 
{
0xa2, 0x00, 0xbd, 0x00, 0xb0, 0x9d, 0x00, 0x20, 0xbd, 0x00, 0xb1, 0x9d, 0x00, 0x21, 0xbd, 0x00, 
0xb2, 0x9d, 0x00, 0x22, 0xbd, 0x00, 0xb3, 0x9d, 0x00, 0x23, 0xbd, 0x00, 0xb4, 0x9d, 0x00, 0x24, 
0xbd, 0x00, 0xb5, 0x9d, 0x00, 0x25, 0xbd, 0x00, 0xb6, 0x9d, 0x00, 0x26, 0xbd, 0x00, 0xb7, 0x9d, 
0x00, 0x27, 0xbd, 0x00, 0xb8, 0x9d, 0x00, 0x28, 0xbd, 0x00, 0xb9, 0x9d, 0x00, 0x29, 0xbd, 0x00, 
0xba, 0x9d, 0x00, 0x2a, 0xbd, 0x00, 0xbb, 0x9d, 0x00, 0x2b, 0xbd, 0x00, 0xbc, 0x9d, 0x00, 0x2c, 
0xbd, 0x00, 0xbd, 0x9d, 0x00, 0x2d, 0xbd, 0x00, 0xbe, 0x9d, 0x00, 0x2e, 0xbd, 0x00, 0xbf, 0x9d, 
0x00, 0x2f, 0xca, 0xd0, 0x9d, 0x60
};
memcpy( &cartMenuCurrent[ 0x2000 ], copyCodeSprite, 102 );
#endif

#if 1
if ( firstSprites ) 
{
firstSprites = 0;
for ( int i = 0; i < 64 * 64; i++ )
bitmapPrev[ i ] = ~bitmap[ i ];
}


// generate copy speed code to transfer sprite data
int curValA = 0xffff;
int curAddr = 0x2000, curOfs = 0, ofsIncr = 1;

debugAccesses2 = 0;

static int lastCurOfs = 0;


cartMenuCurrent[ curAddr ++ ] = 0xA2; // LDX
cartMenuCurrent[ curAddr ++ ] = 0;
cartMenuCurrent[ curAddr ++ ] = 0xA0; // LDY
cartMenuCurrent[ curAddr ++ ] = 255;
debugAccesses2 += 4;

/*static int frameAlt = 0;

frameAlt = 1 - frameAlt;
if ( frameAlt == 0 )
{
curOfs = 0; ofsIncr = 1;
} else
{
curOfs = 4095; ofsIncr = -1;
}*/

curOfs = lastCurOfs; ofsIncr = 1;

int nBytesToTransfer = 7*8*64;

while ( nBytesToTransfer > 0 ) //curOfs >= 0 && curOfs < 4096 )
{
nBytesToTransfer --;

int byteA = bitmap[ curOfs ];

if ( byteA != bitmapPrev[ curOfs ] )
{
bitmapPrev[ curOfs ] = byteA;

if ( byteA == 0 )
{
cartMenuCurrent[ curAddr ++ ] = 0x8E; // STX
debugAccesses2 += 1;
} else
if ( byteA == 255 )
{
cartMenuCurrent[ curAddr ++ ] = 0x8C; // STY
debugAccesses2 += 1;
} else
{
if ( byteA != curValA )
{
curValA = byteA;
cartMenuCurrent[ curAddr ++ ] = 0xA9; // LDA
cartMenuCurrent[ curAddr ++ ] = byteA;
debugAccesses2 += 2;
} 
cartMenuCurrent[ curAddr ++ ] = 0x8D; // STA
debugAccesses2 += 1;
}
cartMenuCurrent[ curAddr ++ ] = (0x2000+curOfs)&255;
cartMenuCurrent[ curAddr ++ ] = ((0x2000+curOfs)>>8)&255;
debugAccesses2 += 2;
}

if ( curAddr >= 0x3000 - 10 )
{
goto cantCopyEverythingThisTime;
}

curOfs += ofsIncr;
/*if ( curOfs >= 56 * 64 )
{
goto copySpriteDone;
}*/
curOfs %= 56*64;
}

cantCopyEverythingThisTime:
lastCurOfs = curOfs;
cartMenu[ curAddr ++ ] = 0x60; // RTS
debugAccesses2 += 1;
#endif

sk64Command = 0xfe;

//warmCache( pFIQ );
warmCacheOnlyPL( pFIQ );
updateMenu = 0;
delayUpdateMenu = 0;
doneWithHandling = 1;
}

