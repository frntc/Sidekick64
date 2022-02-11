/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 tft_sid_vis.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - this is some visuals code for the SID/FM/etc emulation together with ST7789 displays
 Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

 Logo created with http://patorjk.com/software/taag/
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
				{
					if ( visUpdate && visMode == 0 && bufferIsFreeI2C() > 1024 ) // oscilloscope
					{
						if ( scopeX == 0 )
							tftClearDirty();
					
						s32 y = 160 + min( 40, max( -40, ( left + right ) / 1 / 192 ) );
						y = max( 10, min( 229, y ) );

						if ( y != scopeValues[scopeX] )
						{
							extern unsigned char tftFrameBuffer12Bit[ 240 * 240 * 3 / 2 ];
							setDoubleWPixel12( scopeValues[scopeX], scopeX*2, *(u32*)&tftFrameBuffer12Bit[ scopeX * 2 * 3 / 2 + scopeValues[scopeX] * 240 * 3 / 2 ] );
							setDoubleWPixel12( y, scopeX*2, 0xffffffff );

							scopeValues[scopeX] = y;
						}

						scopeX++;
						if ( scopeX >= 118 )
						{
							//visUpdate = 0;
							scopeX = 0;
						}

						if ( visModeGotoNext )
						{
							visUpdate = 1;
							visModeGotoNext = 0;
							visMode ++;
						}
					} else
					if ( visMode == 1 ) // transition oscilloscope -> vu meter
					{
						if ( visUpdate )
						{
							tftClearDirty();
							for ( u32 j = 104; j < 224; j++ )
								for ( u32 i = 0; i < 240; i++ )
									setPixelDirty( j, i, *(u16*)&tftBackground[ (i + j * 240) * 2 ] );
						}

						visUpdate = 0;

						if ( bufferEmptyI2C() && !tftIsDirtyRegion() )
						{
							visUpdate = 1;
							visModeGotoNext = 0;
							visMode ++;
						}

						if ( bufferEmptyI2C() )
							tftUpdateNextDirtyRegions();
					} else
					if ( visMode == 2 ) // vu meter
					{
						if ( visUpdate )
						{
							visUpdate = 0;
							tftClearDirty();

							// remove old needle
							for ( u32 i = startRow; i < endRow; i++ )
							{
								s32 x = (s32)( px + i * dx );
								setPixelDirty( 220 - i, x, *(u16*)&tftBackground[ (x + (220-i) * 240) * 2 ] );
							}

							// moving average
							float vuValue = min( 1.98f, 1.98f * (float)vuMeter[ 0 ] / 1024.0f * 1.5f * scaleVis );

							const float movAvg = 0.8f;
							vuValueAvg = vuValueAvg * movAvg + vuValue * ( 1.0f - movAvg );
							dx = -1.0f + vuValueAvg + 0.01f;

							startRow =  40.0f * cosf( atanf(dx) );
							endRow   = 100.0f * cosf( atanf(dx) );

							for ( u32 i = startRow; i < endRow; i++ )
							{
								s32 x = (s32)( px + i * dx );
								setPixelDirty( 220 - i, x, 0 );
							}
						} else
						{
							if ( bufferEmptyI2C() )
							{
								if ( tftUpdateNextDirtyRegions() == 0 )
								{
									visUpdate = 1;
									
									if ( visModeGotoNext )
									{
										visModeGotoNext = 0;
										visMode ++;
									}
								}
							}
						}
					} else
					if ( visMode == 3 ) // transition vu meter -> level meter
					{
						if ( visUpdate )
						{
							tftClearDirty();
							for ( u32 j = 104; j < 114; j++ )
								for ( u32 i = 0; i < 240; i++ )
									setPixelDirty( j, i, *(u16*)&tftBackground2[ (i + j * 240) * 2 ] );
							for ( u32 j = 216; j < 224; j++ )
								for ( u32 i = 0; i < 240; i++ )
									setPixelDirty( j, i, *(u16*)&tftBackground2[ (i + j * 240) * 2 ] );
							for ( u32 j = 114; j < 216; j++ )
								for ( u32 i = 0; i < 240; i++ )
									setPixelDirty( j, i, 0 );

							for ( u32 l = 0; l < nLevelMeters; l++ )
							{
								const u32 xpos = 16;
								u32 ypos = 134 + l * 20 + (3-nLevelMeters)*10;

								for ( u32 j = 0; j < 16; j++ )
									for ( u32 i = 0; i < 9 * 23; i++ )
										setPixelDirty( ypos + j, xpos + i, *(u16*)&tftLEDs[ (i + (j+16) * 240) * 2 ] );
							}
						}

						visUpdate = 0;

						if ( bufferEmptyI2C() && !tftIsDirtyRegion() )
						{
							visUpdate = 1;
							visModeGotoNext = 0;
							visMode ++;
	
							px = 120.0f;
							dx = 0.0f;
							vuValueAvg = 0.01f;

							startRow = endRow = 1;
						}

						if ( bufferEmptyI2C() )
							tftUpdateNextDirtyRegions();
					} else
					if ( visMode == 4 ) // level meter
					{
						if ( visUpdate )
						{
							visUpdate = 0;
							tftClearDirty();

							const float movAvg = 0.8f;


							for ( u32 l = 0; l < nLevelMeters; l++ )
							{
								const u32 xpos = 16;
								u32 ypos = 134 + l * 20 + (3-nLevelMeters)*10;

								// moving average
								float v = min( 1.0f, (float)vuMeter[ 1+l ] / 1024.0f );
								ledValueAvg[ l ] = ledValueAvg[ l ] * movAvg + v * ( 1.0f - movAvg );

								u32 nLEDs = max( 0, min( 9, (ledValueAvg[ l ] * 9.5f * scaleVis) ) );

								if ( nLEDs > nPrevLEDs[ l ] ) // render more bright LEDs
								{
									for ( u32 j = 0; j < 16; j++ )
										for ( u32 i = nPrevLEDs[ l ] * 23; i < nLEDs * 23; i++ )
											setPixelDirty( ypos + j, xpos + i, *(u16*)&tftLEDs[ (i + j * 240) * 2 ] );
								} else
								if ( nLEDs < nPrevLEDs[ l ] && nPrevLEDs[ l ] > 0 ) // render some dark LEDs
								{
									for ( u32 j = 0; j < 16; j++ )
										for ( u32 i = nLEDs * 23; i < nPrevLEDs[ l ] * 23; i++ )
											setPixelDirty( ypos + j, xpos + i, *(u16*)&tftLEDs[ (i + (j+16) * 240) * 2 ] );
								}

								nPrevLEDs[ l ] = nLEDs;
							}
						} else
						{
							if ( bufferEmptyI2C() )
							{
								if ( tftUpdateNextDirtyRegions() == 0 )
								{
									visUpdate = 1;
									
									if ( visModeGotoNext )
									{
										visModeGotoNext = 0;
										visMode ++;
									}
								}
							}
						}
					} else
					if ( visMode == 5 ) // transition vu meter -> level meter
					{
						if ( visUpdate )
						{
							tftClearDirty();
							for ( u32 j = 104; j < 224; j++ )
								for ( u32 i = 0; i < 240; i++ )
									setPixelDirty( j, i, *(u16*)&tftBackground2[ (i + j * 240) * 2 ] );
						}

						visUpdate = 0;

						if ( bufferEmptyI2C() && !tftIsDirtyRegion() )
						{
							visUpdate = 1;
							visModeGotoNext = 0;
							visMode = 0;
	
							px = 120.0f;
							dx = 0.0f;
							vuValueAvg = 0.01f;
							nPrevLEDs[ 0 ] = nPrevLEDs[ 1 ] = nPrevLEDs[ 2 ] = 0;

							startRow = endRow = 1;
						}

						if ( bufferEmptyI2C() )
							tftUpdateNextDirtyRegions();
					} 
				}

				vu_nLEDs = 0xffff; 